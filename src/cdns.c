// https://datatracker.ietf.org/doc/html/rfc1034
// https://datatracker.ietf.org/doc/html/rfc1035
// https://www.cloudflare.com/learning/dns/dns-records/

#include "cdns.h"
#include <bits/sockaddr.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/un.h>
#include <fcntl.h>

#define CDNS_ERR_UNDEFINED -1
#define CDNS_NUM_ERR 12

typedef struct ReusableDataCollection {
    int dataSize;
    int numDataPieces;
    void *allocation;
    size_t *unused;
    size_t unusedStartIndex;
    size_t unusedEndIndex;
} ReusableDataCollection;

static int createCollection(ReusableDataCollection *collection, int dataSize, size_t numDataPieces) {
    collection->dataSize = dataSize;
    collection->numDataPieces = numDataPieces;
    collection->allocation = malloc((size_t)dataSize * numDataPieces);
    collection->unused = malloc(sizeof(size_t) * numDataPieces);
    if(collection->allocation == NULL || collection->unused == NULL) {
        return CDNS_ERR_MEM;
    }
    collection->unusedStartIndex = 0;
    collection->unusedEndIndex = 0;
    for(size_t i = 0;i < numDataPieces;i++) {
        collection->unused[i] = i;
    }

    return 0;
}
static int resizeCollection(ReusableDataCollection *collection, size_t newNumDataPieces) {
    collection->allocation = realloc(collection->allocation, newNumDataPieces * collection->dataSize);
    collection->unused = realloc(collection->unused, sizeof(size_t) * newNumDataPieces);
    if(collection->allocation == 0 || collection->unused == 0) {
        return CDNS_ERR_MEM;
    }
    if(collection->unusedStartIndex >= collection->unusedEndIndex) {
        for(size_t i = collection->numDataPieces;i < newNumDataPieces;i++) {
            collection->unused[i] = i;
        }
    } else {
    }
    return 0;
}
// Double check this, wrote while tired
static size_t popNextIndexCollection(ReusableDataCollection *collection) {
    size_t idx = collection->unused[collection->unusedStartIndex];
    collection->unusedStartIndex = (collection->unusedStartIndex + 1) % collection->numDataPieces;
    return idx;
}
static void* getPtrCollection(ReusableDataCollection *collection, size_t idx) {
    return (void*)(((char*)collection->allocation) + idx * collection->dataSize);
}
static void returnSpotCollection(ReusableDataCollection *collection, size_t idx) {
    collection->unused[collection->unusedEndIndex] = idx;
    collection->unusedEndIndex = (collection->unusedEndIndex + 1) % collection->dataSize;
}
static void destroyCollection(ReusableDataCollection *collection) {
    free(collection->allocation);
    free(collection->unused);
}

/// Followed by data in memory
typedef struct ResponseCycleData {
    CdnsCallbackCycleInfo info;
} ResponseCycleData;

typedef struct DnsConnections {
    pthread_t* threads;
} DnsConnections;
static void destroyDnsConnections(DnsConnections* connections) {
    if(connections->threads != 0) {
        free(connections->threads);
    }
}
typedef struct DnsLitener {
    int socket;
    CdnsListenerConfig config;
    bool isOpen;
} DnsListener;
typedef struct DnsState {
    int numListeners;
    DnsListener* listeners;

    int threadRequests;
    int threadOutgoingRequests;
    int numThreads;
    int maxThreads;
    int resendDelay;
    int maxResendCount;

    int requestMakers[6];

    CdnsCallbackDescriptor callback;
    bool listening;
    bool paused;
    DnsConnections connections;
    ReusableDataCollection resDataCollection;
    ReusableDataCollection reqDataCollection;
} DnsState;

typedef struct OutgoingRequestTrackingData {
    CdnsRequestId id;
} OutgoingRequestTrackingData;

typedef struct ResponseContext {
    DnsState* dns;
    int index;
} ResponseContext;

typedef struct ResponseWriteinfo {

} ResponseWriteInfo;

typedef struct RequestWriteInfo {

} RequestWriteInfo;

char *cdnsGetErrorString(int error) {
    char* strings[CDNS_NUM_ERR + 1] = {
        "NONE",
        "MEMORY ERROR",
        "TCP UNSUPPORTED",
        "THREADS UNSUPPORTED",
        "HTTP UNSUPPORTED",
        "SOCKET CLOSE ERROR",
        "NO CALLBACK SPECIFIED",
        "ALREADY LISTENING",
        "INVALID CALLBACK",
        "INVALID PAUSE",
        "ERROR IN RESPONSE FROM EXTERNAL SERVER",
        "STATE MDOFIIED WHILE UNPAUSED",
        "NONBLOCKING SOCKETS UNSUPPORTED"
    };
    if(error <= CDNS_NUM_ERR && error >= 0) {
        return strings[error];
    } else {
        return "UNKNOWN ERROR";
    }
}

#define DEFAULT_THREAD_REQUESTS 256
#define DEFAULT_RESEND_DELAY 1000
#define DEFAULT_RESEND_ATTEMPTS 10

inline int getProtoTypeIndex(CdnsNetworkProtocolType net, CdnsProtocolType typ) {
    return (int)net * 3 + (int)typ;
}
static int makeListener(const CdnsListenerConfig* config, DnsListener* out) {
    // Socket creation timeline:
    // Create socket, with protocol type(socket)
    // Set socket options(setsockopt, fcntl)
    // Bind socket()
    // Destroy socket()
    int sock;
    int domain, type, protocol;
    int port;
    if(config->port == 0) {
        if(config->proto == CdnsProtoUdp) {
            port = htons(CDNS_DNS_UDP_PORT);
        } else if(config->proto == CdnsProtoTcp) {
            port = htons(CDNS_DNS_TCP_PORT);
        } else if(config->proto == CdnsProtoHttp) {
            port = htons(CDNS_DNS_HTTP_PORT);
        }
        // No need for error check, that will happen later
    } else {
        port = htons(config->port);
    }
    if(config->netProto == CdnsNetProtoInet4) {
        domain = AF_INET;
    } else if(config->netProto == CdnsNetProtoInet6) {
        domain = AF_INET6;
    } else {
        return CDNS_ERR_UNDEFINED;
    }
    if(config->proto == CdnsProtoTcp) {
        type = SOCK_STREAM;
        protocol = 0;
        return CDNS_ERR_TCP;
    } else if(config->proto == CdnsProtoHttp) {
        type = SOCK_STREAM;
        protocol = 0;
        return CDNS_ERR_HTTP;
    } else if(config->proto == CdnsProtoUdp) {
        type = SOCK_DGRAM;
        protocol = 0;
        sock = socket(domain, type, protocol);
    } else {
        return CDNS_ERR_UNDEFINED;
    }
    int flags = fcntl(sock, F_GETFL, 0);
    if(flags == -1) return CDNS_ERR_UNDEFINED;
    flags |= O_NONBLOCK;
    if(fcntl(sock, F_SETFL, flags) != 0) {
        return CDNS_ERR_UNDEFINED;
    }
    if(config->netProto == CdnsNetProtoInet4) {
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = port,
            .sin_addr = *((u_int32_t*)config->addr),
            .sin_zero = 0,
        };
        int err = bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
        if(err != 0) {
            return CDNS_ERR_UNDEFINED;
        }
    } else {
        struct sockaddr_in6 addr = {
            .sin6_family = AF_INET,
            .sin6_port = port,
        };
        memcpy((void*)&addr.sin6_addr, config->addr, 16);
        int err = bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
        if(err != 0) {
            return CDNS_ERR_UNDEFINED;
        }
    }
    out->config = *config;
    out->isOpen = true;
    out->socket = sock;
    return 0;
}
int cdnsCreateDns(CdnsState **out, const CdnsConfig *config) {
    DnsState* state = (DnsState*)malloc(sizeof(DnsState));
    if(state == 0) {
        return CDNS_ERR_MEM;
    }
    if(config->threadRequests != 0) {
        state->threadRequests = config->threadRequests;
    } else {
        state->threadRequests = DEFAULT_THREAD_REQUESTS;
    }
    state->threadOutgoingRequests = config->threadOutgoingRequests;
    if(config->resendDelayMs != 0) {
        state->resendDelay = config->resendDelayMs;
    } else {
        state->resendDelay = DEFAULT_RESEND_DELAY;
    }
    if(config->maxResendCount != 0) {
        state->maxResendCount = config->maxResendCount;
    } else {
        state->maxResendCount = DEFAULT_RESEND_ATTEMPTS;
    }
    state->numThreads = 1;
    state->maxThreads = 1;
    memset(&state->callback, 0, sizeof(CdnsCallbackDescriptor));
    state->listening = false;
    state->paused = true;
    memset(&state->connections, 0, sizeof(DnsConnections));

    memset(state->requestMakers, 0, sizeof(DnsState) * 6); // Set these all to be uncreated

    createCollection(&state->reqDataCollection, sizeof(OutgoingRequestTrackingData), state->threadOutgoingRequests * state->maxThreads);
    ReusableDataCollection resDataCollection = {};
    state->resDataCollection = resDataCollection;

    state->numListeners = config->numListeners;
    state->listeners = (DnsListener*)malloc(config->numListeners * sizeof(DnsListener));
    if(state->listeners == NULL) {
        return CDNS_ERR_MEM;
    }
    for(int i = 0;i < config->numListeners;i++) {
        int err = makeListener(&config->listeners[i], &state->listeners[i]);
        if(err != 0) {
            return err;
        }
    }

    *out = (CdnsState*)state;
    
    return 0;
}
int cdnsDestroyDns(CdnsState *_state) {
    DnsState* state = (DnsState*)_state;
    if(state->listening) {
        int a = cdnsPause(_state);
        if(a != 0) {
            return a;
        }
    }

    for(int i = 0;i < state->numListeners;i++) {
        DnsListener* listener = &state->listeners[i];
        if(listener->isOpen) {
            close(listener->socket);
        }
    }
    free(state->listeners);
    free(state);
    destroyCollection(&state->reqDataCollection);
    destroyCollection(&state->resDataCollection);
    return 0;
}
int cdnsSetCallback(CdnsState *_state, const CdnsCallbackDescriptor* callback) {
    if(callback->callback == NULL) {
        return CDNS_ERR_INVALID_CALLBACK;
    }
    DnsState* state = (DnsState*)_state;
    if(!state->paused) {
        return CDNS_ERR_MODIFY_WHILE_RUNNING;
    }
    state->callback = *callback;
    return createCollection(&state->resDataCollection, sizeof(ResponseCycleData) + callback->perCallbackDataSize, state->maxThreads * state->threadRequests);
}
int cdnsPoll(CdnsState *_state) {
    DnsState* state = (DnsState*)_state;
    if(state->callback.callback == NULL) {
        return CDNS_ERR_NO_CALLBACK;
    }
    if(state->listening) {
        return CDNS_ERR_ALREADY_LISTENING;
    }
    state->paused = false;
    state->listening = true;
    // TODO
    state->listening = false;
    return 0;
}
int cdnsPause(CdnsState *_state) {
    DnsState* state = (DnsState*)_state;
    if(state->listening) {
        return CDNS_ERR_INVALID_PAUSE;
    }
    state->paused = true;
    // TODO
    return 0;
}

int cdnsGetResponseReadInfo(CdnsResponseContext *context, CdnsRequestId id, CdnsPacketReadInfo **out) {
    return CDNS_ERR_UNDEFINED;
}
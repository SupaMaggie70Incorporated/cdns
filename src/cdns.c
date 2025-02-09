// https://datatracker.ietf.org/doc/html/rfc1034
// https://datatracker.ietf.org/doc/html/rfc1035
// https://www.cloudflare.com/learning/dns/dns-records/

#include "cdns.h"
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/un.h>

#define CDNS_ERR_UNDEFINED -1
#define CDNS_NUM_ERR 11

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

/// Followed by data in memory
typedef struct ResponseCycleData {
    CdnsCallbackCycleInfo info;
} ResponseCycleData;

typedef struct DnsConnections {
    pthread_t* threads;
} DnsConnections;

typedef struct DnsState {
    u_int16_t udpPort;
    u_int16_t tcpPort;
    int udpSocket;
    int tcpSocket;
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
        "STATE MDOFIIED WHILE UNPAUSED"
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

int cdnsCreateDns(CdnsState **out, const CdnsConfig *config) {
    if(config->tcpPort != 0) {
        return CDNS_ERR_TCP;
    }
    if(config->initialThreads != 0 || config->maxThreads != 0) {
        return CDNS_ERR_THREADS;
    }
    if(config->httpPort != 0) {
        return CDNS_ERR_HTTP;
    }
    DnsState* state = (DnsState*)malloc(sizeof(DnsState));
    if(state == 0) {
        return CDNS_ERR_MEM;
    }

    if(config->udpPort != 0) {
        state->udpPort = config->udpPort;
    } else {
        state->udpPort = CDNS_PORT;
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
    state->udpSocket = 0;
    state->tcpSocket = 0;
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

    if(state->udpSocket != 0) {
        if(close(state->udpSocket) != 0) {
            return CDNS_ERR_SOCK_CLOSE;
        }
    }
    if(state->tcpSocket != 0) {
        if(close(state->tcpSocket) != 0) {
            return CDNS_ERR_SOCK_CLOSE;
        }
    }
    free(state);
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
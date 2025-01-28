// https://datatracker.ietf.org/doc/html/rfc1034
// https://datatracker.ietf.org/doc/html/rfc1035

#include "cdns.h"
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/un.h>


#define NUM_ERR 9

char *cdnsGetErrorString(int error) {
    char* strings[NUM_ERR + 1] = {
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
    };
    if(error <= NUM_ERR) {
        return strings[error];
    } else {
        return "UNKNOWN ERROR";
    }
}

#define UDP_PORT 53
#define DEFAULT_THREAD_REQUESTS 256
#define DEFAULT_RESEND_DELAY 1000
#define DEFAULT_RESEND_ATTEMPTS 10

typedef struct DnsConnections {
    pthread_t* threads;
} DnsConnections;

typedef struct DnsState {
    u_int16_t udpPort;
    u_int16_t tcpPort;
    int udpSocket;
    int tcpSocket;
    int threadRequests;
    int numThreads;
    int maxThreads;
    int resendDelay;
    int maxResendCount;

    CdnsCallbackDescriptor callback;
    bool listening;
    DnsConnections connections;
} DnsState;

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
        state->udpPort = UDP_PORT;
    }
    if(config->threadRequests != 0) {
        state->threadRequests = config->threadRequests;
    } else {
        state->threadRequests = DEFAULT_THREAD_REQUESTS;
    }
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
    memset(&state->connections, 0, sizeof(DnsConnections));

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
    state->callback = *callback;
    return 0;
}
int cdnsPoll(CdnsState *_state) {
    DnsState* state = (DnsState*)_state;
    if(state->callback.callback == NULL) {
        return CDNS_ERR_NO_CALLBACK;
    }
    if(state->listening) {
        return CDNS_ERR_ALREADY_LISTENING;
    }
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
    // TODO
    return 0;
}



typedef struct DnsPacketHeader {
    /// Request/response id
    u_int16_t id;
    u_int16_t qr : 1;
    u_int16_t opcode : 4;
    u_int16_t aa : 1;
    u_int16_t tc : 1;
    u_int16_t rd : 1;
    u_int16_t ra : 1;
    u_int16_t z : 3;
    u_int16_t rcode : 4;
    /// Question count
    u_int16_t qdcount;
    /// Answer count
    u_int16_t ancount;
    /// Authority count
    u_int16_t nscount;
    /// Additional count
    u_int16_t arcount;
} DnsPacketHeader;
typedef struct DnsQuestion {
    // TODO: verify that this is variable length
    char* qname;
    u_int16_t qtype;
    u_int16_t qclass;
} DnsQuestion;
typedef struct DnsPacket {
    DnsPacketHeader header;
    DnsQuestion question;
    // Answer RRs
    // Authority RRs
    // Additional RRs
} DnsPacket;
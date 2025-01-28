#ifndef _CDNS_H_
#define _CDNS_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/**
 * @defgroup cnds cdns
 * @brief This exists so that doxygen would organize all things into a single
 * page
 * @{
 */

/// Includes the IP, port and a DNS id for a given request
typedef struct CdnsRequestId {
  u_int64_t data;
} CdnsRequestId;
/// The cdns server instance. This can also make requests itself.
typedef struct CdnsState CdnsState;
/// Context used to handle a dns request
typedef struct CdnsResponseContext CdnsResponseContext;
typedef enum CdnsResponseStatus {
  /// The response can be considered complete(even if it was terminated)
  CdnsReturned,
  /// Some time should be waited before attempting to respond again
  CdnsWaitMs,
  /// The response should await some other outgoing request
  CdnsPoll,
} CdnsResponseStatus;
/// Whether/how a callback cycle should continue
typedef struct CdnsResponseInfo {
  CdnsResponseStatus status;
  /// The data, either a CdnsRequestId or an integer number of milliseconds
  u_int64_t data;
} CdnsResponseInfo;

/// Returns whether the current callback cycle can be completed.
///
/// Parameters: context, data pointer, whether this is the first call for a
/// cycle.
typedef CdnsResponseInfo (*CdnsCallback)(CdnsResponseContext *, void *, bool);

typedef struct CdnsCallbackDescriptor {
  /// Size of the data that is stored per request/response cycle
  int perCallbackDataSize;
  /// The callback to be called in the cycle
  CdnsCallback callback;
} CdnsCallbackDescriptor;

typedef struct CdnsConfig {
  /// Defaults to 53
  u_int16_t udpPort;
  /// Defaults to no TCP support
  u_int16_t tcpPort;
  /// Defaults to no HTTP support
  u_int16_t httpPort;
  /// Defaults to 1 thread
  u_int16_t initialThreads;
  /// Defaults to 1 thread. If this is higher than initialThreads, then new
  /// threads may be dynamically created when needed
  u_int16_t maxThreads;
  /// Defaults to 256. Maximum requests handled by a single thread concurrently.
  u_int32_t threadRequests;
} CdnsConfig;

/// Creates a DNS instance
int cdnsCreateDns(CdnsState **state, const CdnsConfig *config);
/// Sets the callback for a DNS instance. Must be called before cdnsPoll
int cdnsSetCallback(CdnsState *state, const CdnsCallbackDescriptor *callback);
/// Begin listening on the current thread, blocking. May return while
/// connections are still being processed in the background.
int cdnsPoll(CdnsState *state);
/// Should be called when the DNS server is not polling. Will close all
/// currently pending requests
int cdnsPause(CdnsState *state);
/// Destroys the DNS instance after pausing
int cdnsDestroyDns(CdnsState *state);
/// Gets the string representation of an error value
char *cdnsGetErrorString(int error);

#define CDNS_CHECK_ERROR(VALUE)                                                \
  {                                                                            \
    int value = VALUE;                                                         \
    if (value != 0) {                                                          \
      printf("CDNS ERROR: %s\n", cdnsGetErrorString(value));                   \
      exit(1);                                                                 \
    }                                                                          \
  }
#define CDNS_ERR_MEM 1
#define CDNS_ERR_TCP 2
#define CDNS_ERR_THREADS 3
#define CDNS_ERR_HTTP 4
#define CDNS_ERR_SOCK_CLOSE 5
#define CDNS_ERR_NO_CALLBACK 6
#define CDNS_ERR_ALREADY_LISTENING 7
#define CDNS_ERR_INVALID_CALLBACK 8
#define CDNS_ERR_INVALID_PAUSE 9

#endif
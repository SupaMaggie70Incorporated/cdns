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
/// Info about a DNS request, incoming or outgoing
typedef struct CdnsRequestInfo {
  CdnsRequestId id;
} CdnsRequestInfo;
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
  unsigned int initialThreads;
  /// Defaults to 1 thread. If this is higher than initialThreads, then new
  /// threads may be dynamically created when needed
  unsigned int maxThreads;
  /// Defaults to 256. Maximum requests handled by a single thread concurrently.
  unsigned int threadRequests;
  /// Defaults to 1000. Amount of time to wait after sending a request to
  /// resend.
  unsigned int resendDelayMs;
  /// Defaults to 10. Number of times to resend a DNS request before giving up.
  unsigned int maxResendCount;
} CdnsConfig;

typedef enum CdnsRecordType : u_int16_t {
  CDNS_RR_NULL = 0,
  CDNS_RR_A = 1,
  CDNS_RR_NS = 2,
  // 3, 4 are obsolete
  CDNS_RR_CNAME = 5,
  CDNS_RR_SOA = 6,
  // 7, 8, 9, 10, 11 are obsolete
  CDNS_RR_PTR = 12,
  // 13, 14 are obsolete
  CDNS_RR_MX = 15,
  CDNS_RR_TXT = 16,
  CDNS_RR_RP = 17,
  CDNS_RR_AFSDB = 18,
  CDNS_RR_SIG = 24,
  CDNS_RR_KEY = 25,
  CDNS_RR_AAAA = 28,
  CDNS_RR_LOC = 29,
  CDNS_RR_SRV = 33,
  CDNS_RR_NAPTR = 35,
  CDNS_RR_KX = 36,
  CDNS_RR_CERT = 37,
  CDNS_RR_DNAME = 39,
  CDNS_RR_APL = 42,
  CDNS_RR_DS = 43,
  CDNS_RR_SSHFP = 44,
  CDNS_RR_IPSECKEY = 45,
  CDNS_RR_RRSIG = 46,
  CDNS_RR_NSEC = 47,
  CDNS_RR_DNSKEY = 48,
  CDNS_RR_DHCID = 49,
  CDNS_RR_NSEC3 = 50,
  CDNS_RR_NSEC3PARAM = 51,
  CDNS_RR_TSLA = 52,
  CDNS_RR_SMIMEA = 53,
  CDNS_RR_HIP = 55,
  CDNS_RR_CDS = 59,
  CDNS_RR_CDNSKEY = 60,
  CDNS_RR_OPENPGPKEY = 61,
  CDNS_RR_CSYNC = 62,
  CDNS_RR_ZONEMD = 63,
  CDNS_RR_SVCB = 64,
  CDNS_RR_HTTPS = 65,
  /// The wildcard/asterisk, returns all records. Not a real record type
  CDNS_RR_ALL = 255,

} CdnsRecordType;
typedef enum CdnsRecordClass : u_int16_t {
  CDNS_RC_NULL = 0,
  CDNS_RC_IN = 1,
  CDNS_RC_CH = 3,
  CDNS_RC_HS = 4,
  /// The wildcard/asterisk, returns all records. Not a real record class
  CDNS_RC_ALL = 255,
} CdnsRecordClass;

typedef struct CdnsResourceRecord {
  // name,
  CdnsRecordType type;
  u_int16_t clas;
  u_int32_t ttl;
  u_int16_t rdlength;
  // rdata follows the struct
} DnsRequestFormat;

typedef enum CdnsOpcode {
  CDNS_OP_QUERY = 0,
  CDNS_OP_IQUERY = 1,
  CDNS_OP_STATUS = 2,
  CDNS_OP_NOTIFY = 4,
  CDNS_OP_UPDATE = 5,
  CDNS_OP_DSO = 6,
} CdnsOpcode;
typedef enum CdnsRcode {
  CDNS_RC_NOERROR = 0,
  CDNS_RC_FORMAT_ERR = 1,
  CDNS_RC_SERVER_ERR = 2,
  CDNS_RC_NAME_ERR = 3,
  CDNS_RC_NOT_IMPLEMENTED = 4,
  CDNS_RC_REFUSED = 5,
} CdnsRcode;

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
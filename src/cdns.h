#ifndef _CDNS_H_
#define _CDNS_H_

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define CDNS_PORT 53

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
/// On what condition the callback is exiting
typedef enum CdnsCallbackCycleStatus {
  /// The response can be considered complete(even if it was terminated)
  CdnsReturned,
  /// Some time should be waited before attempting to respond again
  CdnsWaitMs,
  /// The response should await some other outgoing request
  CdnsPoll,
} CdnsCallbackCycleStatus;
typedef union CdnsCallbackCycleData {
  u_int64_t ms;
  CdnsRequestId id;
} CdnsCallbackCycleData;
/// Whether/how a callback cycle should continue
typedef struct CdnsCallbackCycleInfo {
  CdnsCallbackCycleStatus status;
  /// The data, either a CdnsRequestId or an integer number of milliseconds
  CdnsCallbackCycleData data;
} CdnsCallbackCycleInfo;

/// Returns whether the current callback cycle can be completed.
///
/// Parameters: context, data pointer, whether this is the first call for a
/// cycle.
typedef CdnsCallbackCycleInfo (*CdnsCallback)(CdnsResponseContext *, void *,
                                              bool);

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

/// Type of resource record
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
/// Clas of record(for example IN for internet)
typedef enum CdnsRecordClass : u_int16_t {
  CDNS_RC_NULL = 0,
  CDNS_RC_IN = 1,
  CDNS_RC_CH = 3,
  CDNS_RC_HS = 4,
  /// The wildcard/asterisk, returns all records. Not a real record class
  CDNS_RC_ALL = 255,
} CdnsRecordClass;

/// Name comes before this, data comes after
typedef struct CdnsResourceRecordInfo {
  ///
  CdnsRecordType type;
  ///
  u_int16_t clas;
  /// Time to live(if cached)
  u_int32_t ttl;
  /// Data length
  u_int16_t rdlength;
} CdnsResourceRecordInfo;
/// Query type
typedef enum CdnsOpcode : u_int8_t {
  CDNS_OP_QUERY = 0,
  CDNS_OP_IQUERY = 1,
  CDNS_OP_STATUS = 2,
  CDNS_OP_NOTIFY = 4,
  CDNS_OP_UPDATE = 5,
  CDNS_OP_DSO = 6,
} CdnsOpcode;
/// Response code
typedef enum CdnsRcode : u_int8_t {
  CDNS_RC_NOERROR = 0,
  CDNS_RC_FORMAT_ERR = 1,
  CDNS_RC_SERVER_ERR = 2,
  CDNS_RC_NAME_ERR = 3,
  CDNS_RC_NOT_IMPLEMENTED = 4,
  CDNS_RC_REFUSED = 5,
} CdnsRcode;
/// The header for a DNS packet
typedef struct CdnsPacketHeader {
  /// Request/response id
  u_int16_t id;
  /// Whether a query(0) or response(1)
  u_int16_t qr : 1;
  /// Type of query(if any)
  u_int16_t opcode : 4;
  /// Authoritative answer - whether or not the server is an authority for the
  /// domain name in the question section
  u_int16_t aa : 1;
  /// Whether the message is truncated due to limits on
  u_int16_t tc : 1;
  /// Recursion desired - whether the responder should look up the query
  /// recursively
  u_int16_t rd : 1;
  /// Whether or not recursive query is available on the server
  u_int16_t ra : 1;
  /// Reserved
  u_int16_t z : 3;
  /// Response code
  u_int16_t rcode : 4;
  /// Question count
  u_int16_t qdcount;
  /// Answer count
  u_int16_t ancount;
  /// Authority count
  u_int16_t nscount;
  /// Additional count
  u_int16_t arcount;
} CdnsPacketHeader;
/// A question used in DNS queries
///
/// Prefixed by qname, or the name of data. This is stored as a list of length
/// bytes, the data itself, and this list is terminated by a null byte
typedef struct CdnsQuestion {
  u_int16_t qtype;
  u_int16_t qclass;
} CdnsQuestion;

/// Followed by data blob of questions then answer, authority, and additional
/// RRs
typedef struct CdnsPacket {
  CdnsPacketHeader header;
} CdnsPacket;

typedef struct CdnsPacketReadInfo {
  u_int32_t numRecords;
  /// The size of the question or record blob, whichever is valid
  u_int32_t blobSize;
  CdnsPacketHeader *header;
  CdnsQuestion **questions;
  void **records;
} CdnsPacketReadInfo;

typedef struct CdnsResponseWriteinfo CdnsResponseWriteinfo;
typedef struct CdnsRequestWriteInfo CdnsRequestWriteInfo;

typedef enum CdnsNetworkProtocolType {
  CdnsNetProtoInet4,
  CdnsNetProtoInet6
} CdnsNetworkProtocolType;
typedef enum CdnsProtocolType {
  CdnsProtoUdp,
  CdnsProtoTcp,
  CdnsProtoHttp
} CdnsProtocolType;
typedef struct CdnsRequestDestination {
  CdnsNetworkProtocolType netProtocol;
  CdnsProtocolType protocol;
  u_int64_t address;
  u_int16_t port;
} CdnsRequestDestination;

/// Creates a DNS instance
int cdnsCreateDns(CdnsState **state, const CdnsConfig *config);
/// Sets the callback for a DNS instance. Must be called before cdnsPoll
int cdnsSetCallback(CdnsState *state, const CdnsCallbackDescriptor *callback);
/// Begin listening on the current thread, blocking. May return while
/// connections are still being processed in the background if multiple threads
/// are used.
int cdnsPoll(CdnsState *state);
/// Should be called when the DNS server is not polling. Will close all
/// currently pending requests if multiple threads are used
int cdnsPause(CdnsState *state);
/// Destroys the DNS instance after pausing
int cdnsDestroyDns(CdnsState *state);
/// Gets the string representation of an error value
char *cdnsGetErrorString(int error);

/// Sets the read info for a request previously made if a response has been
/// received, zero otherwise. May return an error if there was an error with the
/// response
int cdnsGetResponseReadInfo(CdnsResponseContext *context, CdnsRequestId req,
                            CdnsPacketReadInfo **out);

int cdnsCreateRequest(CdnsResponseContext *context, CdnsRequestWriteInfo **out);
int cdnsWritableRequestHeader(CdnsRequestWriteInfo *writer,
                              CdnsPacketHeader **out);
/// You can write either a single question or multiple with this call. No
/// validation is done.
int cdnsWriteQuestion(CdnsRequestWriteInfo *writer, void *question, int length);
int cdnsSendRequest(CdnsRequestWriteInfo *writer,
                    CdnsRequestDestination destination, CdnsRequestId *id);

int cdnsGetRequestReadInfo(CdnsResponseContext *context,
                           CdnsPacketReadInfo **out);
int cdnsGetResponseWriter(CdnsResponseContext *context,
                          CdnsResponseWriteinfo **out);
int cdnsWritableResponseHeader(CdnsResponseWriteinfo *writer,
                               CdnsPacketHeader **out);
/// You can write either a single record or multiple with this call. No
/// validation is done.
int cdnsWriteRecord(CdnsResponseWriteinfo *writer, void *record, int length);
int cdnsSendResponse(CdnsResponseWriteinfo *writer);

inline unsigned char _cdnsReverseByte(unsigned char b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}
/// Currently only works with <= 16 bits
///
/// PLATFORM SPECIFIC: This does not zero other bits on big endian systems
inline u_int64_t cdnsToLittleEndianArbitrary(u_int64_t value, int numBits) {
#if __BYTE_ORDER == __BIG_ENDIAN
  return value;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
  if (numBits == 0) {
    return 0;
  }
  if (numBits <= 8) {
    return (u_int64_t)_cdnsReverseByte((unsigned char)value) >> (8 - numBits);
  } else if (numBits <= 16) {
    u_int16_t v1 = (u_int16_t)_cdnsReverseByte((unsigned char)value);
    u_int16_t v2 = (u_int16_t)_cdnsReverseByte((unsigned char)(value >> 8)) >>
                   (16 - numBits);
    return (u_int64_t)v2 + (v1 << 8);
  }
  return 1;
#else
#error "__BYTE_ORDER macro not supported by compiler"
#endif
}

#define CDNS_CHECK_ERROR(VALUE)                                                \
  {                                                                            \
    int value = VALUE;                                                         \
    if (value != 0) {                                                          \
      printf("CDNS ERROR: %s\n", cdnsGetErrorString(value));                   \
      exit(1);                                                                 \
    }                                                                          \
  }

#define CDNS_ERR_NONE 0
#define CDNS_ERR_MEM 1
#define CDNS_ERR_TCP 2
#define CDNS_ERR_THREADS 3
#define CDNS_ERR_HTTP 4
#define CDNS_ERR_SOCK_CLOSE 5
#define CDNS_ERR_NO_CALLBACK 6
#define CDNS_ERR_ALREADY_LISTENING 7
#define CDNS_ERR_INVALID_CALLBACK 8
#define CDNS_ERR_INVALID_PAUSE 9
#define CDNS_ERR_REQ_SERVER 10

#endif
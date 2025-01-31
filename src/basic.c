#include <stdio.h>
#include "cdns.h"

CdnsCallbackCycleInfo callback(CdnsResponseContext* context, void* data, bool first) {
    // Brief explanation: when a request is received, immediately forward it to a server and await the response.
    if(first) {
        CdnsPacketReadInfo *req;
        CDNS_CHECK_ERROR(cdnsGetRequestReadInfo(context, &req));

        CdnsRequestWriteInfo *wReq;
        CDNS_CHECK_ERROR(cdnsCreateRequest(context, &wReq));
        CdnsPacketHeader *header;
        CDNS_CHECK_ERROR(cdnsWritableRequestHeader(wReq, &header));
        // We want to preserve the assigned id
        u_int16_t reqId = header->id;
        *header = *req->header;
        header->id = reqId;

        if(header->qdcount > 0) {
            CDNS_CHECK_ERROR(cdnsWriteQuestion(wReq, req->records[0], req->blobSize));
        }
        CdnsRequestDestination dest = {
            .netProtocol = CdnsNetProtoInet4,
            .protocol = CdnsProtoUdp,
            .address = htonl(0x08080808), // 8.8.8.8, google's public DNS 
            .port = htonl(CDNS_PORT)
        };
        CdnsRequestId id;
        CDNS_CHECK_ERROR(cdnsSendRequest(wReq, dest, &id));
        *((CdnsRequestId*)data) = id;

        CdnsCallbackCycleInfo out = {
            .data.id = id,
            .status = CdnsPoll
        };
        return out;
    } else {
        CdnsPacketReadInfo *info;
        CDNS_CHECK_ERROR(cdnsGetResponseReadInfo(context, *(CdnsRequestId*)data, &info));
        CdnsResponseWriteinfo *wRes;
        CDNS_CHECK_ERROR(cdnsGetResponseWriter(context, &wRes));
        CdnsPacketHeader *header;
        u_int16_t resId = header->id;
        *header = *info->header;
        header->id = resId;
        if(info->numRecords > 0) {
            CDNS_CHECK_ERROR(cdnsWriteRecord(wRes, info->records[0], info->blobSize));
        }
        CDNS_CHECK_ERROR(cdnsSendResponse(wRes));

        CdnsCallbackCycleInfo out = {
            .status = CdnsReturned
        };
        return out;
    }
}

int main(int argc, char** argv) {
    printf("Hello, world!\n");
    CdnsState* state;
    CdnsConfig config = {

    };
    CDNS_CHECK_ERROR(cdnsCreateDns(&state, &config));
    CdnsCallbackDescriptor callbackConfig = {
        .callback = callback,
        .perCallbackDataSize = 8
    };
    CDNS_CHECK_ERROR(cdnsSetCallback(state, &callbackConfig));
    CDNS_CHECK_ERROR(cdnsPoll(state));
    CDNS_CHECK_ERROR(cdnsPause(state));
    CDNS_CHECK_ERROR(cdnsDestroyDns(state));
    return 0;
}
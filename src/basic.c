#include <stdio.h>
#include "cdns.h"

CdnsResponseInfo callback(CdnsResponseContext* context, void* data, bool first) {
    CdnsResponseInfo out = {
        .data = 0,
        .status = CdnsReturned
    };
    return out;
}

int main(int argc, char** argv) {
    printf("Hello, world!\n");
    CdnsState* state;
    CdnsConfig config = {

    };
    CDNS_CHECK_ERROR(cdnsCreateDns(&state, &config));
    CdnsCallbackDescriptor callbackConfig = {
        .callback = callback,
        .perCallbackDataSize = 0
    };
    CDNS_CHECK_ERROR(cdnsSetCallback(state, &callbackConfig));
    CDNS_CHECK_ERROR(cdnsPoll(state));
    CDNS_CHECK_ERROR(cdnsPause(state));
    CDNS_CHECK_ERROR(cdnsDestroyDns(state));
    return 0;
}
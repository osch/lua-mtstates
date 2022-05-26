#ifndef MTSTATES_RECEIVER_CAPI_IMPL_H
#define MTSTATES_RECEIVER_CAPI_IMPL_H

#include "util.h"
#include "receiver_capi.h"

extern const receiver_capi mtstates_receiver_capi_impl;

typedef enum {
    BUFFER_INTEGER,
    BUFFER_BYTE,
    BUFFER_NUMBER,
    BUFFER_BOOLEAN,
    BUFFER_STRING,
    BUFFER_SMALLSTRING,
    BUFFER_CARRAY
} SerializeDataType;


struct receiver_writer
{
    int nargs;
    MemBuffer mem;
};



#endif /* MTSTATES_RECEIVER_CAPI_IMPL_H */

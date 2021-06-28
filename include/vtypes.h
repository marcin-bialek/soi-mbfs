#pragma once
#include <inttypes.h>


typedef struct {
    uint64_t position;
    int state;
    int64_t i;
    uint64_t unumber;
    uint64_t length;
    int8_t* buffer;
} VTypesCoder;


typedef enum {
    VTYPES_OK,
    VTYPES_END_OF_BUFFER,
    VTYPES_INCORRECT_STRING
} VTypesError;


VTypesCoder vtypesMakeCoder(void);

VTypesError vtypesEncodeULONG(VTypesCoder* encoder, uint64_t value, int8_t* buffer, uint64_t capacity);
VTypesError vtypesEncodeVULONG(VTypesCoder* encoder, uint64_t value, int8_t* buffer, uint64_t capacity);
VTypesError vtypesEncodeBYTEARRAY(VTypesCoder* encoder, const int8_t* value, uint64_t size, int8_t* buffer, uint64_t capacity);
VTypesError vtypesEncodeSTRING(VTypesCoder* encoder, const char* value, int8_t* buffer, uint64_t capacity);

VTypesError vtypesDecodeULONG(VTypesCoder* decoder, uint64_t* value, const int8_t* data, uint64_t size);
VTypesError vtypesDecodeVULONG(VTypesCoder* decoder, uint64_t* value, const int8_t* data, uint64_t size);
VTypesError vtypesDecodeBYTEARRAY(VTypesCoder* decoder, int8_t** value, uint64_t* arraySize, const int8_t* data, uint64_t size);
VTypesError vtypesDecodeSTRING(VTypesCoder* decoder, char** value, const int8_t* data, uint64_t size);


#include <stdlib.h>
#include <string.h>
#include "vtypes.h"


VTypesCoder vtypesMakeCoder(void) {
    VTypesCoder coder;
    coder.state = 0;
    coder.position = 0;
    return coder;
}


static VTypesError vtypesEncodeUFIXED(VTypesCoder* encoder, uint64_t value, uint8_t size, int8_t* buffer, uint64_t capacity) {
    switch(encoder->state) {
    case 0: {
        encoder->unumber = value;
        encoder->length = size;
        encoder->i = 0;
        encoder->state = 1;
    }
    case 1: {
        while(encoder->i < encoder->length) {
            if(encoder->position >= capacity) {
                encoder->position = 0;
                return VTYPES_END_OF_BUFFER;
            }

            buffer[encoder->position++] = encoder->unumber & 0xff;
            encoder->unumber >>= 8;
            encoder->i++;
        }
    }
    }

    encoder->state = 0;
    return VTYPES_OK;
}


VTypesError vtypesEncodeULONG(VTypesCoder* encoder, uint64_t value, int8_t* buffer, uint64_t capacity) {
    return vtypesEncodeUFIXED(encoder, value, 8, buffer, capacity);
}


VTypesError vtypesEncodeVULONG(VTypesCoder* encoder, uint64_t value, int8_t* buffer, uint64_t capacity) {
    switch(encoder->state) {
    case 0: {
        int i = 63;

        while(i > 0 && !(value & (0xefLL << i))) {
            i -= 7;
        }

        encoder->i = i;
        encoder->unumber = value;
        encoder->state = 1;
    }
    case 1: {
        int8_t b;

        do {
            b = (encoder->unumber >> encoder->i) & 0x7f;

            if(encoder->i != 0) {
                b |= 0x80;
            }

            if(encoder->position >= capacity) {
                encoder->position = 0;
                return VTYPES_END_OF_BUFFER;
            }

            buffer[encoder->position++] = b;
            encoder->i -= 7;
        } while(encoder->i >= 0);
    }
    }

    encoder->state = 0;
    return VTYPES_OK;
}


VTypesError vtypesEncodeBYTEARRAY(VTypesCoder* encoder, const int8_t* value, uint64_t size, int8_t* buffer, uint64_t capacity) {
    switch(encoder->state) {
    case 0: 
    case 1: {
        if(vtypesEncodeVULONG(encoder, size, buffer, capacity) == VTYPES_END_OF_BUFFER) {
            return VTYPES_END_OF_BUFFER;
        }

        encoder->i = 0;
        encoder->state = 2;
    }
    case 2: {
        while(encoder->i < size) {
            if(encoder->position >= capacity) {
                encoder->position = 0;
                return VTYPES_END_OF_BUFFER;
            }

            buffer[encoder->position++] = value[encoder->i++];
        }
    }
    }

    encoder->state = 0;
    return VTYPES_OK;
}


VTypesError vtypesEncodeSTRING(VTypesCoder* encoder, const char* value, int8_t* buffer, uint64_t capacity) {
    switch(encoder->state) {
    case 0: {
        encoder->length = strlen(value) + 1;
    }
    case 1:
    case 2: {
        if(vtypesEncodeBYTEARRAY(
                encoder, 
                (void*)value, 
                encoder->length, 
                buffer, capacity) == VTYPES_END_OF_BUFFER) {
            return VTYPES_END_OF_BUFFER;
        }
    }
    }

    encoder->state = 0;
    return VTYPES_OK;
}


#include <stdio.h>


static VTypesError vtypesDecodeUFIXED(VTypesCoder* decoder, uint64_t* value, uint8_t numberSize, const int8_t* data, uint64_t size) {
    switch(decoder->state) {
    case 0: {
        decoder->unumber = 0;
        decoder->length = numberSize;
        decoder->i = 0;
        decoder->state = 1;
    }
    case 1: {
        while(decoder->i < decoder->length) {
            if(decoder->position >= size) {
                decoder->position = 0;
                return VTYPES_END_OF_BUFFER;
            }

            decoder->unumber |= (0xff & data[decoder->position++]) << ((uint64_t)decoder->i << 3);
            decoder->i++;
        }
    }
    }

    *value = decoder->unumber;
    decoder->state = 0;
    return VTYPES_OK;
}


VTypesError vtypesDecodeULONG(VTypesCoder* decoder, uint64_t* value, const int8_t* data, uint64_t size) {
    return vtypesDecodeUFIXED(decoder, value, 8, data, size);
}


VTypesError vtypesDecodeVULONG(VTypesCoder* decoder, uint64_t* value, const int8_t* data, uint64_t size) {
    switch(decoder->state) {
    case 0: {
        decoder->unumber = 0;
        decoder->state = 1;
    }
    case 1: {
        int8_t b;

        do {
            if(decoder->position >= size) {
                decoder->position = 0;
                return VTYPES_END_OF_BUFFER;
            }

            b = data[decoder->position++];
            decoder->unumber <<= 7;
            decoder->unumber |= b & 0x7f;
        } while(b & 0x80);
    }
    }

    *value = decoder->unumber;
    decoder->state = 0;
    return VTYPES_OK;
}


VTypesError vtypesDecodeBYTEARRAY(VTypesCoder* decoder, int8_t** value, uint64_t* arraySize, const int8_t* data, uint64_t size) {
    switch (decoder->state) {
    case 0:
    case 1: {
        if(vtypesDecodeVULONG(decoder, &decoder->length, data, size) == VTYPES_END_OF_BUFFER) {
            return VTYPES_END_OF_BUFFER;
        }

        decoder->buffer = malloc(decoder->length);
        decoder->i = 0;
        decoder->state = 2;
    }
    case 2: {
        while(decoder->i < decoder->length) {
            if(decoder->position >= size) {
                decoder->position = 0;
                return VTYPES_END_OF_BUFFER;
            }

            decoder->buffer[decoder->i++] = data[decoder->position++];
        }
    }
    }

    *value = decoder->buffer;
    *arraySize = decoder->length;
    decoder->state = 0;
    return VTYPES_OK;
}


VTypesError vtypesDecodeSTRING(VTypesCoder* decoder, char** value, const int8_t* data, uint64_t size) {
    uint64_t length;
    
    if(vtypesDecodeBYTEARRAY(decoder, (int8_t**)value, &length, data, size) == VTYPES_END_OF_BUFFER) {
        return VTYPES_END_OF_BUFFER;
    }

    if((*value)[length - 1] != 0x00) {
        free(*value);
        *value = NULL;
        return VTYPES_INCORRECT_STRING;
    }

    return VTYPES_OK;
}



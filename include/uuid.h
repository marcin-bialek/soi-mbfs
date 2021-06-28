#pragma once
#include <inttypes.h>


typedef struct {
    uint64_t highBytes;
    uint64_t lowBytes;
} UUID;


int uuidFromString(UUID* uuid, const char* str);
void uuidToString(const UUID* uuid, char* buffer);
void uuidToByteArray(const UUID* uuid, int8_t* buffer);

#include <uuid.h>
#include <string.h>


static int nibbleToInt(char n) {
    if('0' <= n && n <= '9') {
        return n - '0';
    }
    else if('a' <= n && n <= 'f') {
        return n - 'a' + 10;
    }
    else if('A' <= n && n <= 'F') {
        return n - 'A' + 10;
    }

    return -1;
}


int uuidFromString(UUID* uuid, const char* str) {
    uint64_t highBytes = 0;
    uint64_t lowBytes = 0;

    for(int i = 0, n; i < 36; i++) {
        if(i == 8 || i == 13 || i == 18 || i == 23) {
            if(str[i] != '-') {
                return -1;
            }

            continue;
        }

        if((n = nibbleToInt(str[i])) == -1) {
            return -1;
        }

        if(i < 18) {
            highBytes = (highBytes << 4) | (n & 0xf); 
        }
        else {
            lowBytes = (lowBytes << 4) | (n & 0xf); 
        }
    }

    uuid->highBytes = highBytes;
    uuid->lowBytes = lowBytes;
    return 0;
}


void uuidToString(const UUID* uuid, char* buffer) {

}


void uuidToByteArray(const UUID* uuid, int8_t* buffer) {
    memcpy(buffer, &uuid->highBytes, 8);
    memcpy(buffer + 8, &uuid->lowBytes, 8);
}



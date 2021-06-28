#include "mbfs.h"


int16_t mbfsHash(const int8_t* data, uint64_t size) {
    uint16_t sum = 0;            

    for(uint64_t i = 0; i < size; i++) {
        sum = (sum >> 1) | ((sum & 1) << 15);
        sum ^= data[i];
    }

    return sum;
}


inline MBFSCoder mbfsMakeCoder(void) {
    MBFSCoder coder;
    coder.state = 0;
    coder.vtypesCoder = vtypesMakeCoder();
    return coder;
}


inline MBFSCoderError mbfsEncodeEntry(MBFSCoder* coder, const MBFSEntry* entry, int8_t* buffer, uint64_t capacity) {
    return entry->encode(coder, entry, buffer, capacity);
}


inline void mbfsDestroyEntry(MBFSEntry* entry) {
    entry->destroy(entry);
}


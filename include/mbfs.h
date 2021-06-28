#pragma once
#include <inttypes.h>
#include "vtypes.h"


#define MBFS_MAGIC          "MBFS"
#define MBFS_MAGIC_X        "MBFX"
#define MBFS_VERSION        1
#define MBFS_BOOT           "$boot"
#define MBFS_FS_INFO        "$fs_info"
#define MBFS_FS_INFO_MIRROR "$fs_info_mirror"
#define MBFS_INDEX          "$index"
#define MBFS_FREE           "$free"


typedef struct __attribute__((packed)) {
    int8_t magic[4];   
    int32_t version;    
    int8_t uuid[16];    
    uint64_t ccount;   
    uint64_t root;      
    uint8_t csp;
    uint8_t iesp;
    int32_t sum;
} MBFSFilesystemInfo;

_Static_assert(sizeof(MBFSFilesystemInfo) == 46, "");


#define MBFS_EMPTY          0x00
#define MBFS_CONTINUE       0x01
#define MBFS_END            0x02
#define MBFS_DIR            0x80
#define MBFS_PDIR           0x81
#define MBFS_FILE           0x82
#define MBFS_PFILE          0x83


typedef struct __attribute__((packed)) {
    uint64_t next;
    int16_t sum;
    uint8_t type;
    int8_t value[];
} MBFSIndexElement;

_Static_assert(sizeof(MBFSIndexElement) == 11, "");


enum MBFSCoderError;
typedef enum MBFSCoderError MBFSCoderError;
struct MBFSCoder;
typedef struct MBFSCoder MBFSCoder;
struct MBFSEntry;
typedef struct MBFSEntry MBFSEntry;


struct MBFSEntry { // base type 
    uint8_t type;
    MBFSCoderError (*encode)(MBFSCoder* coder, const MBFSEntry* entry, int8_t* buffer, uint64_t capacity);
    void (*destroy)(MBFSEntry* entry);
}; 


enum MBFSCoderError {
    MBFSC_OK,
    MBFSC_END_OF_BUFFER,
    MBFSC_BROKEN_ENTRY,
    MBFSC_WRONG_TYPE
};


struct MBFSCoder {
    int state;
    VTypesCoder vtypesCoder;
    int64_t i;
    void* p, *q;
    MBFSEntry* entry;
    uint64_t a, b;
};


int16_t mbfsHash(const int8_t* data, uint64_t size);
MBFSCoder mbfsMakeCoder(void);
MBFSCoderError mbfsEncodeEntry(MBFSCoder* coder, const MBFSEntry* entry, int8_t* buffer, uint64_t capacity);
void mbfsDestroyEntry(MBFSEntry* entry);

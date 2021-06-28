#pragma once
#include "mbfsio.h"


struct MBFSFile;
typedef struct MBFSFile MBFSFile;


typedef struct {
    MBFSDisk* disk;
    MBFSFileEntry* root;
    MBFSIndexElementIterator i;
} MBFSFileIterator;


struct MBFSFile {
    const char* name;
    uint64_t size;
    uint64_t diskSize;
    void* __private;
};


extern const struct MBFS {
    const char* DEFAULT_ID;

    MBFSDisk* (* const createDisk)(const char* device, UUID uuid, uint8_t csp, uint8_t iesp, uint64_t size);
    MBFSDisk* (* const openDisk)(const char* device);
    void (* const closeDisk)(MBFSDisk* disk);

    MBFSFileIterator (* const makeFileIterator)(MBFSDisk* disk);
    MBFSFile* (* const nextFile)(MBFSFileIterator* iterator);
    MBFSFile* (* const makeFile)(MBFSDisk* disk, const char* name);
    MBFSFile* (* const openFile)(MBFSDisk* disk, const char* name);
    void (* const closeFile)(MBFSFile* file);
    void (* const deleteFile)(MBFSFile* file);
    uint64_t (* const extendFile)(MBFSFile* file, uint64_t newSize);
    uint64_t (* const read)(MBFSFile* file, void* buffer);
    uint64_t (* const write)(MBFSFile* file, const void* buffer);

    MBFSFileEntryClusterIterator (* const getClusterIterator)(MBFSFile* file);
} MBFS;


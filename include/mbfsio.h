#pragma once
#include <inttypes.h>
#include "mbfs.h"
#include "mbfsfileentry.h"
#include "uuid.h"


typedef struct MBFSDisk MBFSDisk;


typedef enum {
    MBFSIO_OK,
    MBFSIO_WRONG_PARAMS,
    MBFSIO_FILE_ERROR,
    MBFSIO_BROKEN_FILESYSTEM,
    MBFSIO_UNSUPPORTED_FILESYSTEM
} MBFSIOError;


typedef struct {
    bool valid;
    MBFSFileEntry* file;
    uint64_t index;
    MBFSIndexElement* weakReference;
} MBFSIndexElementPointer;


typedef enum {
    MBFS_IEIM_STEP,
    MBFS_IEIM_NEXT_ENTRY,
    MBFS_IEIM_SAME_ENTRY,
} MBFSIndexElementIteratorMode;


typedef struct {
    bool hasNext;
    uint64_t currentIndex;
    uint64_t currentIndexInCluster;
    MBFSDisk* disk;
    MBFSIndexElementPointer next;
    MBFSIndexElementIteratorMode mode;
} MBFSIndexElementIterator;


MBFSIndexElementPointer mbfsioMakeIndexElementPointer(MBFSFileEntry* indexFile, uint64_t elementIndex);
MBFSIndexElementIterator mbfsioMakeIndexElementIterator(MBFSDisk* disk, MBFSIndexElementPointer start, MBFSIndexElementIteratorMode mode);
MBFSIndexElementPointer mbfsioIndexElementIteratorNext(MBFSIndexElementIterator* iterator);
uint64_t mbfsioAllocCluster(MBFSDisk* disk);
uint64_t mbfsioFreeCluster(MBFSDisk* disk, uint64_t cluster);
uint64_t mbfsioAllocClustersForFileEntry(MBFSDisk* disk, MBFSFileEntry* file, uint64_t clustersCount);
void mbfsioRemoveEntry(MBFSDisk* disk, MBFSIndexElementPointer start);
int mbfsioInsertEntry(MBFSDisk* disk, MBFSFileEntry* indexFile, const MBFSEntry* entry, MBFSIndexElementPointer* ptr);
MBFSFileEntry* mbfsioDecodeMBFSFileEntry(MBFSDisk* disk, MBFSIndexElementPointer start);
MBFSFileEntry* mbfsioFindAndDecodeFileEntry(MBFSDisk* disk, MBFSFileEntry* indexFile, const char* name, MBFSIndexElementPointer* ptr);
MBFSFileEntry* mbfsioGetRootIndexFileEntry(MBFSDisk* disk);
uint64_t mbfsioGetClusterSize(MBFSDisk* disk);
MBFSDisk* mbfsCreateDisk(const char* device, UUID uuid, uint64_t ccount, uint8_t csp, uint8_t iesp, MBFSIOError* error);
MBFSDisk* mbfsOpenDisk(const char* device, MBFSIOError* error);
MBFSIOError mbfsCloseDisk(MBFSDisk* disk);
uint64_t mbfsioWriteCluster(MBFSDisk* disk, uint64_t cluster, const int8_t* buffer);
uint64_t mbfsioReadCluster(MBFSDisk* disk, uint64_t cluster, int8_t* buffer);

#pragma once
#include "mbfs.h"
#include "list.h"


typedef struct MBFSFileEntry MBFSFileEntry; // derives from MBFSEntry


typedef struct {
    const char* key;
    const int8_t* value;
    uint64_t valueSize;
} MBFSFileEntryExtension;


typedef struct {
    bool packed;
    ListNode* next;
    uint64_t i;
} MBFSFileEntryClusterIterator;


MBFSFileEntry* mbfsMakeFileEntry(const char* name, uint64_t size);
MBFSFileEntry* mbfsMakePackedFileEntry(const char* name, uint64_t size);
MBFSCoderError mbfsDecodeFileEntry(MBFSCoder* coder, MBFSFileEntry** entry, const int8_t* data, uint64_t size, uint8_t type);
bool mbfsFileEntryIsPacked(const MBFSFileEntry* entry);
bool mbfsFileEntryIsDirectory(const MBFSFileEntry* entry);
void mbfsFileEntryAddCluster(MBFSFileEntry* entry, uint64_t cluster);
void mbfsFileEntryAddClusterGroup(MBFSFileEntry* entry, uint64_t start, uint64_t count);
void mbfsFileEntryAddExtension(MBFSFileEntry* entry, MBFSFileEntryExtension extension);
uint64_t mbfsFileEntryGetSize(const MBFSFileEntry* entry);
void mbfsFileEntrySetSize(MBFSFileEntry* entry, uint64_t size);
const char* mbfsFileEntryGetName(const MBFSFileEntry* entry);
int32_t mbfsFileEntryGetNameHash(const MBFSFileEntry* entry);
uint64_t mbfsFileEntryGetClustersCount(const MBFSFileEntry* entry);
MBFSFileEntryClusterIterator mbfsFileEntryGetClusterIterator(const MBFSFileEntry* entry);
bool mbfsFileEntryClusterIteratorHasNext(MBFSFileEntryClusterIterator* iterator);
uint64_t mbfsFileEntryClusterIteratorNext(MBFSFileEntryClusterIterator* iterator);

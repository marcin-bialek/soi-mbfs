#include "mbfsi.h"
#include <stdio.h>
#include <stdlib.h>


typedef struct {
    MBFSDisk* disk;
    MBFSFileEntry* entry;
    MBFSIndexElementPointer p;
    MBFSFileEntryClusterIterator i;
    uint64_t position;
} MBFSFilePrivate;


static MBFSDisk* createDisk(const char* device, UUID uuid, uint8_t csp, uint8_t iesp, uint64_t size) {
    const uint64_t a = 1LL << csp;
    uint64_t ccount = (size + a - 1) / a;

    MBFSIOError error;
    return mbfsCreateDisk(device, uuid, ccount, csp, iesp, &error);
}


static MBFSDisk* openDisk(const char* device) {
    MBFSIOError error;
    return mbfsOpenDisk(device, &error);
}


static void closeDisk(MBFSDisk* disk) {
    mbfsCloseDisk(disk);
}


static MBFSFile* makeFileObject(MBFSDisk* disk, MBFSFileEntry* entry, MBFSIndexElementPointer p) {
    MBFSFile* file = malloc(sizeof(MBFSFile));
    file->__private = malloc(sizeof(MBFSFilePrivate));
    ((MBFSFilePrivate*)file->__private)->disk = disk;
    ((MBFSFilePrivate*)file->__private)->entry = entry;
    ((MBFSFilePrivate*)file->__private)->p = p;
    ((MBFSFilePrivate*)file->__private)->i = mbfsFileEntryGetClusterIterator(entry);
    ((MBFSFilePrivate*)file->__private)->position = 0;
    return file;
}


static MBFSFileIterator makeFileIterator(MBFSDisk* disk) {
    MBFSFileIterator iterator;
    MBFSFileEntry* rootIndex = mbfsioGetRootIndexFileEntry(disk);
    MBFSIndexElementPointer p = mbfsioMakeIndexElementPointer(rootIndex, 0);
    iterator.disk = disk;
    iterator.root = mbfsioGetRootIndexFileEntry(disk);
    iterator.i = mbfsioMakeIndexElementIterator(disk, p, MBFS_IEIM_NEXT_ENTRY);
    return iterator;
}


static MBFSFile* nextFile(MBFSFileIterator* iterator) {
    MBFSIndexElementPointer p = mbfsioIndexElementIteratorNext(&iterator->i);

    if(!p.valid) {
        return NULL;
    }
 
    MBFSFileEntry* entry = mbfsioDecodeMBFSFileEntry(iterator->disk, p);

    if(entry == NULL) {
        return NULL;
    }

    MBFSFile* file = makeFileObject(iterator->disk, entry, p);
    file->name = mbfsFileEntryGetName(entry);
    file->size = mbfsFileEntryGetSize(entry);
    file->diskSize = mbfsFileEntryGetClustersCount(entry) * mbfsioGetClusterSize(iterator->disk);
    return file;
}


static MBFSFile* openFile(MBFSDisk* disk, const char* name) {
    MBFSIndexElementPointer p;
    MBFSFileEntry* root = mbfsioGetRootIndexFileEntry(disk);
    MBFSFileEntry* entry = mbfsioFindAndDecodeFileEntry(disk, root, name, &p);

    if(entry == NULL) {
        return NULL;
    }

    MBFSFile* file = makeFileObject(disk, entry, p);
    file->name = mbfsFileEntryGetName(entry);
    file->size = mbfsFileEntryGetSize(entry);
    file->diskSize = mbfsFileEntryGetClustersCount(entry) * mbfsioGetClusterSize(disk);
    return file;
}


static void closeFile(MBFSFile* file) {
    mbfsDestroyEntry((MBFSEntry*)((MBFSFilePrivate*)file->__private)->entry);
    free(file->__private);
    free(file);
}


static void deleteFile(MBFSFile* file) {
    MBFSFileEntryClusterIterator i = mbfsFileEntryGetClusterIterator(((MBFSFilePrivate*)file->__private)->entry);

    while(mbfsFileEntryClusterIteratorHasNext(&i)) {
        uint64_t cluster = mbfsFileEntryClusterIteratorNext(&i);
        mbfsioFreeCluster(((MBFSFilePrivate*)file->__private)->disk, cluster);
    }

    mbfsioRemoveEntry(((MBFSFilePrivate*)file->__private)->disk, ((MBFSFilePrivate*)file->__private)->p);
    mbfsDestroyEntry(((MBFSFilePrivate*)file->__private)->entry);
    free(file->__private);
    free(file);
}


static uint64_t mbfsread(MBFSFile* file, void* buffer) {
    if(mbfsFileEntryClusterIteratorHasNext(&((MBFSFilePrivate*)file->__private)->i)) {
        uint64_t cluster = mbfsFileEntryClusterIteratorNext(&((MBFSFilePrivate*)file->__private)->i);
        mbfsioReadCluster(((MBFSFilePrivate*)file->__private)->disk, cluster, buffer);
        uint64_t cs = mbfsioGetClusterSize(((MBFSFilePrivate*)file->__private)->disk);

        if(file->size - ((MBFSFilePrivate*)file->__private)->position > cs) {
            ((MBFSFilePrivate*)file->__private)->position += cs;
            return cs;
        }
        else {
            uint64_t r = file->size - ((MBFSFilePrivate*)file->__private)->position;
            ((MBFSFilePrivate*)file->__private)->position = file->size;
            return r;
        }
    }

    return 0;
}


static uint64_t mbfswrite(MBFSFile* file, const void* buffer) {
    if(mbfsFileEntryClusterIteratorHasNext(&((MBFSFilePrivate*)file->__private)->i)) {
        uint64_t cluster = mbfsFileEntryClusterIteratorNext(&((MBFSFilePrivate*)file->__private)->i);
        mbfsioWriteCluster(((MBFSFilePrivate*)file->__private)->disk, cluster, buffer);
        uint64_t cs = mbfsioGetClusterSize(((MBFSFilePrivate*)file->__private)->disk);
        ((MBFSFilePrivate*)file->__private)->position += cs;
        return cs;
    }

    return 0;
}


static uint64_t extendFile(MBFSFile* file, uint64_t newSize) {
    uint64_t cs = mbfsioGetClusterSize(((MBFSFilePrivate*)file->__private)->disk);
    uint64_t a = (newSize + cs - 1) / cs;
    uint64_t b = (file->size + cs - 1) / cs;

    if(newSize > file->size) {
        if(a > b) {
            mbfsioAllocClustersForFileEntry(((MBFSFilePrivate*)file->__private)->disk, ((MBFSFilePrivate*)file->__private)->entry, a - b);
            ((MBFSFilePrivate*)file->__private)->i = mbfsFileEntryGetClusterIterator(((MBFSFilePrivate*)file->__private)->entry);
            file->diskSize = a * cs;
        }

        mbfsFileEntrySetSize(((MBFSFilePrivate*)file->__private)->entry, newSize);
        mbfsioRemoveEntry(((MBFSFilePrivate*)file->__private)->disk, ((MBFSFilePrivate*)file->__private)->p);
        mbfsioInsertEntry(((MBFSFilePrivate*)file->__private)->disk, ((MBFSFilePrivate*)file->__private)->p.file, ((MBFSFilePrivate*)file->__private)->entry, NULL);
        file->size = newSize;
        return newSize;
    }

    return file->size;
}


static MBFSFile* makeFile(MBFSDisk* disk, const char* name) {
    MBFSFile* file = openFile(disk, name);

    if(file) {
        return file;
    }

    MBFSFileEntry* entry = mbfsMakeFileEntry(name, 0);
    MBFSIndexElementPointer p;
    
    if(mbfsioInsertEntry(disk, mbfsioGetRootIndexFileEntry(disk), entry, &p)) {
        return NULL;
    }

    file = makeFileObject(disk, entry, p);
    file->name = name;
    file->size = 0;
    file->diskSize = 0;
    return file;
}


static MBFSFileEntryClusterIterator getClusterIterator(MBFSFile* file) {
    return mbfsFileEntryGetClusterIterator(((MBFSFilePrivate*)file->__private)->entry);
}


extern const struct MBFS MBFS = {
    .DEFAULT_ID = "00000000-0000-0000-0000-000000000000",
    .createDisk = createDisk,
    .openDisk = openDisk,
    .closeDisk = closeDisk,
    .makeFileIterator = makeFileIterator,
    .nextFile = nextFile,
    .openFile = openFile,
    .makeFile = makeFile,
    .closeFile = closeFile,
    .deleteFile = deleteFile,
    .read = mbfsread,
    .write = mbfswrite,
    .extendFile = extendFile,
    .getClusterIterator = getClusterIterator
};


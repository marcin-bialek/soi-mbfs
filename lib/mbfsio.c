#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "mbfsio.h"


typedef struct {
    int8_t* pointer;
    uint64_t cluster;
    bool dirty;
} Buffer;


typedef enum {
    CLUSTER_BUFFER = 0,
    INDEX_BUFFER = 1,
    FREE_BUFFER = 2
} BufferIndex;


struct MBFSDisk {
    int descriptor;
    uint64_t diskSize; // in clusters
    uint64_t clusterSize;
    uint64_t indexElementSize;
    uint64_t indexElementCapacity;
    uint64_t indexElementsPerCluster;
    MBFSFilesystemInfo fsInfo;
    MBFSFileEntry* rootIndexFile;
    MBFSFileEntry* freeFile;
    Buffer buffers[3]; // access ONLY with special functions
};


static void allocBuffers(MBFSDisk* disk) {
    for(int i = 0; i < 3; i++) {
        disk->buffers[i].pointer = malloc(disk->clusterSize);
        disk->buffers[i].cluster = 0;
    }
}


static inline off_t changeCluster(const MBFSDisk* disk, uint64_t cluster) {
    const off_t offset = cluster * disk->clusterSize;
    return lseek(disk->descriptor, offset, SEEK_SET) == offset ? offset : 0;
}


// Don't use it if you don't know how
static ssize_t storeBuffer(MBFSDisk* disk, BufferIndex bufferIndex, uint64_t cluster) {
    if(!disk->buffers[bufferIndex].dirty && disk->buffers[bufferIndex].cluster == cluster) {
        return disk->clusterSize;
    }

    if(changeCluster(disk, cluster) == 0) {
        return 0;
    }

    disk->buffers[bufferIndex].dirty = false;
    disk->buffers[bufferIndex].cluster = cluster;
    return write(disk->descriptor, disk->buffers[bufferIndex].pointer, disk->clusterSize);
}


// Read data from cluster into buffer
static ssize_t fetchBuffer(MBFSDisk* disk, BufferIndex bufferIndex, uint64_t cluster) {
    if(disk->buffers[bufferIndex].cluster == cluster) {
        return disk->clusterSize;
    }

    storeBuffer(disk, bufferIndex, disk->buffers[bufferIndex].cluster);

    if(changeCluster(disk, cluster) == 0) {
        return 0;
    }

    disk->buffers[bufferIndex].dirty = false;
    disk->buffers[bufferIndex].cluster = cluster;
    return read(disk->descriptor, disk->buffers[bufferIndex].pointer, disk->clusterSize);
}


static void freeBuffers(MBFSDisk* disk) {
    for(int i = 0; i < 3; i++) {
        storeBuffer(disk, i, disk->buffers[i].cluster);
        free(disk->buffers[i].pointer);
    }
}


static inline void clearBuffer(MBFSDisk* disk, BufferIndex bufferIndex) {
    storeBuffer(disk, bufferIndex, disk->buffers[bufferIndex].cluster);
    bzero(disk->buffers[bufferIndex].pointer, disk->clusterSize);
}


// Write data to buffer. If size == 0 wirte hole cluster
static inline void writeBuffer(MBFSDisk* disk, BufferIndex bufferIndex, uint64_t offset, const int8_t* data, size_t size) {
    storeBuffer(disk, bufferIndex, disk->buffers[bufferIndex].cluster);
    memcpy(disk->buffers[bufferIndex].pointer + offset, data, size == 0 ? disk->clusterSize - offset : size);
    disk->buffers[bufferIndex].dirty = true;
}


// Read data from buffer. If capacity == 0 read hole cluster
static inline void readBuffer(const MBFSDisk* disk, BufferIndex bufferIndex, uint64_t offset, int8_t* buffer, size_t capacity) {
    memcpy(buffer, disk->buffers[bufferIndex].pointer + offset, capacity == 0 ? disk->clusterSize - offset : capacity);
}


MBFSIndexElementPointer mbfsioMakeIndexElementPointer(MBFSFileEntry* indexFile, uint64_t elementIndex) {
    MBFSIndexElementPointer pointer;
    pointer.file = indexFile;
    pointer.index = elementIndex;
    return pointer;
}


MBFSIndexElementIterator mbfsioMakeIndexElementIterator(MBFSDisk* disk, MBFSIndexElementPointer start, MBFSIndexElementIteratorMode mode) {
    MBFSIndexElementIterator iterator;
    iterator.mode = mode;
    iterator.hasNext = true;
    iterator.disk = disk;
    iterator.next = start;
    return iterator;
}


MBFSIndexElementPointer mbfsioIndexElementIteratorNext(MBFSIndexElementIterator* iterator) {
    MBFSIndexElementPointer pointer;
    pointer.valid = false;
    pointer.file = NULL;
    pointer.index = 0;
    pointer.weakReference = NULL;

    if(iterator->hasNext == false) {
        return pointer;
    }

    MBFSFileEntryClusterIterator i = mbfsFileEntryGetClusterIterator(iterator->next.file);

    if(mbfsFileEntryClusterIteratorHasNext(&i) == false) {
        iterator->hasNext = false;
        return pointer;
    }

    MBFSIndexElement* reference = NULL;
    uint64_t index = iterator->next.index;
    uint64_t cluster = mbfsFileEntryClusterIteratorNext(&i);

    for(bool found = false; !found;) {
        while(index >= iterator->disk->indexElementsPerCluster) {
            if(!mbfsFileEntryClusterIteratorHasNext(&i)) {
                iterator->hasNext = false;
                iterator->next.valid = false;
                iterator->next.weakReference = NULL;
                return iterator->next;
            }

            cluster = mbfsFileEntryClusterIteratorNext(&i);
            index -= iterator->disk->indexElementsPerCluster;
        }

        fetchBuffer(iterator->disk, INDEX_BUFFER, cluster);
        reference = (void*)(iterator->disk->buffers[INDEX_BUFFER].pointer + 
                           (index * iterator->disk->indexElementSize));
        iterator->currentIndex = iterator->next.index;
        iterator->currentIndexInCluster = index;

        switch(iterator->mode) {
        case MBFS_IEIM_STEP:    
            iterator->next.index++;
            found = true;
            break;

        case MBFS_IEIM_NEXT_ENTRY:
            index++;
            iterator->next.index++;
            found = (reference->type != MBFS_EMPTY) &&
                    (reference->type != MBFS_CONTINUE) && 
                    (reference->type != MBFS_END);
            break;

        case MBFS_IEIM_SAME_ENTRY: 
            iterator->hasNext = (reference->type != MBFS_END) && 
                                (reference->next != iterator->next.index);
            iterator->next.index = reference->next;
            found = true;
            break;
        }
    }

    pointer.valid = true;
    pointer.file = iterator->next.file;
    pointer.index = iterator->currentIndex;
    pointer.weakReference = reference;
    return pointer;
}


uint64_t mbfsioAllocCluster(MBFSDisk* disk) {
    MBFSFileEntryClusterIterator i = mbfsFileEntryGetClusterIterator(disk->freeFile);

    if(mbfsFileEntryClusterIteratorHasNext(&i) == false) {
        return 0; // something is broken
    }

    const uint64_t firstCluster = mbfsFileEntryClusterIteratorNext(&i);
    uint64_t last, tableSize;
    fetchBuffer(disk, FREE_BUFFER, firstCluster);
    readBuffer(disk, FREE_BUFFER, 0, (void*)&last, 8);
    readBuffer(disk, FREE_BUFFER, 8, (void*)&tableSize, 8);

    if(tableSize == 0) {
        if(last >= disk->fsInfo.ccount - 1) {
            return 0; // out of space
        }

        last++;
        writeBuffer(disk, FREE_BUFFER, 0, (void*)&last, 8);
        return last;
    }    

    const uint64_t elementsPerCluster = disk->clusterSize >> 3;
    uint64_t cluster = firstCluster;
    uint64_t tableIndex = 2 + tableSize - 1;

    while(tableIndex >= elementsPerCluster) {
        if(!mbfsFileEntryClusterIteratorHasNext(&i)) {
            return 0; // something is broken
        }

        cluster = mbfsFileEntryClusterIteratorNext(&i);
        tableIndex -= elementsPerCluster;
    }

    tableSize--;
    writeBuffer(disk, FREE_BUFFER, 8, (void*)&tableSize, 8);
    fetchBuffer(disk, FREE_BUFFER, cluster);
    readBuffer(disk, FREE_BUFFER, 8 * tableIndex, (void*)&cluster, 8);
    return cluster;
}


uint64_t mbfsioFreeCluster(MBFSDisk* disk, uint64_t c) {
    MBFSFileEntryClusterIterator i = mbfsFileEntryGetClusterIterator(disk->freeFile);

    if(mbfsFileEntryClusterIteratorHasNext(&i) == false) {
        return 0; // something is broken
    }

    const uint64_t firstCluster = mbfsFileEntryClusterIteratorNext(&i);
    uint64_t last, tableSize;
    fetchBuffer(disk, FREE_BUFFER, firstCluster);
    readBuffer(disk, FREE_BUFFER, 0, (void*)&last, 8);
    readBuffer(disk, FREE_BUFFER, 8, (void*)&tableSize, 8);

    if(c == last) {
        last--;
        writeBuffer(disk, FREE_BUFFER, 0, (void*)&last, 8);
        return c;
    }    

    const uint64_t elementsPerCluster = disk->clusterSize >> 3;
    uint64_t cluster = firstCluster;
    uint64_t tableIndex = 2 + tableSize;

    while(tableIndex >= elementsPerCluster) {
        if(!mbfsFileEntryClusterIteratorHasNext(&i)) {
            return 0; // something is broken
        }

        cluster = mbfsFileEntryClusterIteratorNext(&i);
        tableIndex -= elementsPerCluster;
    }

    tableSize++;
    writeBuffer(disk, FREE_BUFFER, 8, (void*)&tableSize, 8);
    fetchBuffer(disk, FREE_BUFFER, cluster);
    writeBuffer(disk, FREE_BUFFER, 8 * tableIndex, (void*)&c, 8);
    return c;
}


uint64_t mbfsioAllocClustersForFileEntry(MBFSDisk* disk, MBFSFileEntry* file, uint64_t clustersCount) {
    for(uint64_t i = 0; i < clustersCount; i++) {
        const uint64_t cluster = mbfsioAllocCluster(disk);

        if(cluster == 0) {
            return i;
        }

        clearBuffer(disk, CLUSTER_BUFFER);
        storeBuffer(disk, CLUSTER_BUFFER, cluster);
        mbfsFileEntryAddCluster(file, cluster);
    }

    return clustersCount;
}


MBFSFileEntry* mbfsioDecodeIndexFileEntry(MBFSDisk* disk, uint64_t firstCluster) {
    MBFSFileEntry* tmp = mbfsMakeFileEntry(MBFS_INDEX, 0);
    mbfsFileEntryAddCluster(tmp, firstCluster);
    MBFSIndexElementPointer start = mbfsioMakeIndexElementPointer(tmp, 0);
    MBFSIndexElementIterator i = mbfsioMakeIndexElementIterator(disk, start, MBFS_IEIM_SAME_ENTRY);
    MBFSIndexElementPointer element;
    MBFSFileEntry* entry;
    MBFSCoderError error;
    MBFSCoder decoder = mbfsMakeCoder();
    uint64_t c = 1;

    do {
        if((element = mbfsioIndexElementIteratorNext(&i)).weakReference == NULL) {
            mbfsDestroyEntry(tmp);
            return NULL;
        }

        error = mbfsDecodeFileEntry(
            &decoder, &entry, 
            element.weakReference->value, 
            disk->indexElementCapacity, 
            element.weakReference->type
        );

        if(error == MBFSC_BROKEN_ENTRY) {
            mbfsDestroyEntry(tmp);
            return NULL;
        }

        MBFSFileEntryClusterIterator ci = mbfsFileEntryGetClusterIterator((MBFSFileEntry*)decoder.entry);

        for(uint64_t j = 1; mbfsFileEntryClusterIteratorHasNext(&ci); j++) {
            uint64_t cluster = mbfsFileEntryClusterIteratorNext(&ci);

            if(j > c) {
                mbfsFileEntryAddCluster(tmp, cluster);
                c++;
            }
        }
    } while(error == MBFSC_END_OF_BUFFER);    

    mbfsDestroyEntry(tmp);
    return entry;
}


MBFSFileEntry* mbfsioDecodeMBFSFileEntry(MBFSDisk* disk, MBFSIndexElementPointer start) {
    MBFSIndexElementIterator i = mbfsioMakeIndexElementIterator(disk, start, MBFS_IEIM_SAME_ENTRY);
    MBFSIndexElementPointer element;
    MBFSFileEntry* entry;
    MBFSCoderError error;
    MBFSCoder decoder = mbfsMakeCoder();

    do {
        if((element = mbfsioIndexElementIteratorNext(&i)).weakReference == NULL) {
            return NULL;
        }

        error = mbfsDecodeFileEntry(
            &decoder, &entry, 
            element.weakReference->value, 
            disk->indexElementCapacity, 
            element.weakReference->type
        );

        if(error == MBFSC_BROKEN_ENTRY) {
            return NULL;
        }
    } while(error == MBFSC_END_OF_BUFFER);    

    return entry;
}


void mbfsioRemoveEntry(MBFSDisk* disk, MBFSIndexElementPointer start) {
    MBFSIndexElementIterator i = mbfsioMakeIndexElementIterator(disk, start, MBFS_IEIM_SAME_ENTRY);
    MBFSIndexElementPointer element;

    while((element = mbfsioIndexElementIteratorNext(&i)).weakReference) {
        element.weakReference->type = MBFS_EMPTY;
        disk->buffers[INDEX_BUFFER].dirty = true;
    }  
}


int mbfsioInsertEntry(MBFSDisk* disk, MBFSFileEntry* indexFile, const MBFSEntry* entry, MBFSIndexElementPointer* ptr) {
    MBFSIndexElement* slot;
    MBFSIndexElementPointer nextSlot;
    MBFSIndexElement* element = malloc(disk->indexElementSize);
    element->type = MBFS_EMPTY;
    bool indexEntry = false;

    if(entry->type == MBFS_FILE || entry->type == MBFS_PFILE) {
        element->sum = mbfsFileEntryGetNameHash((MBFSFileEntry*)entry);
        const char* name = mbfsFileEntryGetName((MBFSFileEntry*)entry);
        indexEntry = strcmp(name, MBFS_INDEX) == 0;
    }

    MBFSCoderError error;
    MBFSCoder encoder = mbfsMakeCoder();
    MBFSIndexElementPointer p = mbfsioMakeIndexElementPointer(indexFile, 0);
    MBFSIndexElementIterator i = mbfsioMakeIndexElementIterator(disk, p, MBFS_IEIM_STEP);
    uint64_t cluster, nextCluster, firstIndex;

    do {
        if((nextSlot = mbfsioIndexElementIteratorNext(&i)).weakReference == NULL) {
            if(mbfsioAllocClustersForFileEntry(disk, indexFile, 1) != 1) {
                free(element);
                return 1;
            }

            p.index = 0;
            mbfsioRemoveEntry(disk, p);
            mbfsioInsertEntry(disk, indexFile, (MBFSEntry*)indexFile, NULL);

            p.index = i.currentIndex + 1;
            i = mbfsioMakeIndexElementIterator(disk, p, MBFS_IEIM_STEP);
            error = MBFSC_END_OF_BUFFER;
            continue;
        }

        if(nextSlot.weakReference->type != MBFS_EMPTY) {
            error = MBFSC_END_OF_BUFFER;
            continue;
        }

        if(!indexEntry && i.currentIndexInCluster == 0) {
            error = MBFSC_END_OF_BUFFER;
            continue;
        }

        nextCluster = disk->buffers[INDEX_BUFFER].cluster;

        if(element->type == MBFS_EMPTY) {
            firstIndex = i.currentIndex;
            element->type = entry->type;
        }
        else {
            fetchBuffer(disk, INDEX_BUFFER, cluster);
            element->next = i.currentIndex;
            memcpy(slot, element, disk->indexElementSize);
            disk->buffers[INDEX_BUFFER].dirty = true;

            if(element->type != MBFS_CONTINUE) {
                element->type = MBFS_CONTINUE;
            }
        }
        
        slot = nextSlot.weakReference;
        cluster = nextCluster;
        error = mbfsEncodeEntry(&encoder, entry, &element->value[0], disk->indexElementCapacity);
    } while(error != MBFSC_OK);

    element->next = firstIndex;

    if(element->type == MBFS_CONTINUE) { 
        element->type = MBFS_END; 
    } 

    fetchBuffer(disk, INDEX_BUFFER, cluster);
    memcpy(slot, element, disk->indexElementSize);
    disk->buffers[INDEX_BUFFER].dirty = true;

    if(ptr) {
        *ptr = mbfsioMakeIndexElementPointer(indexFile, firstIndex);
    }

    free(element);
    return 0;
}


MBFSFileEntry* mbfsioFindAndDecodeFileEntry(MBFSDisk* disk, MBFSFileEntry* indexFile, const char* name, MBFSIndexElementPointer* ptr) {
    MBFSIndexElementPointer p = mbfsioMakeIndexElementPointer(indexFile, 0);
    MBFSIndexElementIterator i = mbfsioMakeIndexElementIterator(disk, p, MBFS_IEIM_NEXT_ENTRY);
    const int32_t sum = mbfsHash((void*)name, strlen(name) + 1);

    while((p = mbfsioIndexElementIteratorNext(&i)).valid) {
        if(p.weakReference->sum == sum) {
            MBFSFileEntry* entry = mbfsioDecodeMBFSFileEntry(disk, p);

            if(entry && strcmp(name, mbfsFileEntryGetName(entry)) == 0) {
                if(ptr) *ptr = p;
                return entry;
            }
        }
    }

    return NULL;
}


static void fillFilesystemInfo(MBFSFilesystemInfo* info, int32_t version, const UUID* uuid, uint64_t ccount, uint64_t root, uint8_t csp, uint8_t iesp) {
    memcpy(info->magic, MBFS_MAGIC, 4);
    info->version = version;
    uuidToByteArray(uuid, info->uuid);
    info->ccount = ccount;
    info->root = root;
    info->csp = csp;
    info->iesp = iesp;
    info->sum = mbfsHash((void*)info, sizeof(MBFSFilesystemInfo) - sizeof(int32_t));
}


static void writeFilesystemInfo(MBFSDisk* disk) {
    clearBuffer(disk, CLUSTER_BUFFER);
    writeBuffer(disk, CLUSTER_BUFFER, 0, (void*)&disk->fsInfo, sizeof(MBFSFilesystemInfo));
    storeBuffer(disk, CLUSTER_BUFFER, 1);
    storeBuffer(disk, CLUSTER_BUFFER, 2);
}


MBFSDisk* mbfsCreateDisk(const char* device, UUID uuid, uint64_t ccount, uint8_t csp, uint8_t iesp, MBFSIOError* error) {
    if(csp < 9 || 63 < csp || iesp < 6 || csp - 1 < iesp) {
        *error = MBFSIO_WRONG_PARAMS;
        return NULL;
    }
    
    MBFSDisk* disk = malloc(sizeof(MBFSDisk));
    disk->diskSize = 0;
    disk->clusterSize = 1LL << csp;
    disk->indexElementSize = 1LL << iesp;
    disk->indexElementsPerCluster = 1LL << (csp - iesp);
    disk->indexElementCapacity = disk->indexElementSize - sizeof(MBFSIndexElement);
    fillFilesystemInfo(&disk->fsInfo, 1, &uuid, ccount, 3, csp, iesp);
    
    if((disk->descriptor = open(device, O_RDWR | O_CREAT | O_TRUNC, 0766)) == -1) {
        free(disk);
        *error = MBFSIO_FILE_ERROR;
        return NULL;
    }

    allocBuffers(disk);
    writeFilesystemInfo(disk);

    // make root $index file
    disk->rootIndexFile = mbfsMakeFileEntry(MBFS_INDEX, disk->clusterSize);
    mbfsFileEntryAddCluster(disk->rootIndexFile, 3);
    
    if(mbfsioInsertEntry(disk, disk->rootIndexFile, (MBFSEntry*)disk->rootIndexFile, NULL)) {
        mbfsDestroyEntry((MBFSEntry*)disk->rootIndexFile);
        freeBuffers(disk);
        close(disk->descriptor);
        free(disk);
        *error = MBFSIO_WRONG_PARAMS;
        return NULL;
    }

    // make $free file
    disk->freeFile = mbfsMakeFileEntry(MBFS_FREE, disk->clusterSize);
    mbfsFileEntryAddCluster(disk->freeFile, 4);
    
    if(mbfsioInsertEntry(disk, disk->rootIndexFile, (MBFSEntry*)disk->freeFile, NULL)) {
        mbfsCloseDisk(disk);
        *error = MBFSIO_WRONG_PARAMS;
        return NULL;
    }

    clearBuffer(disk, FREE_BUFFER);
    const uint64_t lastUsed = 4;
    writeBuffer(disk, FREE_BUFFER, 0, (void*)&lastUsed, 8);
    storeBuffer(disk, FREE_BUFFER, 4);

    // make $boot file 
    MBFSFileEntry* bootFile = mbfsMakeFileEntry(MBFS_BOOT, disk->clusterSize);
    mbfsFileEntryAddCluster(bootFile, 0);
    
    if(mbfsioInsertEntry(disk, disk->rootIndexFile, (MBFSEntry*)bootFile, NULL)) {
        mbfsDestroyEntry((MBFSEntry*)bootFile);
        mbfsCloseDisk(disk);
        *error = MBFSIO_WRONG_PARAMS;
        return NULL;
    }

    mbfsDestroyEntry((MBFSEntry*)bootFile);

    // make $fs_info file 
    MBFSFileEntry* fsInfoFile = mbfsMakeFileEntry(MBFS_FS_INFO, disk->clusterSize);
    mbfsFileEntryAddCluster(fsInfoFile, 1);
    
    if(mbfsioInsertEntry(disk, disk->rootIndexFile, (MBFSEntry*)fsInfoFile, NULL)) {
        mbfsDestroyEntry((MBFSEntry*)fsInfoFile);
        mbfsCloseDisk(disk);
        *error = MBFSIO_WRONG_PARAMS;
        return NULL;
    }

    mbfsDestroyEntry((MBFSEntry*)fsInfoFile);

    // make $fs_info_mirror file 
    MBFSFileEntry* fsInfoMirrorFile = mbfsMakeFileEntry(MBFS_FS_INFO_MIRROR, disk->clusterSize);
    mbfsFileEntryAddCluster(fsInfoMirrorFile, 2);
    
    if(mbfsioInsertEntry(disk, disk->rootIndexFile, (MBFSEntry*)fsInfoMirrorFile, NULL)) {
        mbfsDestroyEntry((MBFSEntry*)fsInfoMirrorFile);
        mbfsCloseDisk(disk);
        *error = MBFSIO_WRONG_PARAMS;
        return NULL;
    }

    mbfsDestroyEntry((MBFSEntry*)fsInfoMirrorFile);

    *error = MBFSIO_OK;
    return disk;
}


MBFSDisk* mbfsOpenDisk(const char* device, MBFSIOError* error) {
    int descriptor;

    if((descriptor = open(device, O_RDWR, 0766)) == -1) {
        *error = MBFSIO_FILE_ERROR;
        return NULL;
    }

    MBFSFilesystemInfo info;

    for(int i = 9; i < 60; i++) {
        const uint64_t offset = 1LL << i;

        if(lseek(descriptor, offset, SEEK_SET) != offset) {
            close(descriptor);
            *error = MBFSIO_BROKEN_FILESYSTEM;
            return NULL;
        }

        if(read(descriptor, (void*)&info, sizeof(info)) != sizeof(info)) {
            close(descriptor);
            *error = MBFSIO_BROKEN_FILESYSTEM;
            return NULL;
        }

        if(memcmp(&info.magic, MBFS_MAGIC, 4) != 0) {
            if(memcmp(&info.magic, MBFS_MAGIC_X, 4) != 0) {
                continue;
            }
        }

        if(info.sum == mbfsHash(&info, sizeof(info) - sizeof(int32_t))) {
            if(info.csp == i) {
                break;
            }
        }
    }

    if(memcmp(&info.magic, MBFS_MAGIC_X, 4) == 0) {
        close(descriptor);
        *error = MBFSIO_UNSUPPORTED_FILESYSTEM;
        return NULL;
    }

    MBFSDisk* disk = malloc(sizeof(MBFSDisk));
    disk->descriptor = descriptor;
    disk->fsInfo = info;
    disk->diskSize = 0;
    disk->clusterSize = 1LL << info.csp;
    disk->indexElementSize = 1LL << info.iesp;
    disk->indexElementsPerCluster = 1LL << (info.csp - info.iesp);
    disk->indexElementCapacity = disk->indexElementSize - sizeof(MBFSIndexElement);
    allocBuffers(disk);

    disk->rootIndexFile = mbfsioDecodeIndexFileEntry(disk, info.root);

    if(disk->rootIndexFile == NULL) {
        freeBuffers(disk);
        close(disk->descriptor);
        free(disk);
        *error = MBFSIO_BROKEN_FILESYSTEM;
        return NULL;
    }

    disk->freeFile = mbfsioFindAndDecodeFileEntry(disk, disk->rootIndexFile, MBFS_FREE, NULL);

    if(disk->freeFile == NULL) {
        mbfsDestroyEntry((MBFSEntry*)disk->rootIndexFile);
        freeBuffers(disk);
        close(disk->descriptor);
        free(disk);
        *error = MBFSIO_BROKEN_FILESYSTEM;
        return NULL;
    }

    *error = MBFSIO_OK;
    return disk;
}


MBFSIOError mbfsCloseDisk(MBFSDisk* disk) {
    mbfsDestroyEntry((MBFSEntry*)disk->rootIndexFile);
    mbfsDestroyEntry((MBFSEntry*)disk->freeFile);
    freeBuffers(disk);
    close(disk->descriptor);
    free(disk);
    return MBFSIO_OK;
}


MBFSFileEntry* mbfsioGetRootIndexFileEntry(MBFSDisk* disk) {
    return disk->rootIndexFile;
}


uint64_t mbfsioGetClusterSize(MBFSDisk* disk) {
    return disk->clusterSize;
}


uint64_t mbfsioWriteCluster(MBFSDisk* disk, uint64_t cluster, const int8_t* buffer) {
    fetchBuffer(disk, CLUSTER_BUFFER, cluster);
    writeBuffer(disk, CLUSTER_BUFFER, 0, buffer, disk->clusterSize);
    return disk->clusterSize;
}


uint64_t mbfsioReadCluster(MBFSDisk* disk, uint64_t cluster, int8_t* buffer) {
    fetchBuffer(disk, CLUSTER_BUFFER, cluster);
    readBuffer(disk, CLUSTER_BUFFER, 0, buffer, disk->clusterSize);
    return disk->clusterSize;
}


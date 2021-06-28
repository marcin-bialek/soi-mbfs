#include <string.h>
#include <stdlib.h>
#include "mbfsfileentry.h"


struct MBFSFileEntry { 
    MBFSEntry base;
    bool decoded;
    bool packed;
    bool directory;
    //MBFSIndexReference index; 
    uint64_t size;
    const char* name;
    List clusters; // if packed is false: list of uint64_t, otherwise: list of uint64_t[2] (start, count)
    List extensions; // list of MBFSFileEntryExtension
}; 


static void destroyFileEntry(MBFSFileEntry* entry) {
    if(entry->decoded) {
        free((void*)entry->name);
    }

    for(ListNode* n = entry->clusters.head; n; n = n->next) {
        free(n->value);
    }

    for(ListNode* n = entry->extensions.head; n; n = n->next) {
        if(entry->decoded) {
            free((void*)((MBFSFileEntryExtension*)n->value)->key);
            free((void*)((MBFSFileEntryExtension*)n->value)->value);
        }

        free(n->value);
    }
    
    free(entry);
}


static MBFSCoderError encodeFileEntry(MBFSCoder* coder, const MBFSFileEntry* entry, int8_t* buffer, uint64_t capacity) {
    switch(coder->state) {
    case 0: {
        coder->state = 1;
    }
    case 1: {
        if(vtypesEncodeVULONG(
                &coder->vtypesCoder, 
                entry->size, 
                buffer, capacity) == VTYPES_END_OF_BUFFER) {
            return MBFSC_END_OF_BUFFER;
        }

        coder->state = 2;
    }
    case 2: {
        if(vtypesEncodeSTRING(
                &coder->vtypesCoder, 
                entry->name, 
                buffer, capacity) == VTYPES_END_OF_BUFFER) {
            return MBFSC_END_OF_BUFFER;
        }

        coder->state = 3;
    }
    case 3: {
        if(vtypesEncodeVULONG(
                &coder->vtypesCoder, 
                entry->clusters.size, 
                buffer, capacity) == VTYPES_END_OF_BUFFER) {
            return MBFSC_END_OF_BUFFER;
        }

        coder->p = entry->clusters.head;
        coder->state = 4;
    }
    case 4: {
        while(coder->p) {
            if(entry->packed) {
                if(vtypesEncodeULONG(
                        &coder->vtypesCoder, 
                        (*(uint64_t (*)[2])((ListNode*)coder->p)->value)[0], 
                        buffer, capacity) == VTYPES_END_OF_BUFFER) {
                    return MBFSC_END_OF_BUFFER;
                }

                coder->state = 5;
    case 5:
                if(vtypesEncodeULONG(
                        &coder->vtypesCoder, 
                        (*(uint64_t (*)[2])((ListNode*)coder->p)->value)[1], 
                        buffer, capacity) == VTYPES_END_OF_BUFFER) {
                    return MBFSC_END_OF_BUFFER;
                }

                coder->state = 4;
            }
            else {
                if(vtypesEncodeULONG(
                        &coder->vtypesCoder, 
                        *(uint64_t*)((ListNode*)coder->p)->value, 
                        buffer, capacity) == VTYPES_END_OF_BUFFER) {
                    return MBFSC_END_OF_BUFFER;
                }
            }

            coder->p = ((ListNode*)coder->p)->next;
        }
        
        coder->state = 6;
    }
    case 6: {
        if(vtypesEncodeVULONG(
                &coder->vtypesCoder, 
                entry->extensions.size, 
                buffer, capacity) == VTYPES_END_OF_BUFFER) {
            return MBFSC_END_OF_BUFFER;
        }

        coder->p = entry->extensions.head;
        coder->state = 7;
    }
    case 7: {
        while(coder->p) {
            if(vtypesEncodeSTRING(
                    &coder->vtypesCoder, 
                    ((MBFSFileEntryExtension*)((ListNode*)coder->p)->value)->key, 
                    buffer, capacity) == VTYPES_END_OF_BUFFER) {
                return MBFSC_END_OF_BUFFER;
            }

            coder->state = 8;
    case 8:
            if(vtypesEncodeBYTEARRAY(
                    &coder->vtypesCoder, 
                    ((MBFSFileEntryExtension*)((ListNode*)coder->p)->value)->value, 
                    ((MBFSFileEntryExtension*)((ListNode*)coder->p)->value)->valueSize, 
                    buffer, capacity) == VTYPES_END_OF_BUFFER) {
                return MBFSC_END_OF_BUFFER;
            }

            coder->p = ((ListNode*)coder->p)->next;
            coder->state = 7;
        }
    }
    }

    coder->state = 0;
    return MBFSC_OK;
}


static MBFSFileEntry* makeFileEntry(const char* name, uint64_t size, bool packed, bool directory) {
    MBFSFileEntry* entry = malloc(sizeof(MBFSFileEntry));
    entry->base.type = directory ? (packed ? MBFS_PDIR : MBFS_DIR) : (packed ? MBFS_PFILE : MBFS_FILE);
    entry->base.destroy = (void*)destroyFileEntry;
    entry->base.encode = (void*)encodeFileEntry;
    entry->decoded = false;
    entry->packed = packed;
    entry->directory = directory;
    entry->size = size;
    entry->name = name;
    entry->clusters = listMake();
    entry->extensions = listMake();
    return entry;
}


inline MBFSFileEntry* mbfsMakeFileEntry(const char* name, uint64_t size) {
    return makeFileEntry(name, size, false, false);
}


inline MBFSFileEntry* mbfsMakePackedFileEntry(const char* name, uint64_t size) {
    return makeFileEntry(name, size, true, false);
}


MBFSCoderError mbfsDecodeFileEntry(MBFSCoder* coder, MBFSFileEntry** entry, const int8_t* data, uint64_t size, uint8_t type) { 
    switch(coder->state) {
    case 0: {
        coder->entry = malloc(sizeof(MBFSFileEntry));
        coder->entry->type = type;
        coder->entry->destroy = (void*)destroyFileEntry;
        coder->entry->encode = (void*)encodeFileEntry;
        ((MBFSFileEntry*)coder->entry)->decoded = true;
        ((MBFSFileEntry*)coder->entry)->decoded = false;
        ((MBFSFileEntry*)coder->entry)->packed = (type == MBFS_PFILE) || (type == MBFS_PDIR);
        ((MBFSFileEntry*)coder->entry)->directory = (type == MBFS_DIR) || (type == MBFS_PDIR);
        ((MBFSFileEntry*)coder->entry)->clusters = listMake();
        ((MBFSFileEntry*)coder->entry)->extensions = listMake();
        coder->state = 1;
    }
    case 1: {
        if(vtypesDecodeVULONG(
                &coder->vtypesCoder, 
                &((MBFSFileEntry*)coder->entry)->size, 
                data, size) == VTYPES_END_OF_BUFFER) {
            return MBFSC_END_OF_BUFFER; 
        }

        coder->state = 2;
    }
    case 2: {
        const VTypesError error = vtypesDecodeSTRING(
            &coder->vtypesCoder, 
            (char**)&((MBFSFileEntry*)coder->entry)->name, 
            data, size
        );

        if(error == VTYPES_END_OF_BUFFER) {
            return MBFSC_END_OF_BUFFER;       
        }
        else if(error == VTYPES_INCORRECT_STRING) {
            mbfsDestroyEntry(coder->entry);
            *entry = NULL;
            return MBFSC_BROKEN_ENTRY;
        }

        coder->state = 3;
    }
    case 3: {
        if(vtypesDecodeVULONG(
                &coder->vtypesCoder, 
                &coder->i, 
                data, size) == VTYPES_END_OF_BUFFER) {
            return MBFSC_END_OF_BUFFER; 
        }

        coder->state = 4;
    }
    case 4: {
        while(coder->i) {
            if(((MBFSFileEntry*)coder->entry)->packed) {
                if(vtypesDecodeULONG(
                        &coder->vtypesCoder, 
                        &coder->a, 
                        data, size) == VTYPES_END_OF_BUFFER) {
                    return MBFSC_END_OF_BUFFER;
                }

                coder->state = 5;
    case 5:
                if(vtypesDecodeULONG(
                        &coder->vtypesCoder, 
                        &coder->b, 
                        data, size) == VTYPES_END_OF_BUFFER) {
                    return MBFSC_END_OF_BUFFER;
                }

                mbfsFileEntryAddClusterGroup((void*)coder->entry, coder->a, coder->b);
                coder->state = 4;
            }
            else {
                if(vtypesDecodeULONG(
                        &coder->vtypesCoder, 
                        &coder->a, 
                        data, size) == VTYPES_END_OF_BUFFER) {
                    return MBFSC_END_OF_BUFFER;
                }

                mbfsFileEntryAddCluster((void*)coder->entry, coder->a);
            }

            coder->i--;
        }

        coder->state = 6;
    }
    case 6: {
        if(vtypesDecodeVULONG(
                &coder->vtypesCoder, 
                &coder->i, 
                data, size) == VTYPES_END_OF_BUFFER) {
            return MBFSC_END_OF_BUFFER; 
        }

        coder->state = 7;
    }
    case 7: {
        while(coder->i) {
            const VTypesError error = vtypesDecodeSTRING(
                &coder->vtypesCoder, 
                (char**)&coder->p, 
                data, size
            );

            if(error == VTYPES_END_OF_BUFFER) {
                return MBFSC_END_OF_BUFFER;       
            }
            else if(error == VTYPES_INCORRECT_STRING) {
                mbfsDestroyEntry(coder->entry);
                *entry = NULL;
                return MBFSC_BROKEN_ENTRY;
            }

            coder->state = 8;
    case 8:
            if(vtypesDecodeBYTEARRAY(
                    &coder->vtypesCoder, 
                    (int8_t**)&coder->q, 
                    &coder->a, 
                    data, size) == VTYPES_END_OF_BUFFER) {
                return MBFSC_END_OF_BUFFER;  
            }

            mbfsFileEntryAddExtension((void*)coder->entry, (MBFSFileEntryExtension){
                coder->p, coder->q, coder->a
            });

            coder->i--;
            coder->state = 7;
        }
    }
    }

    *entry = (MBFSFileEntry*)coder->entry;
    coder->state = 0;
    return MBFSC_OK;
}


inline bool mbfsFileEntryIsPacked(const MBFSFileEntry* entry) {
    return entry->packed;
}


inline bool mbfsFileEntryIsDirectory(const MBFSFileEntry* entry) {
    return entry->directory;
}


void mbfsFileEntryAddCluster(MBFSFileEntry* entry, uint64_t cluster) {
    if(entry->packed) {
        mbfsFileEntryAddClusterGroup(entry, cluster, 1);
        return;
    }

    uint64_t* value = malloc(sizeof(uint64_t));
    *value = cluster;
    listAppend(&entry->clusters, value);
}


void mbfsFileEntryAddClusterGroup(MBFSFileEntry* entry, uint64_t start, uint64_t count) {
    if(!entry->packed) {
        for(uint64_t i = 0; i < count; i++) {
            mbfsFileEntryAddCluster(entry, start + i);
        }

        return;
    }

    uint64_t (*value)[2] = malloc(16);
    (*value)[0] = start;
    (*value)[1] = count;
    listAppend(&entry->clusters, value);
}


void mbfsFileEntryAddExtension(MBFSFileEntry* entry, MBFSFileEntryExtension extension) {
    MBFSFileEntryExtension* value = malloc(sizeof(MBFSFileEntryExtension));
    *value = extension;
    listAppend(&entry->extensions, value);
}


inline uint64_t mbfsFileEntryGetSize(const MBFSFileEntry* entry) {
    return entry->size;
}


inline void mbfsFileEntrySetSize(MBFSFileEntry* entry, uint64_t size) {
    entry->size = size;
}


inline const char* mbfsFileEntryGetName(const MBFSFileEntry* entry) {
    return entry->name;
}


inline int32_t mbfsFileEntryGetNameHash(const MBFSFileEntry* entry) {
    return mbfsHash((void*)entry->name, strlen(entry->name) + 1);
}


inline uint64_t mbfsFileEntryGetClustersCount(const MBFSFileEntry* entry) {
    return entry->clusters.size;
}


inline MBFSFileEntryClusterIterator mbfsFileEntryGetClusterIterator(const MBFSFileEntry* entry) {
    MBFSFileEntryClusterIterator iterator;
    iterator.packed = entry->packed;
    iterator.next = entry->clusters.head;
    iterator.i = 0;
    return iterator;
}


inline bool mbfsFileEntryClusterIteratorHasNext(MBFSFileEntryClusterIterator* iterator) {
    return iterator->next != NULL;
}


uint64_t mbfsFileEntryClusterIteratorNext(MBFSFileEntryClusterIterator* iterator) {
    if(iterator->next == NULL) {
        return 0;
    }

    if(iterator->packed == false) {
        uint64_t c = *(uint64_t*)iterator->next->value;
        iterator->next = iterator->next->next;
        return c;
    }

    const uint64_t (*group)[2] = iterator->next->value;

    if(iterator->i < (*group)[1]) {
        uint64_t c = (*group)[0] + iterator->i;
        iterator->i++;
        return c;
    }

    iterator->next = iterator->next->next;
    iterator->i = 0;
    return mbfsFileEntryClusterIteratorNext(iterator);
}

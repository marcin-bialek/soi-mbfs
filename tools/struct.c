#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "mbfsi.h"
#include "list.h"


static void _Noreturn printHelpAndExit() {
    printf("Użycie:  mbfs-struct [-h] \x1b[4mfilename\x1b[0m\n\n");

    printf("Wyświetla strukturę wirtualnego dysku MBFS \x1b[4mfilename\x1b[0m w formacie:\n");
    printf("[adres klastra] [rozmiar] [stan] [nazwa pliku, do którego należy zajęty klaster]\n\n");

    printf("Opcje:\n");
    printf("    -h         Wyświetla pomoc.\n");
    printf("\n");

    exit(0);
}


static void dostruct(MBFSDisk* disk) {
    MBFSFile* freeFile = MBFS.openFile(disk, "$free");
    uint64_t* buffer = malloc(mbfsioGetClusterSize(disk));
    MBFS.read(freeFile, buffer);
    const uint64_t lastUsed = buffer[0];
    free(buffer);
    MBFS.closeFile(freeFile);

    char** clusters = malloc((lastUsed + 1) * sizeof(char*));
    bzero(clusters, (lastUsed + 1) * sizeof(char*));
    const uint64_t cs = mbfsioGetClusterSize(disk);
    List names = listMake();

    MBFSFileIterator i = MBFS.makeFileIterator(disk);
    MBFSFile* file;
    uint64_t c = 0;

    while((file = MBFS.nextFile(&i))) {
        char* n = malloc(strlen(file->name) + 1);
        memcpy(n, file->name, strlen(file->name) + 1);
        listAppend(&names, n);

        MBFSFileEntryClusterIterator ci = MBFS.getClusterIterator(file);

        while(mbfsFileEntryClusterIteratorHasNext(&ci)) {
            uint64_t x = mbfsFileEntryClusterIteratorNext(&ci);

            if(x <= lastUsed) {
                clusters[x] = n;
            }
        }
        
        MBFS.closeFile(file);
    }

    for(uint64_t j = 0; j <= lastUsed; j++) {
        printf("%-9llu %-9llu %-6s %s\n", j * cs, cs, clusters[j] ? "zajęty" : "wolny", clusters[j] ? clusters[j] : "");
    }

    for(ListNode* p = names.head; p; p = p->next) {
        free(p->value);
    }

    listDestroy(&names);
}


int main(int argc, char* argv[]) {
    int option;

    while((option = getopt(argc, argv, "h")) != -1) {
        switch(option) {
        case 'h':
        default: 
            printHelpAndExit();
        }
    }

    if(argc != optind + 1) {
        printHelpAndExit();
    }

    MBFSDisk* disk = MBFS.openDisk(argv[optind]);

    if(disk == NULL) {
        printf("Błąd podczas otwierania dysku.\n");
        return 0;
    }

    dostruct(disk);
    MBFS.closeDisk(disk);
    return 0;
}


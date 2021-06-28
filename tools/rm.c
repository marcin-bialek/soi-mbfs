#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "mbfsi.h"


static void _Noreturn printHelpAndExit() {
    printf("Użycie:  mbfs-rm [-h] \x1b[4mdisk\x1b[0m \x1b[4mname\x1b[0m\n\n");

    printf("Usuwa plik \x1b[4mname\x1b[0m z wirtualnego dysku MBFS \x1b[4mdisk\x1b[0m.\n\n");

    printf("Opcje:\n");
    printf("    -h         Wyświetla pomoc.\n");
    printf("\n");

    exit(0);
}


static void rm(MBFSDisk* disk, const char* filename) {
    MBFSFile* file = MBFS.openFile(disk, filename);

    if(file) {
        MBFS.deleteFile(file);
    }    
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

    if(argc != optind + 2) {
        printHelpAndExit();
    }

    MBFSDisk* disk = MBFS.openDisk(argv[optind]);

    if(disk == NULL) {
        printf("Błąd podczas otwierania dysku.\n");
        return 0;
    }

    rm(disk, argv[optind + 1]);
    MBFS.closeDisk(disk);
    return 0;
}


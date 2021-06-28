#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "mbfsi.h"


static void _Noreturn printHelpAndExit() {
    printf("Użycie:  mbfs-ls [-h] [-s] \x1b[4mdisk\x1b[0m\n\n");

    printf("Wyświetla zawartość wirtualnego dysku MBFS \x1b[4mdisk\x1b[0m\n");
    printf("w formacie:\n");
    printf("[rozmiar] [rozmiar na dysku] [nazwa]\n\n");

    printf("Opcje:\n");
    printf("    -h         Wyświetla pomoc.\n");
    printf("    -s         Pokaż pliki systemowe.\n");
    printf("\n");

    exit(0);
}


static void list(MBFSDisk* disk, bool s) {
    MBFSFileIterator i = MBFS.makeFileIterator(disk);
    MBFSFile* file;

    while((file = MBFS.nextFile(&i))) {
        if(s || file->name[0] != '$') {
            printf("%-9llu %-9llu %s\n", file->size, file->diskSize, file->name);
        }

        MBFS.closeFile(file);
    }
}


int main(int argc, char* argv[]) {
    int option;
    bool s = false;

    while((option = getopt(argc, argv, "hs")) != -1) {
        switch(option) {
        case 's':
            s = true;
            break;
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

    list(disk, s);
    MBFS.closeDisk(disk);
    return 0;
}


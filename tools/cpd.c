#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "mbfsi.h"


static void _Noreturn printHelpAndExit() {
    printf("Użycie:  mbfs-cpd [-h] \x1b[4mdisk\x1b[0m \x1b[4msource_file\x1b[0m \x1b[4m...\x1b[0m\n\n");

    printf("Kopiuje plik/pliki \x1b[4msource_file\x1b[0m na wirtualny dysk MBFS \x1b[4mdisk\x1b[0m\n\n");
 
    printf("Opcje:\n");
    printf("    -h         Wyświetla pomoc.\n");
    printf("\n\n");

    printf("Przykłady użycia:\n\n");

    printf("    mbfs-cpd \x1b[4mdisk.bin\x1b[0m \x1b[4mfile1.txt\x1b[0m\n");
    printf("        Kopiuje plik \x1b[4mfile1.txt\x1b[0m na dysk \x1b[4mdisk.bin\x1b[0m.\n\n");

    printf("    mbfs-cpd \x1b[4mdisk.bin\x1b[0m \x1b[4mfile1.txt\x1b[0m \x1b[4mfile2.txt\x1b[0m\n");
    printf("        Kopiuje pliki \x1b[4mfile1.txt\x1b[0m i \x1b[4mfile2.txt\x1b[0m na dysku \x1b[4mdisk.bin\x1b[0m.\n\n");

    exit(0);
}


static int copyOneToDisk(MBFSDisk* disk, const char* name) {
    MBFSFile* t = MBFS.makeFile(disk, name);

    if(t == NULL) {
        return 1;
    }

    int s = open(name, O_RDONLY);

    if(s < 0) {
        MBFS.closeFile(t);
        return 1;
    }

    int64_t size = lseek(s, 0, SEEK_END);
    MBFS.extendFile(t, size);
    lseek(s, 0, SEEK_SET);
    uint64_t cs = mbfsioGetClusterSize(disk);
    void* buffer = malloc(cs);

    while(size > 0 && read(s, buffer, size > cs ? cs : size) > 0) {
        MBFS.write(t, buffer);
        size -= cs;
    }

    free(buffer);
    MBFS.closeFile(t);
    close(s);
    return 0;
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

    if(argc < optind + 2) {
        printHelpAndExit();
    }

    MBFSDisk* disk = MBFS.openDisk(argv[optind]);

    if(disk == NULL) {
        printf("Błąd podczas otwierania dysku %s.\n", argv[optind]);
        return 1;
    }

    while(++optind < argc) {
        copyOneToDisk(disk, argv[optind]);
    }
    
    MBFS.closeDisk(disk);
    return 0;
}


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "mbfsi.h"


static void _Noreturn printHelpAndExit() {
    printf("Użycie:  mbfs-cph [-h] \x1b[4mdisk\x1b[0m \x1b[4msource_file\x1b[0m \x1b[4mtarget_file\x1b[0m\n");
    printf("         mbfs-cph [-h] \x1b[4mdisk\x1b[0m \x1b[4msource_file\x1b[0m \x1b[4m...\x1b[0m \x1b[4mtarget_directory\x1b[0m\n\n");

    printf("Kopiuje plik/pliki \x1b[4msource_file\x1b[0m z wirtualnego dysku MBFS \x1b[4mdisk\x1b[0m.\n");
    printf("do pliku/katalogu \x1b[4mtarget_file\x1b[0m/\x1b[4mtarget_directory\x1b[0m\n\n");
 
    printf("Opcje:\n");
    printf("    -h         Wyświetla pomoc.\n");
    printf("\n\n");

    printf("Przykłady użycia:\n\n");

    printf("    mbfs-cph \x1b[4mdisk.bin\x1b[0m \x1b[4mfile1.txt\x1b[0m \x1b[4mcopy.txt\x1b[0m\n");
    printf("        Kopiuje plik \x1b[4mfile1.txt\x1b[0m z dysku \x1b[4mdisk.bin\x1b[0m do pliku \x1b[4mcopy.txt\x1b[0m.\n\n");

    printf("    mbfs-cph \x1b[4mdisk.bin\x1b[0m \x1b[4mfile1.txt\x1b[0m \x1b[4mfile2.txt\x1b[0m \x1b[4mdir\x1b[0m\n");
    printf("        Kopiuje pliki \x1b[4mfile1.txt\x1b[0m i \x1b[4mfile2.txt\x1b[0m z dysku \x1b[4mdisk.bin\x1b[0m do folderu \x1b[4mdir\x1b[0m.\n\n");

    exit(0);
}


static int copyOneFromDisk(MBFSDisk* disk, const char* source, const char* target) {
    MBFSFile* s = MBFS.openFile(disk, source);

    if(s == NULL) {
        return 1;
    }

    int t = open(target, O_CREAT | O_TRUNC | O_WRONLY, 0766);

    if(t < 0) {
        MBFS.closeFile(s);
        return 1;
    }

    void* buffer = malloc(mbfsioGetClusterSize(disk));
    uint64_t r;

    while((r = MBFS.read(s, buffer)) > 0) {
        write(t, buffer, r);
    }
    
    free(buffer);
    MBFS.closeFile(s);
    close(t);
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

    if(argc < optind + 3) {
        printHelpAndExit();
    }

    MBFSDisk* disk = MBFS.openDisk(argv[optind]);

    if(disk == NULL) {
        printf("Błąd podczas otwierania dysku %s.\n", argv[optind]);
        return 1;
    }

    bool td = argc > optind + 3; 

    while(++optind < argc - 1) {
        if(td) {
            char* target = malloc(strlen(argv[argc - 1]) + strlen(argv[optind]) + 3);
            strcat(target, argv[argc - 1]);
            strcat(target, "/");
            strcat(target, argv[optind]);
            copyOneFromDisk(disk, argv[optind], target);
        }
        else {
            copyOneFromDisk(disk, argv[optind], argv[argc - 1]);
        }
    }
    
    MBFS.closeDisk(disk);
    return 0;
}


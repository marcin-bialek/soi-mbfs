#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "uuid.h"
#include "mbfsi.h"


static void _Noreturn printHelpAndExit() {
    printf("Użycie:  mbfs-mkfs [-c \x1b[4mcsp\x1b[0m] [-e \x1b[4miesp\x1b[0m] [-h] [-i \x1b[4muuid\x1b[0m] \x1b[4mfilename\x1b[0m \x1b[4msize\x1b[0m\n\n");

    printf("Tworzy wirtualny dysk z systemem plików MBFS o maksymalnej\n");
    printf("pojemności \x1b[4msize\x1b[0m (w KiB) i zapisuje go w pliku \x1b[4mfilename\x1b[0m.\n\n");
    
    printf("Opcje:\n");
    printf("    -c \x1b[4mcsp\x1b[0m     Ustawia współczynnik rozmiaru jednego klastra na csp\n");
    printf("               (rozmiar klastra = 2^csp).\n");
    printf("               Miniumum:  9\n");
    printf("               Domyślnie: 12\n");
    printf("    -e \x1b[4miesp\x1b[0m    Ustawia współczynnik rozmiaru jednego elementu tablicy\n");
    printf("               pliku $index na iesp. (rozmiar elementu = 2^iesp).\n");
    printf("               Miniumum:  6\n");
    printf("               Maksimum:  \x1b[4mcsp\x1b[0m - 1\n");
    printf("               Domyślnie: 6\n");
    printf("    -h         Wyświetla pomoc.\n");
    printf("    -i \x1b[4muuid\x1b[0m    Ustawia identyfikator dysku na uuid.\n");
    printf("               Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n");
    printf("               x - cyfra heksadecymalna (0-F)\n");
    printf("\n");
    exit(0);
}


int main(int argc, char* argv[]) {
    int option;
    char* id = NULL; 
    uint64_t csp = 12;
    uint64_t iesp = 6;

    while((option = getopt(argc, argv, "hi:c:e:")) != -1) {
        switch(option) {
        case 'i':
            id = optarg;
            break;
        case 'c':
            csp = strtoul(optarg, NULL, 0);
            break;
        case 'e':
            iesp = strtoul(optarg, NULL, 0);
            break;
        case 'h':
        default: 
            printHelpAndExit();
        }
    }

    if(argc != optind + 2) {
        printHelpAndExit();
    }

    UUID uuid;
    const uint64_t size = strtoull(argv[optind + 1], NULL, 0) << 10;
    const char* filename = argv[optind];

    if(uuidFromString(&uuid, id ? id : MBFS.DEFAULT_ID) != 0) {
        printf("Błąd: nieprawny format identyfikatora.\n");
        return 0;
    }

    if(csp < 9 || csp > 255) {
        printf("Błąd: niepoprawna wartość csp.\n");
        return 0;
    }

    if(iesp < 6 || iesp > csp - 1) {
        printf("Błąd: niepoprawna wartość iesp.\n");
        return 0;
    }

    MBFSDisk* disk = MBFS.createDisk(filename, uuid, csp, iesp, size);

    if(disk == NULL) {
        printf("Błąd podczas tworzenia dysku.\n");
        return 0;
    }

    MBFS.closeDisk(disk);
    return 0;
}


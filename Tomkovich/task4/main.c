#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    FILE *file;
    uid_t realUID, effectiveUID;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return EXIT_FAILURE;
    }

    realUID = getuid();
    effectiveUID = geteuid();

    printf("Real UID: %ld\n", (long)realUID);
    printf("Effective UID: %ld\n", (long)effectiveUID);

    file = fopen(argv[1], "r+");

    if (file == NULL) {
        perror("fopen");
    } else {
        printf("File opened successfully\n");
        fclose(file);
    }

    if (setuid(realUID) == -1) {
        perror("setuid");
        return EXIT_FAILURE;
    }

    realUID = getuid();
    effectiveUID = geteuid();

    printf("\nReal UID: %ld\n", (long)realUID);
    printf("Effective UID: %ld\n", (long)effectiveUID);

    file = fopen(argv[1], "r+");

    if (file == NULL) {
        perror("fopen");
    } else {
        printf("File opened successfully\n");
        fclose(file);
    }

    return EXIT_SUCCESS;
}

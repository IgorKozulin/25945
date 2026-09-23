#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    FILE *file;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("real=%ld effective=%ld\n", (long)getuid(), (long)geteuid());
    file = fopen(argv[1], "r+");
    if (file == NULL)
        perror("fopen");
    else {
        puts("file opened");
        fclose(file);
    }

    if (setuid(getuid()) == -1) {
        perror("setuid");
        return EXIT_FAILURE;
    }

    printf("real=%ld effective=%ld\n", (long)getuid(), (long)geteuid());
    file = fopen(argv[1], "r+");
    if (file == NULL)
        perror("fopen");
    else {
        puts("file opened");
        fclose(file);
    }

    return EXIT_SUCCESS;
}

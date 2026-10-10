#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

void print_uids() {
    printf("Real UID: %d, Effective UID: %d\n", getuid(), geteuid());
}

void try_open_file() {
    FILE *f = fopen("data.txt", "r");
    if (f == NULL) {
        perror("Error");
    } else {
        printf("Success\n");
        fclose(f);
    }
}

int main() {
    print_uids();
    try_open_file();

    if (setuid(getuid()) != 0) {
        perror("setuid error");
        return EXIT_FAILURE;
    }

    print_uids();
    try_open_file();

    return 0;
}


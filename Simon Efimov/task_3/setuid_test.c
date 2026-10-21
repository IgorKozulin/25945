#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

void print_uids(const char *label) {
    printf("%s: Real UID=%d, Effective UID=%d\n",
           label, getuid(), geteuid());
}

int try_open(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        perror("fopen");
        return -1;
    }
    printf("Opened %s successfully\n", filename);
    fclose(f);
    return 0;
}

int main(void) {
    printf("=== Before dropping privileges ===\n");
    print_uids("Before");
    try_open("data.txt");

    printf("\n=== Dropping privileges (setuid(getuid())) ===\n");
    if (setuid(getuid()) == -1) {
        perror("setuid");
        return EXIT_FAILURE;
    }

    print_uids("After");
    try_open("data.txt");

    return 0;
}

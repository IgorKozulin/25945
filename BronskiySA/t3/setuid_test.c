#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
static void check(const char *path) {
    printf("UID=%lu EUID=%lu\n", (unsigned long)getuid(), (unsigned long)geteuid());
    FILE *file = fopen(path, "r");
    if (!file) perror(path);
    else { puts("File opened successfully"); if (fclose(file)) perror("fclose"); }
}
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Usage: setuid_test file\n"); return 1; }
    check(argv[1]);
    if (setuid(getuid()) < 0) { perror("setuid"); return 1; }
    check(argv[1]); return 0;
}

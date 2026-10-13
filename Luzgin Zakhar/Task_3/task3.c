#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

static void check_file(const char *path, const char *stage)
{
    FILE *file;
    printf("\n%s\n", stage);
    printf("Real UID: %lu; Effective UID: %lu\n",
           (unsigned long)getuid(), (unsigned long)geteuid());

    file = fopen(path, "r");
    if (file == NULL) {
        perror("fopen");
        return;
    }
    printf("Файл '%s' успешно открыт\n", path);
    if (fclose(file) == EOF)
        perror("fclose");
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "private_data.txt";
    uid_t real_uid = getuid();

    if (argc > 2) {
        fprintf(stderr, "Использование: %s [путь_к_файлу]\n", argv[0]);
        return EXIT_FAILURE;
    }

    check_file(path, "До вызова setuid(getuid()):");
    if (setuid(real_uid) == -1) {
        perror("setuid");
        return EXIT_FAILURE;
    }
    check_file(path, "После вызова setuid(getuid()):");
    return EXIT_SUCCESS;
}

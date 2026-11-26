#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#define TARGET "data.txt"

static void check_access(void)
{
    FILE *stream;

    printf("uid=%d euid=%d\n", (int)getuid(), (int)geteuid());

    stream = fopen(TARGET, "r");
    if (stream == NULL) {
        perror("fopen");
    } else {
        puts("файл открыт");
        fclose(stream);
    }
}

int main(void)
{
    check_access();

    if (setuid(getuid()) == -1) {
        perror("setuid");
    }

    check_access();
    return 0;
}

#include <stdio.h>
#include <unistd.h>

int main(void)
{
    FILE *f;

    printf("Real UID: %d, Effective UID: %d\n",
           getuid(), geteuid());

    f = fopen("file", "r");

    if (f == NULL)
        perror("fopen");
    else {
        printf("File opened successfully\n");
        fclose(f);
    }

    setuid(getuid());

    printf("Real UID: %d, Effective UID: %d\n",
           getuid(), geteuid());

    f = fopen("file", "r");

    if (f == NULL)
        perror("fopen");
    else {
        printf("File opened successfully\n");
        fclose(f);
    }

    return 0;
}

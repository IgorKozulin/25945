#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define MAX_LINES 1000

volatile sig_atomic_t timeout = 0;

void alarm_handler(int sig)
{
    timeout = 1;
}

int main(int argc, char *argv[])
{
    int fd;
    int n;
    int count = 0;

    off_t offset[MAX_LINES];
    int length[MAX_LINES];

    struct stat st;
    char *data;

    if (argc != 2) {
        printf("Usage: %s file\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
        perror("open");
        return 1;
    }

    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return 1;
    }

    if (st.st_size == 0) {
        printf("File is empty\n");
        close(fd);
        return 0;
    }

    data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    offset[0] = 0;

    for (int i = 0; i < st.st_size; i++) {
        if (data[i] == '\n') {
            length[count] = i - offset[count];
            count++;

            if (count < MAX_LINES)
                offset[count] = i + 1;
        }
    }

    if (offset[count] < st.st_size) {
        length[count] = st.st_size - offset[count];
        count++;
    }

    struct sigaction sa = {0};
    sa.sa_handler = alarm_handler;
    sigaction(SIGALRM, &sa, NULL);

    while (1) {
        timeout = 0;

        printf("Enter line number (0 to exit): ");
        fflush(stdout);

        alarm(5);

        if (scanf("%d", &n) != 1) {
            if (timeout) {
                printf("\n");
                printf("%.*s", (int)st.st_size, data);
            }
            break;
        }

        alarm(0);

        if (n == 0)
            break;

        if (n < 1 || n > count) {
            printf("No such line\n");
            continue;
        }

        printf("%.*s\n",
               length[n - 1],
               data + offset[n - 1]);
    }

    munmap(data, st.st_size);
    close(fd);

    return 0;
}

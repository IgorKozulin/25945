#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_LINES 1000

int main(int argc, char *argv[])
{
    int fd;
    char c;
    int n;
    int count = 0;
    int len = 0;
    off_t offset[MAX_LINES];
    int length[MAX_LINES];

    if (argc != 2) {
        printf("Usage: %s file\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    offset[0] = 0;

    while (read(fd, &c, 1) == 1) {
        if (c == '\n') {
            length[count] = len;
            count++;
            len = 0;

            if (count < MAX_LINES)
                offset[count] = lseek(fd, 0L, 1);
        } else {
            len++;
        }
    }

    if (len > 0) {
        length[count] = len;
        count++;
    }

    while (1) {
        printf("Enter line number (0 to exit): ");
        scanf("%d", &n);

        if (n == 0)
            break;

        if (n < 1 || n > count) {
            printf("No such line\n");
            continue;
        }

        char *buf = malloc(length[n - 1] + 1);

        lseek(fd, offset[n - 1], 0);
        read(fd, buf, length[n - 1]);

        buf[length[n - 1]] = '\0';

        printf("%s\n", buf);

        free(buf);
    }

    close(fd);

    return 0;
}

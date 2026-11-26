#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#define LINES_MAX 1000
#define CHUNK 512

int main(int argc, char *argv[])
{
    int fd;
    off_t begin[LINES_MAX];
    int size[LINES_MAX];
    int cnt = 0;
    char block[CHUNK];
    ssize_t rd;
    off_t cur = 0;
    off_t line_start = 0;
    int k, want, res;
    char *text;

    if (argc != 2) {
        printf("Использование: %s файл\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    while ((rd = read(fd, block, CHUNK)) > 0) {
        for (k = 0; k < rd; k++) {
            if (block[k] == '\n') {
                if (cnt >= LINES_MAX) {
                    printf("слишком много строк\n");
                    return 1;
                }
                begin[cnt] = line_start;
                size[cnt] = cur - line_start;
                cnt++;
                line_start = cur + 1;
            }
            cur++;
        }
    }
    if (rd == -1) {
        perror("read");
        return 1;
    }
    if (line_start < cur && cnt < LINES_MAX) {
        begin[cnt] = line_start;
        size[cnt] = cur - line_start;
        cnt++;
    }

    printf("строка  отступ  длина\n");
    for (k = 0; k < cnt; k++)
        printf("%6d  %6ld  %5d\n", k + 1, (long)begin[k], size[k]);

    while (1) {
        printf("Номер строки (0 - выход): ");
        res = scanf("%d", &want);
        if (res == EOF)
            break;
        if (res != 1) {
            printf("нужно число\n");
            while (getchar() != '\n')
                ;
            continue;
        }
        if (want == 0)
            break;
        if (want < 0 || want > cnt) {
            printf("в файле строк: %d\n", cnt);
            continue;
        }

        text = malloc(size[want - 1] + 1);
        if (text == NULL) {
            perror("malloc");
            return 1;
        }
        lseek(fd, begin[want - 1], SEEK_SET);
        if (read(fd, text, size[want - 1]) != size[want - 1]) {
            perror("read");
            free(text);
            return 1;
        }
        text[size[want - 1]] = '\0';
        printf("%s\n", text);
        free(text);
    }

    close(fd);
    return 0;
}

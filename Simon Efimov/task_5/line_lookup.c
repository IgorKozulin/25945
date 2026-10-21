#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct {
    long offset;
    int length;
} LineInfo;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    LineInfo *table = NULL;
    int num_lines = 0;
    int capacity = 0;

    char buf[1];
    long pos = 0;
    long line_start = 0;
    int line_len = 0;

    while (read(fd, buf, 1) == 1) {
        if (buf[0] == '\n') {
            if (num_lines >= capacity) {
                capacity = capacity == 0 ? 16 : capacity * 2;
                table = realloc(table, capacity * sizeof(LineInfo));
                if (table == NULL) {
                    perror("realloc");
                    close(fd);
                    return 1;
                }
            }
            table[num_lines].offset = line_start;
            table[num_lines].length = line_len;
            num_lines++;
            line_start = pos + 1;
            line_len = 0;
        } else {
            line_len++;
        }
        pos++;
    }

    if (line_len > 0) {
        if (num_lines >= capacity) {
            capacity = capacity == 0 ? 16 : capacity * 2;
            table = realloc(table, capacity * sizeof(LineInfo));
            if (table == NULL) {
                perror("realloc");
                close(fd);
                return 1;
            }
        }
        table[num_lines].offset = line_start;
        table[num_lines].length = line_len;
        num_lines++;
    }

    printf("--- Debug: Line Table ---\n");
    for (int i = 0; i < num_lines; i++) {
        printf("Line %d: Offset = %ld, Length = %d\n",
               i + 1, table[i].offset, table[i].length);
    }
    printf("-------------------------\n");

    int line_num;
    while (1) {
        printf("Enter line number (0 to quit): ");
        fflush(stdout);
        if (scanf("%d", &line_num) != 1) {
            break;
        }
        if (line_num == 0) {
            break;
        }
        if (line_num < 1 || line_num > num_lines) {
            printf("Invalid line number. Valid range: 1..%d\n", num_lines);
            continue;
        }

        LineInfo info = table[line_num - 1];
        if (lseek(fd, info.offset, SEEK_SET) == -1) {
            perror("lseek");
            continue;
        }

        char *line_buf = malloc(info.length + 1);
        if (line_buf == NULL) {
            perror("malloc");
            continue;
        }

        ssize_t n = read(fd, line_buf, info.length);
        if (n == -1) {
            perror("read");
            free(line_buf);
            continue;
        }
        line_buf[n] = '\0';
        printf("%s\n", line_buf);
        free(line_buf);
    }

    close(fd);
    free(table);
    return 0;
}

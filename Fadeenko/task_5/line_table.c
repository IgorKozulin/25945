#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    off_t offset;
    size_t length;
} Line;

int main(int argc, char *argv[])
{
    int fd;
    char block[4096];
    ssize_t bytes;
    off_t position = 0;
    off_t start = 0;
    Line *lines = NULL;
    size_t count = 0;
    size_t capacity = 0;
    size_t number;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return EXIT_FAILURE;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    while ((bytes = read(fd, block, sizeof(block))) > 0) {
        position = lseek(fd, 0L, SEEK_CUR);
        if (position == (off_t)-1) {
            perror("lseek");
            free(lines);
            close(fd);
            return EXIT_FAILURE;
        }

        for (ssize_t i = 0; i < bytes; ++i) {
            if (block[i] == '\n') {
                off_t newline = position - bytes + i;

                if (count == capacity) {
                    capacity = capacity == 0 ? 16 : capacity * 2;
                    Line *new_lines = realloc(lines, capacity * sizeof(*lines));
                    if (new_lines == NULL) {
                        perror("realloc");
                        free(lines);
                        close(fd);
                        return EXIT_FAILURE;
                    }
                    lines = new_lines;
                }
                lines[count++] = (Line){start, (size_t)(newline - start + 1)};
                start = newline + 1;
            }
        }
    }

    if (bytes == -1) {
        perror("read");
        free(lines);
        close(fd);
        return EXIT_FAILURE;
    }

    if (position > start) {
        if (count == capacity) {
            capacity = capacity == 0 ? 1 : capacity * 2;
            Line *new_lines = realloc(lines, capacity * sizeof(*lines));
            if (new_lines == NULL) {
                perror("realloc");
                free(lines);
                close(fd);
                return EXIT_FAILURE;
            }
            lines = new_lines;
        }
        lines[count++] = (Line){start, (size_t)(position - start)};
    }

    while (printf("Line number (0 to exit): "), fflush(stdout),
           scanf("%zu", &number) == 1 && number != 0) {
        if (number > count) {
            puts("No such line");
            continue;
        }

        char *text = malloc(lines[number - 1].length + 1);
        if (text == NULL || lseek(fd, lines[number - 1].offset, SEEK_SET) == (off_t)-1) {
            perror(text == NULL ? "malloc" : "lseek");
            free(text);
            break;
        }

        bytes = read(fd, text, lines[number - 1].length);
        if (bytes < 0 || (size_t)bytes != lines[number - 1].length) {
            perror("read");
            free(text);
            break;
        }

        text[bytes] = '\0';
        printf("%s", text);
        if (bytes == 0 || text[bytes - 1] != '\n')
            putchar('\n');
        free(text);
    }

    free(lines);
    close(fd);
    return EXIT_SUCCESS;
}

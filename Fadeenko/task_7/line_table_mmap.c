#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    size_t offset;
    size_t length;
} Line;

int main(int argc, char *argv[])
{
    int fd;
    struct stat info;
    char *data = MAP_FAILED;
    size_t size;
    Line *lines = NULL;
    size_t count = 0;
    size_t capacity = 0;
    size_t start = 0;
    size_t number;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return EXIT_FAILURE;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1 || fstat(fd, &info) == -1) {
        perror(fd == -1 ? "open" : "fstat");
        if (fd != -1)
            close(fd);
        return EXIT_FAILURE;
    }

    size = (size_t)info.st_size;
    if (size != 0) {
        data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) {
            perror("mmap");
            close(fd);
            return EXIT_FAILURE;
        }
    }
    close(fd);

    for (size_t i = 0; i < size; ++i) {
        if (data[i] == '\n') {
            if (count == capacity) {
                capacity = capacity == 0 ? 16 : capacity * 2;
                Line *new_lines = realloc(lines, capacity * sizeof(*lines));
                if (new_lines == NULL) {
                    perror("realloc");
                    free(lines);
                    munmap(data, size);
                    return EXIT_FAILURE;
                }
                lines = new_lines;
            }
            lines[count++] = (Line){start, i - start + 1};
            start = i + 1;
        }
    }

    if (size > start) {
        if (count == capacity) {
            capacity = capacity == 0 ? 1 : capacity * 2;
            Line *new_lines = realloc(lines, capacity * sizeof(*lines));
            if (new_lines == NULL) {
                perror("realloc");
                free(lines);
                munmap(data, size);
                return EXIT_FAILURE;
            }
            lines = new_lines;
        }
        lines[count++] = (Line){start, size - start};
    }

    while (printf("Line number (0 to exit): "), fflush(stdout),
           scanf("%zu", &number) == 1 && number != 0) {
        if (number > count) {
            puts("No such line");
            continue;
        }
        fwrite(data + lines[number - 1].offset, 1, lines[number - 1].length, stdout);
        if (lines[number - 1].length == 0 ||
            data[lines[number - 1].offset + lines[number - 1].length - 1] != '\n')
            putchar('\n');
    }

    free(lines);
    if (size != 0)
        munmap(data, size);
    return EXIT_SUCCESS;
}

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

static int append_line(Line **lines, size_t *count, size_t *capacity, Line value)
{
    if (*count == *capacity) {
        size_t new_capacity = *capacity == 0 ? 16 : *capacity * 2;
        Line *new_lines = realloc(*lines, new_capacity * sizeof(**lines));
        if (new_lines == NULL)
            return -1;
        *lines = new_lines;
        *capacity = new_capacity;
    }
    (*lines)[(*count)++] = value;
    return 0;
}

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
    int status = EXIT_SUCCESS;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return EXIT_FAILURE;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }
    if (fstat(fd, &info) == -1) {
        perror("fstat");
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
            Line value = {start, i - start + 1};
            if (append_line(&lines, &count, &capacity, value) == -1) {
                perror("realloc");
                status = EXIT_FAILURE;
                goto cleanup;
            }
            start = i + 1;
        }
    }

    if (size > start) {
        Line value = {start, size - start};
        if (append_line(&lines, &count, &capacity, value) == -1) {
            perror("realloc");
            status = EXIT_FAILURE;
            goto cleanup;
        }
    }

    while (printf("Line number (0 to exit): "), fflush(stdout),
           scanf("%zu", &number) == 1 && number != 0) {
        size_t index;
        size_t offset;
        size_t length;

        if (number > count) {
            puts("No such line");
            continue;
        }
        index = number - 1;
        offset = lines[index].offset;
        length = lines[index].length;
        fwrite(data + offset, 1, length, stdout);
        if (length == 0 || data[offset + length - 1] != '\n')
            putchar('\n');
    }

cleanup:
    free(lines);
    if (size != 0)
        munmap(data, size);
    return status;
}
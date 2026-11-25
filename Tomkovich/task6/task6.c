#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    off_t offset;
    size_t length;
} Line;

static volatile sig_atomic_t timed_out;

static void on_alarm(int signal_number)
{
    (void)signal_number;
    timed_out = 1;
}

static void flush_remaining(int fd, char *block, size_t block_size)
{
    ssize_t bytes;
    puts("\nTime is up. Whole file:");
    fflush(stdout);
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        return;
    }
    while ((bytes = read(fd, block, block_size)) > 0)
        if (write(STDOUT_FILENO, block, (size_t)bytes) != bytes)
            break;
}

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
    char block[4096];
    char input[64];
    ssize_t bytes;
    off_t position = 0;
    off_t start = 0;
    Line *lines = NULL;
    size_t count = 0;
    size_t capacity = 0;
    struct sigaction action = {0};
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

    while ((bytes = read(fd, block, sizeof(block))) > 0) {
        position = lseek(fd, 0L, SEEK_CUR);
        if (position == (off_t)-1) {
            perror("lseek");
            status = EXIT_FAILURE;
            goto cleanup;
        }

        for (ssize_t i = 0; i < bytes; ++i) {
            if (block[i] == '\n') {
                off_t newline = position - bytes + i;
                Line value = {start, (size_t)(newline - start + 1)};
                if (append_line(&lines, &count, &capacity, value) == -1) {
                    perror("realloc");
                    status = EXIT_FAILURE;
                    goto cleanup;
                }
                start = newline + 1;
            }
        }
    }

    if (bytes == -1) {
        perror("read");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    if (position > start) {
        Line value = {start, (size_t)(position - start)};
        if (append_line(&lines, &count, &capacity, value) == -1) {
            perror("realloc");
            status = EXIT_FAILURE;
            goto cleanup;
        }
    }

    action.sa_handler = on_alarm;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL) == -1) {
        perror("sigaction");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    for (;;) {
        long number;
        char *end;
        char *text;
        size_t length;

        timed_out = 0;
        printf("Line number (0 to exit, 5 seconds): ");
        fflush(stdout);
        alarm(5);
        errno = 0;

        if (fgets(input, sizeof(input), stdin) == NULL) {
            alarm(0);
            if (!timed_out)
                break;
            flush_remaining(fd, block, sizeof(block));
            break;
        }

        alarm(0);
        number = strtol(input, &end, 10);
        if (end == input || (*end != '\n' && *end != '\0')) {
            puts("Enter a number");
            continue;
        }
        if (number == 0)
            break;
        if (number < 1 || (size_t)number > count) {
            puts("No such line");
            continue;
        }

        length = lines[number - 1].length;
        text = malloc(length + 1);
        if (text == NULL) {
            perror("malloc");
            status = EXIT_FAILURE;
            break;
        }
        if (lseek(fd, lines[number - 1].offset, SEEK_SET) == (off_t)-1) {
            perror("lseek");
            free(text);
            status = EXIT_FAILURE;
            break;
        }
        bytes = read(fd, text, length);
        if (bytes < 0 || (size_t)bytes != length) {
            perror("read");
            free(text);
            status = EXIT_FAILURE;
            break;
        }
        text[bytes] = '\0';
        printf("%s", text);
        if (bytes == 0 || text[bytes - 1] != '\n')
            putchar('\n');
        free(text);
    }

cleanup:
    free(lines);
    close(fd);
    return status;
}
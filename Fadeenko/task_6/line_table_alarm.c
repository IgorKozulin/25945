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

    action.sa_handler = on_alarm;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL) == -1) {
        perror("sigaction");
        free(lines);
        close(fd);
        return EXIT_FAILURE;
    }

    for (;;) {
        long number;
        char *end;

        timed_out = 0;
        printf("Line number (0 to exit, 5 seconds): ");
        fflush(stdout);
        alarm(5);
        errno = 0;

        if (fgets(input, sizeof(input), stdin) == NULL) {
            alarm(0);
            if (!timed_out)
                break;

            puts("\nTime is up. Whole file:");
            fflush(stdout);
            if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
                perror("lseek");
                break;
            }
            while ((bytes = read(fd, block, sizeof(block))) > 0)
                if (write(STDOUT_FILENO, block, (size_t)bytes) != bytes)
                    break;
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

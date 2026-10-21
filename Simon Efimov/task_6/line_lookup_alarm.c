#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

typedef struct {
    long offset;
    int length;
} LineInfo;

static const char *g_filename = NULL;

/* SIGALRM handler: print whole file and exit */
void alarm_handler(int sig) {
    (void)sig;
    printf("\n[TIMEOUT] Time is up! Printing full file content...\n");

    int fd = open(g_filename, O_RDONLY);
    if (fd == -1) { perror("open"); _exit(EXIT_FAILURE); }

    char buf[1024];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (write(STDOUT_FILENO, buf, n) != n) break;
    }
    close(fd);
    _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    g_filename = argv[1];

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) { perror("open"); return 1; }

    LineInfo *table = NULL;
    int num_lines = 0;
    int capacity = 0;

    char buf[1];
    long pos = 0;
    long line_start = 0;
    int line_len = 0;

    /* Build line table */
    while (read(fd, buf, 1) == 1) {
        if (buf[0] == '\n') {
            if (num_lines >= capacity) {
                capacity = capacity == 0 ? 16 : capacity * 2;
                table = realloc(table, capacity * sizeof(LineInfo));
                if (table == NULL) { perror("realloc"); close(fd); return 1; }
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

    /* Last line without '\n' */
    if (line_len > 0) {
        if (num_lines >= capacity) {
            capacity = capacity == 0 ? 16 : capacity * 2;
            table = realloc(table, capacity * sizeof(LineInfo));
            if (table == NULL) { perror("realloc"); close(fd); return 1; }
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

    signal(SIGALRM, alarm_handler);

    /* Interactive loop with 5s timeout */
    int line_num;
    while (1) {
        printf("Enter line number (0 to quit, 5 sec timeout): ");
        fflush(stdout);

        alarm(5);

        if (scanf("%d", &line_num) != 1) {
            alarm(0);
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            continue;
        }

        alarm(0);

        if (line_num == 0) break;
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
        if (line_buf == NULL) { perror("malloc"); continue; }

        ssize_t n = read(fd, line_buf, info.length);
        if (n == -1) { perror("read"); free(line_buf); continue; }

        line_buf[n] = '\0';
        printf("%s\n", line_buf);
        free(line_buf);
    }

    close(fd);
    free(table);
    return 0;
}

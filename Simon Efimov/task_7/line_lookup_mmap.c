#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>

/* Line descriptor: offset and length in file */
typedef struct {
    long offset;
    int length;
} LineInfo;

/* Global mmap pointer for the alarm handler */
static char *g_map = NULL;
static size_t g_size = 0;

/* SIGALRM handler: print whole file, exit */
void alarm_handler(int sig) {
    (void)sig;
    printf("\n[TIMEOUT] Time is up! Printing full file content...\n");
    if (g_map != NULL && g_size > 0) {
        ssize_t n = write(STDOUT_FILENO, g_map, g_size);
        (void)n;
    }
    _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) { perror("open"); return 1; }

    /* Get file size via fstat */
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return 1;
    }

    if (st.st_size == 0) {
        printf("File is empty.\n");
        close(fd);
        return 0;
    }

    /* Map the file into memory */
    g_size = (size_t)st.st_size;
    g_map = mmap(NULL, g_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (g_map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    /* Descriptor is no longer needed after mmap */
    close(fd);

    /* Build line table */
    LineInfo *table = NULL;
    int num_lines = 0;
    int capacity = 0;

    long line_start = 0;
    for (size_t i = 0; i < g_size; i++) {
        if (g_map[i] == '\n') {
            if (num_lines >= capacity) {
                capacity = capacity == 0 ? 16 : capacity * 2;
                table = realloc(table, capacity * sizeof(LineInfo));
                if (table == NULL) { perror("realloc"); munmap(g_map, g_size); return 1; }
            }
            table[num_lines].offset = line_start;
            table[num_lines].length = (int)(i - line_start);
            num_lines++;
            line_start = (long)i + 1;
        }
    }

    /* Last line without '\n' */
    if ((size_t)line_start < g_size) {
        if (num_lines >= capacity) {
            capacity = capacity == 0 ? 16 : capacity * 2;
            table = realloc(table, capacity * sizeof(LineInfo));
            if (table == NULL) { perror("realloc"); munmap(g_map, g_size); return 1; }
        }
        table[num_lines].offset = line_start;
        table[num_lines].length = (int)(g_size - (size_t)line_start);
        num_lines++;
    }

    /* Debug output */
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

        /* Write exactly 'length' bytes directly from mapped memory */
        LineInfo info = table[line_num - 1];
        fwrite(g_map + info.offset, 1, info.length, stdout);
        putchar('\n');
    }

    /* Cleanup */
    free(table);
    munmap(g_map, g_size);
    return 0;
}

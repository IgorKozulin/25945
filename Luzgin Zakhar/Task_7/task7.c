#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct { size_t offset, length; } Line;
typedef struct { Line *items; size_t count, capacity; } Table;
static volatile sig_atomic_t timed_out = 0;

static void on_alarm(int signo) { (void)signo; timed_out = 1; }

static int add_line(Table *t, size_t offset, size_t length) {
    if (t->count == t->capacity) {
        size_t cap = t->capacity ? t->capacity * 2 : 16;
        Line *p;
        if (cap < t->capacity || cap > (size_t)-1 / sizeof(Line)) return -1;
        p = realloc(t->items, cap * sizeof(Line));
        if (!p) return -1;
        t->items = p; t->capacity = cap;
    }
    t->items[t->count].offset = offset;
    t->items[t->count].length = length;
    ++t->count;
    return 0;
}

/* Строим таблицу напрямую по отображённым байтам (без read/lseek). */
static int build_table(const char *data, size_t size, Table *table) {
    size_t i, start = 0;
    for (i = 0; i < size; ++i) {
        if (data[i] == '\n') {
            if (add_line(table, start, i + 1 - start) < 0) return -1;
            start = i + 1;
        }
    }
    if (start < size && add_line(table, start, size - start) < 0) return -1;
    return 0;
}

/* Выводим таблицу смещений и длин без чтения файла через read. */
static void show_table(const Table *t) {
    size_t i;
    puts("Таблица строк:");
    puts("Номер    Смещение     Длина");
    for (i = 0; i < t->count; ++i) {
        printf("%-8zu %-12zu %zu\n", i + 1,
               t->items[i].offset, t->items[i].length);
    }
}

/* Чтение номера с 5-секундным alarm. fread/fgets здесь не используются для файла. */
static int input_number(char *input, size_t cap) {
    size_t n = 0;
    int overflow = 0;
    int c;
    for (;;) {
        if (timed_out) return 1;
        c = getchar();
        if (c == EOF) {
            if (timed_out) return 1;
            if (ferror(stdin) && errno == EINTR) {
                clearerr(stdin); continue;
            }
            return -1;
        }
        if (c == '\n') break;
        if (n + 1 < cap) input[n++] = (char)c;
        else overflow = 1;
    }
    input[n] = '\0';
    return overflow ? 2 : 0;
}

int main(int argc, char **argv) {
    int fd, rc = EXIT_SUCCESS;
    struct stat st;
    struct sigaction sa;
    size_t file_size = 0;
    char *data = NULL;
    Table table = {NULL, 0, 0};
    if (argc != 2) {
        fprintf(stderr, "Использование: %s текстовый_файл\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); return EXIT_FAILURE; }
    if (fstat(fd, &st) < 0 || st.st_size < 0) {
        perror("fstat"); rc = EXIT_FAILURE; goto done;
    }
    if ((unsigned long long)st.st_size > (unsigned long long)(size_t)-1) {
        fputs("Файл слишком большой\n", stderr); rc = EXIT_FAILURE; goto done;
    }
    file_size = (size_t)st.st_size;
    if (file_size > 0) {
        void *p = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (p == MAP_FAILED) { perror("mmap"); rc = EXIT_FAILURE; goto done; }
        data = p;
    }
    if (build_table(data, file_size, &table) < 0) {
        perror("realloc"); rc = EXIT_FAILURE; goto done;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
        perror("sigaction"); rc = EXIT_FAILURE; goto done;
    }
    show_table(&table);
    for (;;) {
        char input[128], *end;
        unsigned long long number;
        int state;
        printf("Номер строки (0 — выход, 5 секунд): ");
        fflush(stdout);
        timed_out = 0;
        alarm(5);
        state = input_number(input, sizeof(input));
        alarm(0);
        if (timed_out || state == 1) {
            puts("\nВремя вышло. Содержимое файла:");
            if (file_size > 0 && fwrite(data, 1, file_size, stdout) != file_size)
                rc = EXIT_FAILURE;
            if (file_size > 0 && data[file_size-1] != '\n') putchar('\n');
            break;
        }
        if (state == -1) break;
        if (state == 2 || input[0] == '-' || input[0] == '+') {
            puts("Некорректный номер."); continue;
        }
        errno = 0;
        number = strtoull(input, &end, 10);
        if (errno == ERANGE || end == input || *end != '\0') {
            puts("Некорректный номер."); continue;
        }
        if (number == 0) break;
        if (number > table.count) {
            printf("Строка отсутствует (всего %zu).\n", table.count);
            continue;
        }
        {
            Line item = table.items[(size_t)number - 1];
            printf("Строка %llu: ", number);
            if (item.length > 0 && fwrite(data + item.offset, 1, item.length, stdout) != item.length) {
                perror("fwrite"); rc = EXIT_FAILURE; break;
            }
            if (item.length == 0 || data[item.offset + item.length - 1] != '\n') putchar('\n');
        }
    }
done:
    free(table.items);
    if (data != NULL && munmap(data, file_size) < 0) {
        perror("munmap"); rc = EXIT_FAILURE;
    }
    if (close(fd) < 0) { perror("close"); rc = EXIT_FAILURE; }
    return rc;
}

/* Сборка: gcc -std=c99 -Wall -Wextra task7.c -o task7
   Запуск: ./task7 sample.txt
   Введите 1, 2 или 0; если не вводить номер 5 секунд, выведется весь файл. */

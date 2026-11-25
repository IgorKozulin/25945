#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct { off_t offset, length; } Line;
typedef struct { Line *items; size_t count, capacity; } Table;
static volatile sig_atomic_t timed_out = 0;

static void on_alarm(int signo) { (void)signo; timed_out = 1; }

static int add_line(Table *t, off_t start, off_t length) {
    if (t->count == t->capacity) {
        size_t cap = t->capacity ? t->capacity * 2 : 16;
        Line *p;
        if (cap < t->capacity || cap > (size_t)-1 / sizeof(Line)) return -1;
        p = realloc(t->items, cap * sizeof(Line));
        if (!p) return -1;
        t->items = p; t->capacity = cap;
    }
    t->items[t->count].offset = start;
    t->items[t->count].length = length;
    ++t->count;
    return 0;
}

/* Сначала строим таблицу смещений и длин строк. */
static int build_table(int fd, Table *t) {
    char buf[4096];
    ssize_t n;
    off_t start = 0, pos = 0;
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) return -1;
    while ((n = read(fd, buf, sizeof(buf))) != 0) {
        ssize_t i;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        for (i = 0; i < n; ++i) {
            ++pos;
            if (buf[i] == '\n') {
                if (add_line(t, start, pos - start) != 0) return -1;
                start = pos;
            }
        }
    }
    if (pos > start && add_line(t, start, pos - start) != 0) return -1;
    return 0;
}

static int print_range(int fd, off_t offset, off_t length) {
    char buf[4096];
    int last = '\n';
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) return -1;
    while (length > 0) {
        size_t want = length > (off_t)sizeof(buf) ? sizeof(buf) : (size_t)length;
        ssize_t n = read(fd, buf, want);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        if (fwrite(buf, 1, (size_t)n, stdout) != (size_t)n) return -1;
        last = (unsigned char)buf[n - 1];
        length -= (off_t)n;
    }
    if (last != '\n') putchar('\n');
    return 0;
}

/* Выводим таблицу начальных позиций и длин строк. */
static void show_table(const Table *t) {
    size_t i;
    puts("Таблица строк:");
    puts("Номер    Смещение     Длина");
    for (i = 0; i < t->count; ++i) {
        printf("%-8zu %-12jd %jd\n", i + 1,
               (intmax_t)t->items[i].offset, (intmax_t)t->items[i].length);
    }
}

/* При истечении пяти секунд печатаем файл целиком. */
static int print_whole_file(int fd) {
    char buf[4096];
    ssize_t n;
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) return -1;
    while ((n = read(fd, buf, sizeof(buf))) != 0) {
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return -1;
        if (fwrite(buf, 1, (size_t)n, stdout) != (size_t)n) return -1;
    }
    return 0;
}

/* Читаем номер непосредственно с терминала, чтобы alarm прерывал ожидание. */
static int read_number(char *out, size_t cap) {
    size_t len = 0;
    int overflow = 0;
    char ch;
    for (;;) {
        ssize_t n;
        if (timed_out) return 1;
        n = read(STDIN_FILENO, &ch, 1);
        if (n == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        if (ch == '\n') break;
        if (len + 1 < cap) out[len++] = ch;
        else overflow = 1;
    }
    out[len] = '\0';
    return overflow ? 2 : 0;
}

int main(int argc, char **argv) {
    int fd, rc = EXIT_SUCCESS;
    Table table = {NULL, 0, 0};
    struct sigaction sa;
    if (argc != 2) {
        fprintf(stderr, "Использование: %s текстовый_файл\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); return EXIT_FAILURE; }
    if (build_table(fd, &table) < 0) {
        perror("build_table"); rc = EXIT_FAILURE; goto done;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* SIGALRM должен прерывать ожидание ввода. */
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
        state = read_number(input, sizeof(input));
        alarm(0);
        if (timed_out || state == 1) {
            puts("\nВремя вышло. Содержимое файла:");
            if (print_whole_file(fd) < 0) {
                perror("print_whole_file"); rc = EXIT_FAILURE;
            }
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
        printf("Строка %llu: ", number);
        if (print_range(fd, table.items[(size_t)number - 1].offset,
                        table.items[(size_t)number - 1].length) < 0) {
            perror("print_range"); rc = EXIT_FAILURE; break;
        }
    }
done:
    free(table.items);
    if (close(fd) < 0) { perror("close"); rc = EXIT_FAILURE; }
    return rc;
}

/* Сборка: gcc -std=c99 -Wall -Wextra task6.c -o task6
   Запуск: ./task6 sample.txt
   Введите 1, 2 или 0; если не вводить номер 5 секунд, выведется весь файл. */

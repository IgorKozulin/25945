#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct {
    off_t offset;
    off_t length;
} line_info;

typedef struct {
    line_info *items;
    size_t count;
    size_t capacity;
} line_table;

static int append_line(line_table *table, off_t offset, off_t length)
{
    line_info *new_items;
    size_t new_capacity;

    if (table->count == table->capacity) {
        if (table->capacity > (size_t)-1 / 2 / sizeof(line_info)) {
            fputs("Слишком много строк\n", stderr);
            return -1;
        }
        new_capacity = table->capacity ? table->capacity * 2 : 16;
        new_items = realloc(table->items, new_capacity * sizeof(line_info));
        if (new_items == NULL) {
            perror("realloc");
            return -1;
        }
        table->items = new_items;
        table->capacity = new_capacity;
    }
    table->items[table->count].offset = offset;
    table->items[table->count].length = length;
    ++table->count;
    return 0;
}

static int build_table(int fd, line_table *table)
{
    off_t start = 0;
    off_t position;
    ssize_t bytes;
    char character;

    /* Читаем текст через read(2); позицию находим через lseek(2). */
    while ((bytes = read(fd, &character, 1)) != 0) {
        if (bytes == -1) {
            if (errno == EINTR)
                continue;
            perror("read");
            return -1;
        }
        if (character != '\n')
            continue;
        position = lseek(fd, 0L, SEEK_CUR);
        if (position == (off_t)-1) {
            perror("lseek");
            return -1;
        }
        if (append_line(table, start, position - start) == -1)
            return -1;
        start = position;
    }
    position = lseek(fd, 0L, SEEK_CUR);
    if (position == (off_t)-1) {
        perror("lseek");
        return -1;
    }
    if (position > start && append_line(table, start, position - start) == -1)
        return -1;
    return 0;
}

static void print_table(const line_table *table)
{
    size_t index;
    puts("\nТаблица строк:");
    puts("Номер      Отступ      Длина");
    for (index = 0; index < table->count; ++index)
        printf("%-10zu %-11jd %jd\n", index + 1,
               (intmax_t)table->items[index].offset,
               (intmax_t)table->items[index].length);
}

static int print_line(int fd, line_info line)
{
    char buffer[4096];
    off_t remaining = line.length;
    int last_character = '\0';

    if (lseek(fd, line.offset, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        return -1;
    }
    while (remaining > 0) {
        size_t wanted = remaining > (off_t)sizeof(buffer)
                            ? sizeof(buffer) : (size_t)remaining;
        ssize_t bytes = read(fd, buffer, wanted);
        if (bytes == -1) {
            if (errno == EINTR)
                continue;
            perror("read");
            return -1;
        }
        if (bytes == 0) {
            fputs("Неожиданный конец файла\n", stderr);
            return -1;
        }
        if (fwrite(buffer, 1, (size_t)bytes, stdout) != (size_t)bytes) {
            perror("fwrite");
            return -1;
        }
        last_character = (unsigned char)buffer[bytes - 1];
        remaining -= (off_t)bytes;
    }
    if (last_character != '\n')
        putchar('\n');
    return 0;
}

int main(int argc, char **argv)
{
    int fd;
    line_table table = {NULL, 0, 0};
    char input[128];
    int exit_code = EXIT_SUCCESS;

    if (argc != 2) {
        fprintf(stderr, "Использование: %s текстовый_файл\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }
    if (build_table(fd, &table) == -1) {
        exit_code = EXIT_FAILURE;
        goto finish;
    }
    print_table(&table);

    for (;;) {
        char *end;
        unsigned long long number;

        printf("Введите номер строки (0 — выход): ");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL)
            break;
        errno = 0;
        if (input[0] == '-' || input[0] == '+') {
            puts("Номер строки должен быть неотрицательным целым числом.");
            continue;
        }
        number = strtoull(input, &end, 10);
        if (errno == ERANGE || end == input ||
            (*end != '\n' && *end != '\0')) {
            puts("Некорректный номер строки.");
            continue;
        }
        if (number == 0)
            break;
        if (number > table.count) {
            printf("Такой строки нет (в файле %zu строк).\n", table.count);
            continue;
        }
        printf("Строка %llu: ", number);
        if (print_line(fd, table.items[(size_t)number - 1]) == -1) {
            exit_code = EXIT_FAILURE;
            break;
        }
    }
finish:
    free(table.items);
    if (close(fd) == -1) {
        perror("close");
        exit_code = EXIT_FAILURE;
    }
    return exit_code;
}

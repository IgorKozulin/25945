#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

// Храним начало и длину строки
typedef struct {
    off_t offset;
    off_t length;
} line_info;

// Таблица всех строк файла
typedef struct {
    line_info *items;
    size_t count;
    size_t capacity;
} line_table;

// Функция добавления строки в таблицу
static int append_line(line_table *table, off_t offset, off_t length)
{
    line_info *new_items;
    size_t new_capacity;

    // Если массив заполнен, увеличиваем память
    if (table->count == table->capacity) {
        if (table->capacity > (size_t)-1 / 2 / sizeof(line_info)) {
            fputs("Слишком много строк\n", stderr);
            return -1;
        }

        new_capacity = table->capacity ? table->capacity * 2 : 16;

        new_items = realloc(table->items,
                            new_capacity * sizeof(line_info));

        if (new_items == NULL) {
            perror("realloc");
            return -1;
        }

        table->items = new_items;
        table->capacity = new_capacity;
    }

    // Запоминаем позицию и длину строки
    table->items[table->count].offset = offset;
    table->items[table->count].length = length;

    ++table->count;
    return 0;
}

// Функция построения таблицы строк
static int build_table(int fd, line_table *table)
{
    off_t start = 0;
    off_t position;
    ssize_t bytes;
    char character;

    // Читаем файл посимвольно
    while ((bytes = read(fd, &character, 1)) != 0) {

        // Проверяем ошибки чтения
        if (bytes == -1) {
            if (errno == EINTR)
                continue;

            perror("read");
            return -1;
        }

        // Ищем конец строки
        if (character != '\n')
            continue;

        // Получаем позицию после переноса строки
        position = lseek(fd, 0L, SEEK_CUR);

        if (position == (off_t)-1) {
            perror("lseek");
            return -1;
        }

        // Добавляем строку в таблицу
        if (append_line(table, start, position - start) == -1)
            return -1;

        start = position;
    }

    // Получаем текущую позицию в файле
    position = lseek(fd, 0L, SEEK_CUR);

    if (position == (off_t)-1) {
        perror("lseek");
        return -1;
    }

    // Добавляем последнюю строку без \n
    if (position > start &&
        append_line(table, start, position - start) == -1)
        return -1;

    return 0;
}

// Функция вывода таблицы строк
static void print_table(const line_table *table)
{
    size_t index;

    puts("\nТаблица строк:");
    puts("Номер      Отступ      Длина");

    // Выводим данные каждой строки
    for (index = 0; index < table->count; ++index)
        printf("%-10zu %-11jd %jd\n",
               index + 1,
               (intmax_t)table->items[index].offset,
               (intmax_t)table->items[index].length);
}

// Функция вывода выбранной строки
static int print_line(int fd, line_info line)
{
    char buffer[4096];
    off_t remaining = line.length;
    int last_character = '\0';

    // Переходим к началу нужной строки
    if (lseek(fd, line.offset, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        return -1;
    }

    // Читаем строку частями
    while (remaining > 0) {

        size_t wanted = remaining > (off_t)sizeof(buffer)
                            ? sizeof(buffer) : (size_t)remaining;

        // Считываем данные из файла
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

        // Выводим прочитанные символы
        if (fwrite(buffer, 1, (size_t)bytes, stdout) != (size_t)bytes) {
            perror("fwrite");
            return -1;
        }

        last_character = (unsigned char)buffer[bytes - 1];
        remaining -= (off_t)bytes;
    }

    // Добавляем перенос строки при необходимости
    if (last_character != '\n')
        putchar('\n');

    return 0;
}

// Главная функция программы
int main(int argc, char **argv)
{
    int fd;
    line_table table = {NULL, 0, 0};
    char input[128];
    int exit_code = EXIT_SUCCESS;

    // Проверяем аргументы запуска
    if (argc != 2) {
        fprintf(stderr, "Использование: %s текстовый_файл\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Открываем файл для чтения
    fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    // Строим таблицу строк
    if (build_table(fd, &table) == -1) {
        exit_code = EXIT_FAILURE;
        goto finish;
    }

    // Показываем таблицу
    print_table(&table);

    // Цикл ввода номера строки
    for (;;) {
        char *end;
        unsigned long long number;

        printf("Введите номер строки (0 — выход): ");
        fflush(stdout);

        // Считываем номер строки
        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        errno = 0;

        // Запрещаем отрицательные номера
        if (input[0] == '-' || input[0] == '+') {
            puts("Номер строки должен быть неотрицательным целым числом.");
            continue;
        }

        // Преобразуем ввод в число
        number = strtoull(input, &end, 10);

        // Проверяем корректность числа
        if (errno == ERANGE || end == input ||
            (*end != '\n' && *end != '\0')) {
            puts("Некорректный номер строки.");
            continue;
        }

        // Ноль завершает программу
        if (number == 0)
            break;

        // Проверяем существование строки
        if (number > table.count) {
            printf("Такой строки нет (в файле %zu строк).\n",
                   table.count);
            continue;
        }

        // Выводим нужную строку
        printf("Строка %llu: ", number);

        if (print_line(fd, table.items[(size_t)number - 1]) == -1) {
            exit_code = EXIT_FAILURE;
            break;
        }
    }

finish:
    // Освобождаем память
    free(table.items);

    // Закрываем файл
    if (close(fd) == -1) {
        perror("close");
        exit_code = EXIT_FAILURE;
    }

    return exit_code;
}

// для задачи 7 убедиться что сумма всех строк в файле текста занимают столько же памяти сколько и фаил текста 
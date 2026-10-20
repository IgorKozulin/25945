#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define INITIAL_CAPACITY 64
#define TIMEOUT_SECONDS 5

typedef struct {
    off_t offset;
    int length;
} LineInfo;

// глобальные переменные нужны, чтобы обработчик сигнала имел доступ к отображению
static char *g_mapped_data = NULL;
static size_t g_file_size = 0;

// асинхронно-безопасный обработчик сигнала при истечении 5 секунд
void on_alarm_timeout(int sig)
{
    (void)sig;

    const char msg[] = "\n[TIMEOUT] Время вышло! Печать содержимого файла:\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);

    // выводим всю отображенную память целиком за один системный вызов
    if (g_mapped_data != NULL && g_file_size > 0) {
        write(STDOUT_FILENO, g_mapped_data, g_file_size);
    }

    // завершаем процесс без вызова буферов stdio
    _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    // проверка аргументов командной строки
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <имя_файла>\n", argv[0]);
        return 1;
    }

    // открытие файла
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Ошибка open");
        return 1;
    }

    // получаем метаданные файла через fstat, чтобы узнать его точный размер в байтах
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        perror("Ошибка fstat");
        close(fd);
        return 1;
    }

    // mmap на файле нулевого размера вызовет ошибку, поэтому проверяем заранее
    if (file_stat.st_size == 0) {
        printf("Файл пуст.\n");
        close(fd);
        return 0;
    }

    g_file_size = (size_t)file_stat.st_size;

    // PROT_READ — только для чтения
    // MAP_PRIVATE — изменения (если бы они были) не пишутся обратно в файл
    g_mapped_data = mmap(NULL, g_file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (g_mapped_data == MAP_FAILED) {
        perror("Ошибка mmap");
        close(fd);
        return 1;
    }

    // После успешного mmap дескриптор файла больше не нужен, его можно закрыть
    close(fd);

    // 5. Построение таблицы смещений и длин строк (по массиву в памяти)
    int capacity = INITIAL_CAPACITY;
    int line_count = 0;
    LineInfo *table = malloc(capacity * sizeof(LineInfo));
    if (!table) {
        perror("Ошибка malloc");
        munmap(g_mapped_data, g_file_size);
        return 1;
    }

    off_t current_line_start = 0;

    for (size_t i = 0; i < g_file_size; i++) {
        if (g_mapped_data[i] == '\n') {
            table[line_count].offset = current_line_start;
            
            // Длина строки без символа '\n' (и без '\r', если файл с Windows)
            int len = i - current_line_start;
            if (len > 0 && g_mapped_data[i - 1] == '\r') {
                len--;
            }
            table[line_count].length = len;
            line_count++;

            // Динамическое увеличение емкости массива при необходимости
            if (line_count >= capacity) {
                capacity *= 2;
                LineInfo *temp = realloc(table, capacity * sizeof(LineInfo));
                if (!temp) {
                    perror("Ошибка realloc");
                    free(table);
                    munmap(g_mapped_data, g_file_size);
                    return 1;
                }
                table = temp;
            }

            // Следующая строка начинается сразу за '\n'
            current_line_start = i + 1;
        }
    }

    // Если последняя строка не заканчивалась на '\n'
    if (current_line_start < (off_t)g_file_size) {
        table[line_count].offset = current_line_start;
        int len = g_file_size - current_line_start;
        if (len > 0 && g_mapped_data[g_file_size - 1] == '\r') {
            len--;
        }
        table[line_count].length = len;
        line_count++;
    }

    // 6. Отладочный вывод таблицы смещений
    printf("--- Debug: Line Table ---\n");
    for (int i = 0; i < line_count; i++) {
        printf("Line %d: Offset = %ld, Length = %d\n",
               i + 1, (long)table[i].offset, table[i].length);
    }
    printf("Total lines: %d\n", line_count);
    printf("-------------------------\n\n");

    signal(SIGALRM, on_alarm_timeout);

    int line_number;
    while (1) {
        printf("Enter line number (0 to quit, 5 sec timeout): ");
        fflush(stdout);

        alarm(TIMEOUT_SECONDS);

        int scan_status = scanf("%d", &line_number);

        alarm(0);

        if (scan_status != 1) {
            printf("Некорректный ввод.\n");
            break;
        }

        if (line_number == 0) {
            break;
        }

        if (line_number < 1 || line_number > line_count) {
            printf("No such line\n");
            continue;
        }

        int idx = line_number - 1;

        // печатаем строку прямо из отображенной памяти без буферов и копирования!
        printf("%.*s\n", table[idx].length, g_mapped_data + table[idx].offset);
    }

    free(table);
    munmap(g_mapped_data, g_file_size);

    return 0;
}
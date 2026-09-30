#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

void check_file_access(const char *path)
{
    // Используем режим "r" вместо "r+", так как для проверки прав достаточно чтения
    FILE *target_stream = fopen(path, "r");
    
    if (target_stream == NULL) {
        perror("Ошибка при вызове fopen");
    } else {
        printf("Файл '%s' успешно открыт\n", path);
        fclose(target_stream);
    }
}

// Печать идентификаторов в функцию
void display_process_identity(void)
{
    uid_t ruid = getuid();
    uid_t euid = geteuid();
    printf("Текущий статус процесса -> Реальный UID: %d | Эффективный UID: %d\n", (int)ruid, (int)euid);
}

int main(int argument_count, char *argument_values[])
{
    // Проверка входных аргументов с измененным текстом ошибки
    if (argument_count != 2) {
        fprintf(stderr, "Критическая ошибка. Использование: %s <путь_к_файлу>\n", argument_values[0]);
        exit(EXIT_FAILURE);
    }

    const char *target_file = argument_values[1];

    // Этап 1: Проверка с повышенными привилегиями (setuid)
    display_process_identity();
    check_file_access(target_file);

    // Этап 2: Сброс эффективных прав до уровня реального пользователя
    if (setuid(getuid()) < 0) {
        perror("Не удалось выполнить setuid");
        return EXIT_FAILURE;
    }

    // Этап 3: Повторная проверка после сброса прав
    display_process_identity();
    check_file_access(target_file);

    return EXIT_SUCCESS;
}

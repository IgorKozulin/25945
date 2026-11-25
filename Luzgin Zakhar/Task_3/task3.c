#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

// Функция: выводит UID и проверяет доступ к файлу
static void check_file(const char *path, const char *stage)
{
    FILE *file;

    // Показываем этап проверки
    printf("\n%s\n", stage);

    // Выводим реальный и эффективный UID
    printf("Real UID: %lu; Effective UID: %lu\n",
           (unsigned long)getuid(),
           (unsigned long)geteuid());

    // Пробуем открыть файл для чтения
    file = fopen(path, "r");

    // Проверяем, удалось ли открыть файл
    if (file == NULL) {
        perror("fopen");
        return;
    }

    // Если файл открылся успешно
    printf("Файл '%s' успешно открыт\n", path);

    // Закрываем файл
    if (fclose(file) == EOF)
        perror("fclose");
}

// Главная функция
int main(int argc, char **argv)
{
    // Получаем путь к файлу
    const char *path = (argc > 1) ? argv[1] : "private_data.txt";

    // Запоминаем реальный UID пользователя
    uid_t real_uid = getuid();

    // Проверяем количество аргументов
    if (argc > 2) {
        fprintf(stderr, "Использование: %s [путь_к_файлу]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Первая проверка доступа
    check_file(path, "До вызова setuid(getuid()):");

    // Приравниваем эффективный UID к реальному
    if (setuid(real_uid) == -1) {
        perror("setuid");
        return EXIT_FAILURE;
    }

    // Повторная проверка доступа
    check_file(path, "После вызова setuid(getuid()):");

    return EXIT_SUCCESS;
}
//cd Task_3
//sh prepare.sh
//./task3
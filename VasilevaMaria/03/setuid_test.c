
#include <stdio.h>
#include <unistd.h>

int main(void){
    FILE *f;

    // Выводим идентификаторы пользователя до сброса привилегий.
    // Real UID — идентификатор пользователя, запустившего программу.
    // Effective UID — идентификатор, используемый при проверке прав.
    printf("Real UID: %d, Effective UID: %d\n", getuid(), geteuid());

    f = fopen("data.txt", "r");

    if (f == NULL) perror("fopen");
    else {
        printf("File opened successfully\n");
        fclose(f);
    }

    // Пытаемся сбросить привилегии:
    // устанавливаем эффективный UID равным реальному UID.
    if (setuid(getuid()) == -1) {
        // Если системный вызов завершился ошибкой, выводим сообщение о причине.
        perror("setuid");
    }

    // Повторно выводим UID после вызова setuid().
    printf("Real UID: %d, Effective UID: %d\n", getuid(), geteuid());

    // Повторно проверяем доступ к файлу после сброса привилегий.
    f = fopen("data.txt", "r");

    if (f == NULL) perror("fopen");
    else {
        printf("File opened successfully\n");
        fclose(f);
    }

    return 0;
}

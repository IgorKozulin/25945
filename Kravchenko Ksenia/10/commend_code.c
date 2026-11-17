/* Лабораторная работа №10. Код завершения команды.
 *
 * Задание: программа запускает команду, заданную первым аргументом, в виде
 * порожденного процесса. Все остальные аргументы программы передаются этой
 * команде. Затем родитель дожидается завершения подпроцесса и печатает его
 * код завершения.
 *
 * Примеры запуска:
 *   ./start ls -l /tmp   - запустит ls -l /tmp
 *   ./start false        - напечатает код завершения 1
 */

#include <stdio.h>      /* printf, perror                    */
#include <stdlib.h>     /* exit                              */
#include <unistd.h>     /* fork, execvp, getpid              */
#include <sys/wait.h>   /* waitpid, WIFEXITED, WEXITSTATUS   */

int main(int argc, char *argv[]) {
    /* Проверка наличия аргументов: argv[0] - имя самой программы, команда - argv[1] */
    if (argc < 2) {
        printf("[LOG] Error: Usage %s <command> [args...]\n", argv[0]);
        return 1;
    }

    char *command = argv[1]; // Первая команда
    printf("[LOG] Program started. Command: %s (PID: %d)\n", command, getpid());

    /* Создание порождённого процесса:
       родитель получит PID ребёнка, ребёнок - 0, при ошибке - -1 */
    pid_t pid = fork();
    if (pid == -1) {
        perror("[LOG] Error forking process");
        return 1;
    }

    if (pid == 0) {
        // Дочерний процесс
        printf("[LOG] Child process started. Executing %s (PID: %d)\n", command, getpid());
        // Передача всех аргументов команде
        /* execvp заменяет образ процесса-ребёнка на указанную команду,
           ищет её в PATH. argv + 1 пропускает имя программы, поэтому
           команда получает ровно те аргументы, что были у нас после имени:
           argv[1] становится для неё argv[0] и т. д. */
        execvp(command, argv + 1);
        /* Сюда попадаем только если exec не удался (команда не найдена) */
        perror("[LOG] Error executing command");
        exit(1);
    } else {
        // Родительский процесс
        printf("[LOG] Parent process started. Waiting for child (PID: %d)\n", getpid());

        // Ожидание завершения порождённого процесса
        /* status - «сырой» код состояния: в нём зашиты и способ завершения
           (нормальный выход, сигнал, остановка), и сам код возврата */
        int status;
        waitpid(pid, &status, 0);
        printf("[LOG] Child process finished (PID: %d)\n", getpid());

        // Вывод кода завершения
        /* WIFEXITED - был ли ребёнок завершён нормально (через exit/return),
           WEXITSTATUS - собственно код возврата из младших 8 бит status.
           Для убитого сигналом процесса код завершения не определён */
        if (WIFEXITED(status)) {
            printf("[LOG] Exit code of %s: %d (PID: %d)\n", command, WEXITSTATUS(status), getpid());
        } else {
            printf("[LOG] Process %s did not exit normally (PID: %d)\n", command, getpid());
        }

        printf("[LOG] Program finished (PID: %d)\n", getpid());
    }

    return 0;
}

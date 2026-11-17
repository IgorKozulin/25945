/* Лабораторная работа №9. Создание двух процессов.
 *
 * Задание: программа создаёт подпроцесс, который исполняет cat(1) длинного
 * файла, а родитель в это время печатает свой текст.
 *   Часть 1 - родитель печатает сразу, параллельно с cat, поэтому строки
 *             родителя и вывод cat могут перемешиваться в любом порядке;
 *   Часть 2 - родитель дожидается завершения подпроцесса (waitpid) и только
 *             после этого печатает последнюю строку.
 */

#include <stdio.h>      /* printf, fprintf, perror, scanf */
#include <stdlib.h>     /* exit                          */
#include <unistd.h>     /* fork, execlp, getpid          */
#include <sys/wait.h>   /* waitpid                       */

int main(int argc, char *argv[]) {
    /* Программе нужен ровно один аргумент - имя файла для cat(1) */
    if (argc != 2) {
        fprintf(stderr, "[LOG] Error: Usage %s <filename> (PID: %d)\n", argv[0], getpid());
        return 1;
    }

    char *filename = argv[1];   /* файл, который будет распечатывать подпроцесс */
    printf("[LOG] Program started. Filename: %s (PID: %d)\n", filename, getpid());

    /* Выбор части задания: 1 - без ожидания, 2 - с ожиданием подпроцесса */
    int choice;
    printf("Choose task part (1 or 2): ");
    scanf("%d", &choice);
    printf("[LOG] User chose part %d (PID: %d)\n", choice, getpid());

    if (choice == 1) {
        /* Часть 1: родитель ничего не ждёт - его вывод идёт параллельно с cat */
        printf("[INFO] Part 1: Child runs cat after parent prints text. (PID: %d)\n", getpid());
    } else if (choice == 2) {
        /* Часть 2: последняя строка родителя должна идти уже после вывода cat */
        printf("[INFO] Part 2: Parent waits for child to finish before printing last line. (PID: %d)\n", getpid());
    } else {
        /* Некорректный номер части - завершаемся, ничего не порождая */
        printf("[LOG] Invalid choice. Exiting. (PID: %d)\n", getpid());
        return 1;
    }

    /* fork(2) создаёт точную копию процесса.
       Родителю возвращается PID ребёнка, ребёнку - 0, при ошибке - -1 */
    pid_t pid = fork();
    if (pid == -1) {
        perror("[LOG] Error forking process");
        return 1;
    }

    if (pid == 0) {
        /* ---------- Ветка подпроцесса ---------- */
        printf("[LOG] Child process started. (PID: %d)\n", getpid());
        printf("[LOG] Child executing cat on %s (PID: %d)\n", filename, getpid());
        /* execlp ищет cat в PATH и ЗАМЕНЯЕТ образ процесса:
           при успехе управление в эту программу уже не вернётся */
        execlp("cat", "cat", filename, NULL);
        perror("[LOG] Error executing cat");   /* дошли сюда - значит exec не удался */
        exit(1);
    } else {
        /* ---------- Ветка родителя ---------- */
        printf("[LOG] Parent process started. (PID: %d)\n", getpid());

        if (choice == 1) {
            /* Часть 1: печатаем, не дожидаясь cat - вывод конкурирует с cat */
            printf("[PARENT] Printing text while child runs... (PID: %d)\n", getpid());
            printf("[PARENT] Parent continues printing. (PID: %d)\n", getpid());
        } else if (choice == 2) {
            /* Часть 2: waitpid блокирует родителя, пока подпроцесс не завершится:
               в status лежит код возврата и причина завершения ребёнка */
            int status;
            waitpid(pid, &status, 0);
            printf("[LOG] Child process finished with status %d (PID: %d)\n", status, getpid());
            printf("[PARENT] Child finished, now printing last line. (PID: %d)\n", getpid());
        }

        /* Итоговая строка печатается только во второй части задания */
        if (choice == 2) {
            printf("[LOG] Program finished (PID: %d)\n", getpid());
        }
    }

    return 0;
}

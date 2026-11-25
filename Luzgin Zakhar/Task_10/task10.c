#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
    pid_t pid;
    int status;
    if (argc < 2) {
        fprintf(stderr, "Использование: %s команда [аргументы...]\n", argv[0]);
        return 1;
    }
    pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        execvp(argv[1], &argv[1]);
        perror("execvp");
        _exit(127);
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        perror("waitpid"); return 1;
    }
    if (WIFEXITED(status)) {
        printf("Код завершения: %d\n", WEXITSTATUS(status));
        return 0;
    }
    if (WIFSIGNALED(status)) {
        printf("Команда завершена сигналом: %d\n", WTERMSIG(status));
        return 0;
    }
    puts("Команда завершилась нестандартным способом");
    return 1;
}

/* Сборка: gcc -std=c99 -Wall -Wextra task10.c -o task10
   Запуск: ./task10 ls -l
   Проверка кода: ./task10 sh -c 'exit 7' */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
    pid_t pid;
    int status;
    int wait_child = 1;
    const char *file;

    if (argc == 3 && strcmp(argv[1], "--no-wait") == 0) {
        wait_child = 0;
        file = argv[2];
    } else if (argc == 2) {
        file = argv[1];
    } else {
        fprintf(stderr, "Использование: %s [--no-wait] файл\n", argv[0]);
        return 1;
    }
    fflush(stdout);
    pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        execlp("cat", "cat", file, (char *)NULL);
        perror("execlp");
        _exit(127);
    }
    printf("Родитель: запущен дочерний процесс PID=%ld\n", (long)pid);
    fflush(stdout);
    if (!wait_child) {
        puts("Родитель: без ожидания (--no-wait)");
        return 0;
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        perror("waitpid"); return 1;
    }
    puts("Родитель: дочерний процесс завершился, это последняя строка");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

/* Сборка: gcc -std=c99 -Wall -Wextra task9.c -o task9
   Запуск: ./task9 long.txt
   Первая часть без ожидания: ./task9 --no-wait long.txt */

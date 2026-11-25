#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

static int send_all(int fd, const char *data, size_t size) {
    while (size > 0) {
        ssize_t n = write(fd, data, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        data += n; size -= (size_t)n;
    }
    return 0;
}

int main(int argc, char **argv) {
    int pipefd[2], status;
    pid_t pid;
    const char *text = argc > 1 ? argv[1] : "Hello, UNIX Pipe! Привет 123\n";
    if (pipe(pipefd) < 0) { perror("pipe"); return 1; }
    pid = fork();
    if (pid < 0) { perror("fork"); close(pipefd[0]); close(pipefd[1]); return 1; }
    if (pid == 0) {
        char buffer[4096];
        ssize_t n;
        close(pipefd[1]);
        while ((n = read(pipefd[0], buffer, sizeof(buffer))) != 0) {
            ssize_t i;
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) { perror("read"); _exit(1); }
            for (i = 0; i < n; ++i) buffer[i] = (char)toupper((unsigned char)buffer[i]);
            if (send_all(STDOUT_FILENO, buffer, (size_t)n) < 0) _exit(1);
        }
        close(pipefd[0]);
        _exit(0);
    }
    close(pipefd[0]);
    if (send_all(pipefd[1], text, strlen(text)) < 0) perror("write");
    close(pipefd[1]);
    if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); return 1; }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

/* Сборка: gcc -std=c99 -Wall -Wextra task25.c -o task25
   Запуск: ./task25 'Hello World!'
   Перевод выполняется для ASCII-символов; UTF-8 кириллица не меняется. */

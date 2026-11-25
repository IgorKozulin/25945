#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int send_all(int fd, const char *p, size_t size) {
    while (size) {
        ssize_t n = write(fd, p, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n; size -= (size_t)n;
    }
    return 0;
}

int main(int argc, char **argv) {
    int sock, i;
    struct sockaddr_un addr;
    if (argc < 2) {
        fprintf(stderr, "Использование: %s путь_к_сокету [текст ...]\n", argv[0]);
        return 1;
    }
    if (strlen(argv[1]) >= sizeof(addr.sun_path)) {
        fputs("Слишком длинный путь к сокету\n", stderr); return 1;
    }
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, argv[1]);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(sock); return 1;
    }
    if (argc > 2) {
        for (i = 2; i < argc; ++i) {
            if ((i > 2 && send_all(sock, " ", 1) < 0) ||
                send_all(sock, argv[i], strlen(argv[i])) < 0) {
                perror("write"); close(sock); return 1;
            }
        }
        if (send_all(sock, "\n", 1) < 0) { perror("write"); close(sock); return 1; }
    } else {
        char buf[4096];
        ssize_t n;
        while ((n = read(STDIN_FILENO, buf, sizeof(buf))) != 0) {
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) { perror("read"); close(sock); return 1; }
            if (send_all(sock, buf, (size_t)n) < 0) {
                perror("write"); close(sock); return 1;
            }
        }
    }
    shutdown(sock, SHUT_WR);
    close(sock);
    return 0;
}

/* Сборка: gcc -std=c99 -Wall -Wextra client.c -o client
   Запуск: ./client /tmp/luzgin_task31.sock 'Hello WoRLD!' */

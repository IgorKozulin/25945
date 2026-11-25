#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS 64
static volatile sig_atomic_t stop_requested = 0;
static volatile sig_atomic_t notified = 0;
static void on_signal(int signo) {
    if (signo == SIGINT || signo == SIGTERM) stop_requested = 1;
    else if (signo == SIGIO) notified = 1;
}

/* SIGIO сообщает о данных, O_NONBLOCK не позволяет заблокироваться на read/accept. */
static int make_async(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) return -1;
    if (fcntl(fd, F_SETOWN, getpid()) < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK | FASYNC);
}

int main(int argc, char **argv) {
    struct sockaddr_un addr;
    struct sigaction sa;
    sigset_t mask, previous, waiting;
    char default_path[104];
    const char *path;
    int listener, clients[MAX_CLIENTS], used = 0, result = 0, k;
    if (argc > 2) { fprintf(stderr, "Использование: %s [путь_к_сокету]\n", argv[0]); return 1; }
    snprintf(default_path, sizeof(default_path), "/tmp/luzgin_task32_%lu.sock", (unsigned long)getuid());
    path = argc == 2 ? argv[1] : default_path;
    if (strlen(path) >= sizeof(addr.sun_path)) { fputs("Слишком длинный путь\n", stderr); return 1; }

    sigemptyset(&mask); sigaddset(&mask, SIGIO); sigaddset(&mask, SIGINT); sigaddset(&mask, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &mask, &previous) < 0) { perror("sigprocmask"); return 1; }
    memset(&sa, 0, sizeof(sa)); sa.sa_handler = on_signal; sigemptyset(&sa.sa_mask);
    sigaction(SIGIO, &sa, NULL); sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    waiting = previous;
    sigdelset(&waiting, SIGIO); sigdelset(&waiting, SIGINT); sigdelset(&waiting, SIGTERM);

    listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) { perror("socket"); return 1; }
    memset(&addr, 0, sizeof(addr)); addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    unlink(path);
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        listen(listener, 16) < 0 || make_async(listener) < 0) {
        perror("bind/listen/make_async"); close(listener); unlink(path); return 1;
    }
    printf("Асинхронный сервер SIGIO: %s (Ctrl+C — выход)\n", path);
    fflush(stdout);

    while (!stop_requested) {
        int changed;
        do {
            int i;
            changed = 0;
            for (;;) {
                int c = accept(listener, NULL, NULL);
                if (c < 0) {
                    if (errno == EINTR) continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                    perror("accept"); result = 1; stop_requested = 1; break;
                }
                if (used == MAX_CLIENTS || make_async(c) < 0) close(c);
                else clients[used++] = c;
                changed = 1;
            }
            for (i = 0; i < used;) {
                int remove_client = 0;
                for (;;) {
                    char buf[4096];
                    ssize_t n = read(clients[i], buf, sizeof(buf));
                    ssize_t j;
                    if (n > 0) {
                        for (j = 0; j < n; ++j)
                            buf[j] = (char)toupper((unsigned char)buf[j]);
                        if (fwrite(buf, 1, (size_t)n, stdout) != (size_t)n) {
                            result = 1; stop_requested = 1;
                        }
                        fflush(stdout); changed = 1;
                    } else if (n == 0) {
                        remove_client = 1; changed = 1; break;
                    } else if (errno == EINTR) {
                        continue;
                    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    } else {
                        remove_client = 1; changed = 1; break;
                    }
                    if (stop_requested) break;
                }
                if (remove_client) {
                    close(clients[i]); clients[i] = clients[--used];
                } else ++i;
            }
        } while (changed && !stop_requested);
        if (!stop_requested) {
            notified = 0;
            sigsuspend(&waiting);  /* Ждём сигнал, без select и poll. */
        }
    }
    for (k = 0; k < used; ++k) close(clients[k]);
    close(listener); unlink(path);
    sigprocmask(SIG_SETMASK, &previous, NULL);
    return result;
}

/* Сборка: gcc -std=c99 -Wall -Wextra server.c -o server
   Терминал 1: ./server /tmp/luzgin_task32.sock
   Терминалы 2 и 3: ./client /tmp/luzgin_task32.sock 'Hello' */

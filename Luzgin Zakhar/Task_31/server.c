#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS 64
static volatile sig_atomic_t stopped = 0;
static void finish(int signo) { (void)signo; stopped = 1; }

int main(int argc, char **argv) {
    struct pollfd fds[MAX_CLIENTS+1];
    struct sockaddr_un addr;
    struct sigaction sa;
    char default_path[104];
    const char *path;
    nfds_t used = 1, i;
    int listener, rc = 0;
    if (argc > 2) { fprintf(stderr, "Использование: %s [путь_к_сокету]\n", argv[0]); return 1; }
    snprintf(default_path, sizeof(default_path), "/tmp/luzgin_task31_%lu.sock", (unsigned long)getuid());
    path = argc == 2 ? argv[1] : default_path;
    if (strlen(path) >= sizeof(addr.sun_path)) { fputs("Слишком длинный путь\n", stderr); return 1; }
    listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) { perror("socket"); return 1; }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    unlink(path);
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(listener, 16) < 0) {
        perror("bind/listen"); close(listener); return 1;
    }
    memset(&sa, 0, sizeof(sa)); sa.sa_handler = finish; sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    fds[0].fd = listener; fds[0].events = POLLIN;
    printf("Ожидаю клиентов: %s (Ctrl+C — выход)\n", path); fflush(stdout);
    while (!stopped) {
        int ready = poll(fds, used, -1);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) { perror("poll"); rc = 1; break; }
        if (fds[0].revents & POLLIN) {
            int c = accept(listener, NULL, NULL);
            if (c >= 0) {
                if (used <= MAX_CLIENTS) {
                    fds[used].fd = c; fds[used].events = POLLIN; fds[used].revents = 0;
                    ++used;
                } else close(c);
            } else if (errno != EINTR) perror("accept");
        }
        for (i = 1; i < used;) {
            if (fds[i].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
                char buf[4096];
                ssize_t n = read(fds[i].fd, buf, sizeof(buf));
                if (n > 0) {
                    ssize_t j;
                    for (j = 0; j < n; ++j) buf[j] = (char)toupper((unsigned char)buf[j]);
                    fwrite(buf, 1, (size_t)n, stdout);
                    fflush(stdout);
                } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                    close(fds[i].fd);
                    fds[i] = fds[--used];
                    continue;
                }
            }
            ++i;
        }
    }
    for (i = 1; i < used; ++i) close(fds[i].fd);
    close(listener);
    unlink(path);
    return rc;
}

/* Сборка: gcc -std=c99 -Wall -Wextra server.c -o server
   Терминал 1: ./server /tmp/luzgin_task31.sock
   Терминалы 2 и 3: ./client /tmp/luzgin_task31.sock 'HeLLo Client' */

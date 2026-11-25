#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static volatile sig_atomic_t stopped = 0;
static void finish(int signo) { (void)signo; stopped = 1; }

int main(int argc, char **argv) {
    int listener, client = -1, rc = 0;
    struct sockaddr_un addr;
    struct sigaction sa;
    char default_path[104];
    const char *path;
    if (argc > 2) { fprintf(stderr, "Использование: %s [путь_к_сокету]\n", argv[0]); return 1; }
    snprintf(default_path, sizeof(default_path), "/tmp/luzgin_task30_%lu.sock", (unsigned long)getuid());
    path = argc == 2 ? argv[1] : default_path;
    if (strlen(path) >= sizeof(addr.sun_path)) { fputs("Слишком длинный путь\n", stderr); return 1; }
    listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) { perror("socket"); return 1; }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    unlink(path);
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(listener, 5) < 0) {
        perror("bind/listen"); close(listener); return 1;
    }
    memset(&sa, 0, sizeof(sa)); sa.sa_handler = finish; sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    printf("Ожидаю одного клиента: %s\n", path); fflush(stdout);
    while (!stopped) {
        client = accept(listener, NULL, NULL);
        if (client < 0 && errno == EINTR) continue;
        if (client < 0) { perror("accept"); rc = 1; break; }
        break;
    }
    if (client >= 0) {
        char buf[4096];
        ssize_t n;
        while ((n = read(client, buf, sizeof(buf))) != 0) {
            ssize_t j;
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) { perror("read"); rc = 1; break; }
            for (j = 0; j < n; ++j) buf[j] = (char)toupper((unsigned char)buf[j]);
            if (fwrite(buf, 1, (size_t)n, stdout) != (size_t)n) { rc = 1; break; }
            fflush(stdout);
        }
        close(client);
    }
    close(listener);
    unlink(path);
    return rc;
}

/* Сборка: gcc -std=c99 -Wall -Wextra server.c -o server
   Терминал 1: ./server /tmp/luzgin_task30.sock
   Терминал 2: ./client /tmp/luzgin_task30.sock 'HeLLo WoRLD' */

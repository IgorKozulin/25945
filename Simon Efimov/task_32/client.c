#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./socket"

static ssize_t write_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, p + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

int main(int argc, char *argv[]) {
    const char *name = (argc > 1) ? argv[1] : "client";

    signal(SIGPIPE, SIG_IGN);

    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("[%s] Connected to server.\n", name);

    const char *messages[] = {
        "hello from client\n",
        "this is a test message\n",
        "asynchronous io is awesome\n"
    };

    char buf[256];
    for (size_t i = 0; i < sizeof(messages) / sizeof(messages[0]); i++) {
        int len = snprintf(buf, sizeof(buf), "[%s] %s", name, messages[i]);
        if (write_all(sock_fd, buf, (size_t)len) == -1) {
            perror("write");
            break;
        }
        printf("[%s] Sent: %s", name, buf);
        usleep(500000);  /* 0.5 s: make multiplexing visible */
    }

    printf("[%s] Closing.\n", name);
    close(sock_fd);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./socket"

/* Write exactly len bytes, handling partial writes */
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

int main(void) {
    /* Ignore SIGPIPE: write() should return EPIPE instead */
    signal(SIGPIPE, SIG_IGN);

    /* 1. Create socket */
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return 1;
    }

    /* 2. Connect to server */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Connected to server. Sending messages...\n");

    /* 3. Send messages */
    const char *messages[] = {
        "Hello World!\n",
        "This is a Test Message!!!\n",
        "Unix domain sockets are awesome.\n"
    };

    for (size_t i = 0; i < sizeof(messages) / sizeof(messages[0]); i++) {
        printf("[CLIENT] Sending: %s", messages[i]);
        if (write_all(sock_fd, messages[i], strlen(messages[i])) == -1) {
            perror("write");
            break;
        }
    }

    printf("[CLIENT] All messages sent. Closing connection...\n");

    close(sock_fd);
    return 0;
}

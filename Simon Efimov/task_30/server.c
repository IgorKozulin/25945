#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./socket"
#define BUF_SIZE 1024

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
    unlink(SOCKET_PATH);

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    printf("Server started. Waiting for connections on %s...\n", SOCKET_PATH);
    fflush(stdout);

    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    printf("Client connected!\n");
    fflush(stdout);

    char buf[BUF_SIZE];
    ssize_t n;
    while (1) {
        n = read(client_fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read");
            break;
        }
        if (n == 0) break;  /* EOF */

        char out[BUF_SIZE];
        ssize_t out_len = 0;
        for (ssize_t i = 0; i < n; i++) {
            unsigned char c = (unsigned char)buf[i];
            if (isalpha(c)) {
                out[out_len++] = (char)toupper(c);
            }
            /* else: skip special characters */
        }
        if (out_len > 0) {
            if (write_all(STDOUT_FILENO, out, (size_t)out_len) == -1) {
                perror("write");
                break;
            }
        }
    }

    printf("\nClient disconnected. Shutting down server.\n");

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}

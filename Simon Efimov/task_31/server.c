#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

#define SOCKET_PATH   "./socket"
#define MAX_CLIENTS   10
#define BUF_SIZE      1024

static volatile sig_atomic_t stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    stop = 1;
}

int main(void) {
    /* Remove old socket file */
    unlink(SOCKET_PATH);

    /* Create socket */
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    /* Bind */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    /* Listen */
    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    /* Install SIGINT handler */
    signal(SIGINT, on_sigint);

    printf("[SERVER] Started. Waiting for connections on %s...\n", SOCKET_PATH);
    printf("[SERVER] Press Ctrl+C to stop.\n");
    fflush(stdout);

    /* Prepare poll array */
    struct pollfd fds[MAX_CLIENTS + 1];
    int nfds = 1;  /* only the listening socket for now */

    for (int i = 0; i <= MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    char buf[BUF_SIZE];

    while (!stop) {
        int ret = poll(fds, nfds, 1000);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        if (ret == 0) continue;  /* timeout */

        /* Check listening socket */
        if (fds[0].revents & POLLIN) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd != -1) {
                /* Find a free slot */
                int slot = -1;
                for (int i = 1; i <= MAX_CLIENTS; i++) {
                    if (fds[i].fd == -1) { slot = i; break; }
                }
                if (slot == -1) {
                    printf("[SERVER] Too many clients, rejecting fd=%d\n", client_fd);
                    close(client_fd);
                } else {
                    fds[slot].fd = client_fd;
                    fds[slot].events = POLLIN;
                    fds[slot].revents = 0;
                    if (slot >= nfds) nfds = slot + 1;
                    printf("[SERVER] New client connected (fd=%d). Total: %d\n",
                           client_fd, slot);
                    fflush(stdout);
                }
            }
        }

        /* Check client sockets */
        for (int i = 1; i < nfds; i++) {
            if (fds[i].fd == -1) continue;
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;

            ssize_t n = read(fds[i].fd, buf, sizeof(buf));
            if (n > 0) {
                for (ssize_t k = 0; k < n; k++) {
                    buf[k] = (char)toupper((unsigned char)buf[k]);
                }
                printf("[Client %d]: ", fds[i].fd);
                fflush(stdout);
                if (write(STDOUT_FILENO, buf, (size_t)n) == -1) {
                    perror("write");
                }
                if (buf[n - 1] != '\n') {
                    printf("\n");
                }
                fflush(stdout);
            } else if (n == 0) {
                printf("[SERVER] Client %d disconnected.\n", fds[i].fd);
                close(fds[i].fd);
                fds[i].fd = -1;
                fds[i].events = 0;
                fds[i].revents = 0;

                /* Shrink nfds if possible */
                while (nfds > 1 && fds[nfds - 1].fd == -1) nfds--;
                printf("[SERVER] Remaining clients: %d\n", nfds - 1);
                fflush(stdout);
            } else {
                if (errno == EINTR) continue;
                perror("read");
                close(fds[i].fd);
                fds[i].fd = -1;
                fds[i].events = 0;
                fds[i].revents = 0;
                while (nfds > 1 && fds[nfds - 1].fd == -1) nfds--;
            }
        }
    }

    /* Cleanup */
    for (int i = 0; i < nfds; i++) {
        if (fds[i].fd != -1) close(fds[i].fd);
    }
    close(server_fd);
    unlink(SOCKET_PATH);
    printf("[SERVER] Shut down.\n");
    return 0;
}

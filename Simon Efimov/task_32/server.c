#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <aio.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH   "./socket"
#define MAX_CLIENTS   100

static volatile sig_atomic_t keep_running = 1;

static void on_signal(int sig) {
    (void)sig;
    keep_running = 0;
}

struct client {
    int fd;
    struct aiocb cb;
    char byte;
    int active;
    int id;
};

/* Queue an async read of exactly 1 byte for the client */
static void arm_read(struct client *c) {
    memset(&c->cb, 0, sizeof(c->cb));
    c->cb.aio_fildes = c->fd;
    c->cb.aio_buf = &c->byte;
    c->cb.aio_nbytes = 1;            /* Solaris: 1 byte guarantees completion */
    c->cb.aio_offset = 0;
    c->cb.aio_sigevent.sigev_notify = SIGEV_NONE;

    if (aio_read(&c->cb) == -1) {
        if (errno != EAGAIN && errno != EINPROGRESS) {
            perror("aio_read");
        }
    }
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

    /* Make the listening socket non-blocking for accept() */
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    printf("[SERVER] AIO server started on %s\n", SOCKET_PATH);
    printf("[SERVER] Press Ctrl+C to stop.\n");
    fflush(stdout);

    struct client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].active = 0;
        clients[i].id = 0;
    }
    int active_count = 0;

    while (keep_running) {
        /* Accept all pending connections */
        while (1) {
            int new_fd = accept(server_fd, NULL, NULL);
            if (new_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                perror("accept");
                break;
            }

            /* CRITICAL: POSIX AIO on Solaris needs blocking client sockets */
            int cflags = fcntl(new_fd, F_GETFL, 0);
            fcntl(new_fd, F_SETFL, cflags & ~O_NONBLOCK);

            int slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == -1) { slot = i; break; }
            }
            if (slot == -1) {
                printf("[SERVER] Too many clients, rejecting fd=%d\n", new_fd);
                close(new_fd);
                continue;
            }

            clients[slot].fd = new_fd;
            clients[slot].active = 1;
            clients[slot].id = slot + 1;
            active_count++;
            arm_read(&clients[slot]);

            printf("[SERVER] Client %d connected (slot=%d fd=%d). Total: %d\n",
                   clients[slot].id, slot, new_fd, active_count);
            fflush(stdout);
        }

        /* Poll AIO requests */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == -1 || !clients[i].active) continue;

            int err = aio_error(&clients[i].cb);
            if (err == EINPROGRESS || err == EAGAIN) continue;

            if (err != 0 && err != ECANCELED) {
                printf("[SERVER] Client %d error: %s\n",
                       clients[i].id, strerror(err));
                close(clients[i].fd);
                clients[i].fd = -1;
                clients[i].active = 0;
                active_count--;
                continue;
            }

            ssize_t n = aio_return(&clients[i].cb);
            if (n == 0) {
                printf("[SERVER] Client %d disconnected (EOF). Remaining: %d\n",
                       clients[i].id, active_count - 1);
                close(clients[i].fd);
                clients[i].fd = -1;
                clients[i].active = 0;
                active_count--;
                fflush(stdout);
            } else if (n > 0) {
                char ch = (char)toupper((unsigned char)clients[i].byte);
                write(STDOUT_FILENO, &ch, 1);
                arm_read(&clients[i]);
            } else {
                perror("aio_return");
                close(clients[i].fd);
                clients[i].fd = -1;
                clients[i].active = 0;
                active_count--;
            }
        }

        usleep(10000);  /* 10 ms: keep CPU usage low */
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1) {
            aio_cancel(clients[i].fd, &clients[i].cb);
            close(clients[i].fd);
        }
    }
    close(server_fd);
    unlink(SOCKET_PATH);
    printf("\n[SERVER] Shut down.\n");
    return 0;
}

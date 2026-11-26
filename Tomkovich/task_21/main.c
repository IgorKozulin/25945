#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define BUFFER_SIZE 1024

int main(void)
{
    int pipefd[2];
    pid_t pid;
    int status;

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    if (fflush(NULL) != 0) {
        perror("fflush");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    pid = fork();

    if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    if (pid == 0) {
        close(pipefd[1]);

        printf("[CHILD] Waiting for data from parent...\n");
        fflush(stdout);

        char buffer[BUFFER_SIZE];
        ssize_t bytesRead;

        while (1) {
            bytesRead = read(pipefd[0], buffer, BUFFER_SIZE - 1);

            if (bytesRead == 0) {
                break;
            }

            if (bytesRead == -1) {
                if (errno == EINTR) {
                    continue;
                }

                perror("read");
                close(pipefd[0]);
                _exit(1);
            }

            for (ssize_t i = 0; i < bytesRead; i++) {
                buffer[i] = (char)toupper((unsigned char)buffer[i]);
            }

            buffer[bytesRead] = '\0';

            printf("[CHILD] Received: %s", buffer);
            fflush(stdout);
        }

        close(pipefd[0]);
        fflush(stdout);
        _exit(0);
    }

    close(pipefd[0]);

    printf("[PARENT] Pipe connection established.\n");
    printf("[PARENT] Enter text (empty line or Ctrl+D to exit):\n");

    char input[BUFFER_SIZE];

    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n[PARENT] End of input.\n");
            break;
        }

        if (input[0] == '\n') {
            printf("[PARENT] Empty line. Stopping.\n");
            break;
        }

        size_t length = strlen(input);
        size_t sent = 0;

        while (sent < length) {
            ssize_t result = write(pipefd[1],
                                   input + sent,
                                   length - sent);

            if (result == -1) {
                if (errno == EINTR) {
                    continue;
                }

                perror("write");
                break;
            }

            if (result == 0) {
                break;
            }

            sent += (size_t)result;
        }

        if (sent != length) {
            break;
        }
    }

    close(pipefd[1]);

    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return 1;
        }
    }

    if (WIFEXITED(status)) {
        printf("[PARENT] Child exit code: %d\n",
               WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status)) {
        printf("[PARENT] Child terminated by signal: %d\n",
               WTERMSIG(status));
    }

    printf("[PARENT] Child finished. Parent exiting.\n");

    return 0;
}

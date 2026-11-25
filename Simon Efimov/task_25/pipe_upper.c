#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

int main(void) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Child: read from pipe, convert to upper, print, count chars */
        close(pipefd[1]);  /* close write end */

        char buf[1024];
        ssize_t n;
        long total_chars = 0;

        while ((n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            total_chars += n;

            for (ssize_t i = 0; i < n; i++) {
                buf[i] = (char)toupper((unsigned char)buf[i]);
            }
            printf("%s", buf);
            fflush(stdout);
        }

        /* Report total number of characters */
        printf("\n[CHILD] Total characters received: %ld\n", total_chars-1);
        fflush(stdout);

        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    } else {
        /* Parent: read from stdin, write to pipe */
        close(pipefd[0]);  /* close read end */

        printf("[PARENT] Pipe established. Enter text (empty line or Ctrl+D to quit):\n");

        char line[1024];
        while (1) {
            printf("> ");
            fflush(stdout);

            if (fgets(line, sizeof(line), stdin) == NULL) {
                printf("\n[PARENT] EOF — finishing input.\n");
                break;
            }

            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
                len--;
            }

            if (len == 0) {
                printf("[PARENT] Empty line — finishing input.\n");
                break;
            }

            line[len] = '\n';
            line[len + 1] = '\0';
            if (write(pipefd[1], line, len + 1) == -1) {
                perror("write");
                break;
            }
        }

        close(pipefd[1]);
        wait(NULL);
        printf("[PARENT] Child finished. Exiting.\n");
    }

    return 0;
}

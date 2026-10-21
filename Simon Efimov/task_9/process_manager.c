#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    printf("[PARENT] Starting program. My PID: %d\n", getpid());

    /* Create child process */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Child process: run cat on the file */
        printf("[CHILD] I am the child. My PID: %d\n", getpid());

        execlp("cat", "cat", argv[1], NULL);

        /* If execlp fails, we reach this line */
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        /* Parent process */
        printf("[PARENT] I am the parent. Child PID: %d\n", pid);
        printf("[PARENT] Printing some text while child is working...\n");

        printf("[PARENT] Waiting for child to finish...\n");
        int status;
        pid_t w = waitpid(pid, &status, 0);
        if (w == -1) {
            perror("waitpid");
            return 1;
        }

        if (WIFEXITED(status)) {
            printf("[PARENT] Child exited with status: %d\n",
                   WEXITSTATUS(status));
        } else {
            printf("[PARENT] Child terminated abnormally\n");
        }

        /* This line must be printed strictly AFTER child finishes */
        printf("[PARENT] Child has finished. This is the last line printed by parent.\n");
    }

    return 0;
}

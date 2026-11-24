#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    pid_t pid;
    int status;
    int no_wait = argc == 3 && strcmp(argv[1], "--no-wait") == 0;

    if (argc != 2 && !no_wait) {
        fprintf(stderr, "Usage: %s [--no-wait] file\n", argv[0]);
        return 1;
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        execlp("cat", "cat", "--", argv[argc - 1], (char *)NULL);
        perror("execlp");
        _exit(127);
    }

    printf("Parent is working\n");
    fflush(stdout);
    if (!no_wait) {
        while (waitpid(pid, &status, 0) == -1) {
            if (errno == EINTR)
                continue;
            perror("waitpid");
            return 1;
        }
    }
    printf("Last parent line\n");
    if (no_wait)
        return 0;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

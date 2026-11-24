#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    pid_t pid;
    int status;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s command [arguments...]\n", argv[0]);
        return 1;
    }
    pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        execvp(argv[1], &argv[1]);
        int code = errno == ENOENT ? 127 : 126;
        perror("execvp");
        _exit(code);
    }
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR)
            continue;
        perror("waitpid");
        return 1;
    }
    if (WIFEXITED(status)) {
        printf("Exit code: %d\n", WEXITSTATUS(status));
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        printf("Signal: %d\n", WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }
    return 1;
}

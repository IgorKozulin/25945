#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Usage: process_manager file\n"); return 1; }
    if (fflush(NULL) == EOF) { perror("fflush"); return 1; }
    pid_t child = fork();
    if (child < 0) { perror("fork"); return 1; }
    if (!child) {
        /* Feed cat through stdin, so even a filename starting with '-' is safe. */
        FILE *file = fopen(argv[1], "r");
        if (!file) { perror(argv[1]); _exit(1); }
        if (dup2(fileno(file), STDIN_FILENO) < 0) { perror("dup2"); _exit(1); }
        if (fileno(file) != STDIN_FILENO) fclose(file);
        execlp("cat", "cat", (char *)NULL);
        perror("execlp cat"); _exit(127);
    }
    printf("[PARENT] Child PID: %ld; parent is working.\n", (long)child);
    fflush(stdout);
    int status;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        perror("waitpid"); return 1;
    }
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    printf("[PARENT] Child status: %d\n", code);
    puts("[PARENT] Child has finished. This is the last line.");
    return code;
}

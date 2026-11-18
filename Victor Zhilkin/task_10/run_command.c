#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
    return EXIT_FAILURE;
  }

  pid_t pid = fork();

  if (pid < 0) {
    perror("Fork failed");
    return EXIT_FAILURE;
  }

  if (pid == 0) {
    execvp(argv[1], &argv[1]);

    perror("Execvp failed");
    exit(127);
  } else {
    int status;
    printf("[PARENT] Starting command: %s\n", argv[1]);

    if (waitpid(pid, &status, 0) < 0) {
      perror("Waitpid failed");
      return EXIT_FAILURE;
    }

    if (WIFEXITED(status)) {
      printf("[PARENT] Command exited normally with code: %d\n",
             WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      int sig = WTERMSIG(status);
      printf("[PARENT] Command terminated by signal: %d (%s)\n", sig,
             strsignal(sig));
    }
  }

  return EXIT_SUCCESS;
}

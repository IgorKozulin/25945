#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  pid_t pid;
  int status;
  int exitCode;

  if (argc < 2 || argc > 2) {
    fprintf(stderr, "Usage: process_manager file\n");
    return 1;
  }

  if (fflush(NULL) != 0) {
    perror("fflush");
    return 1;
  }

  pid = fork();

  if (pid == -1) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    FILE *input = fopen(argv[1], "r");

    if (input == NULL) {
      perror(argv[1]);
      _exit(1);
    }

    int fd = fileno(input);

    if (dup2(fd, STDIN_FILENO) == -1) {
      perror("dup2");
      _exit(1);
    }

    if (fd != STDIN_FILENO) {
      fclose(input);
    }

    execlp("cat", "cat", NULL);

    perror("execlp cat");
    _exit(127);
  }

  printf("[PARENT] Process started. Child PID: %ld\n", (long)pid);
  printf("[PARENT] Parent is doing some work...\n");
  fflush(stdout);

  while (1) {
    if (waitpid(pid, &status, 0) != -1) {
      break;
    }

    if (errno != EINTR) {
      perror("waitpid");
      return 1;
    }
  }

  if (WIFEXITED(status)) {
    exitCode = WEXITSTATUS(status);
  } else {
    exitCode = 128 + WTERMSIG(status);
  }

  printf("[PARENT] Child exit code: %d\n", exitCode);
  printf("[PARENT] Child process finished. Final message.\n");

  return exitCode;
}

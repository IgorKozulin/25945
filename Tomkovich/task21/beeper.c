#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  sigset_t signalSet;
  unsigned long long count = 0;

  if (sigemptyset(&signalSet) == -1) {
    perror("sigemptyset");
    return 1;
  }

  if (sigaddset(&signalSet, SIGINT) == -1 ||
      sigaddset(&signalSet, SIGQUIT) == -1) {
    perror("sigaddset");
    return 1;
  }

  if (sigprocmask(SIG_BLOCK, &signalSet, NULL) == -1) {
    perror("sigprocmask");
    return 1;
  }

  printf("[PROCESS] PID: %ld\n", (long)getpid());
  printf("[PROCESS] Ctrl+C - beep, Ctrl+\\ - exit\n");
  fflush(stdout);

  while (1) {
    int signalNumber = sigwaitinfo(&signalSet, NULL);

    if (signalNumber == -1) {
      if (errno == EINTR) {
        continue;
      }

      perror("sigwaitinfo");
      return 1;
    }

    if (signalNumber == SIGINT) {
      putchar('\a');
      fflush(stdout);
      count++;
    } else if (signalNumber == SIGQUIT) {
      printf("\n[PROCESS] Beep count: %llu\n", count);
      printf("[PROCESS] Exiting...\n");
      break;
    }
  }

  return 0;
}

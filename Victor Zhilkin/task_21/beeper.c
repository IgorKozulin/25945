#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  sigset_t signals;
  unsigned long long count = 0;

  sigemptyset(&signals);
  sigaddset(&signals, SIGINT);
  sigaddset(&signals, SIGQUIT);
  if (sigprocmask(SIG_BLOCK, &signals, NULL) == -1) {
    perror("sigprocmask");
    return EXIT_FAILURE;
  }
  printf("PID: %ld. Ctrl-C: beep. Ctrl-\\: print count and exit.\n",
         (long)getpid());
  fflush(stdout);

  for (;;) {
    int number = sigwaitinfo(&signals, NULL);

    if (number == -1) {
      if (errno == EINTR)
        continue;
      perror("sigwaitinfo");
      return EXIT_FAILURE;
    }
    if (number == SIGQUIT) {
      printf("\nBeeps: %lu\n", count);
      break;
    }
    putchar('\a');
    fflush(stdout);
    ++count;
  }
  return EXIT_SUCCESS;
}

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#define MORSE_FILE "morse.txt"

static FILE *morse_file;
static FILE *tty_out;
static int terminal_fd = -1;
static struct termios saved_termios;
static int termios_saved;

static void cleanup(void) {
  if (termios_saved)
    tcsetattr(terminal_fd, TCSANOW, &saved_termios);
  if (morse_file != NULL)
    fclose(morse_file);
  if (tty_out != NULL)
    fclose(tty_out);
}

static int write_symbol(char symbol) {
  if (fputc(symbol, morse_file) == EOF || fflush(morse_file) == EOF) {
    perror("write " MORSE_FILE);
    return -1;
  }
  return 0;
}

static void beep(void) {
  FILE *out = tty_out != NULL ? tty_out : stdout;
  fputc('\a', out);
  fflush(out);
}

int main(void) {
  sigset_t signals;
  unsigned long long count = 0;
  unsigned long long ticks = 0;

  sigemptyset(&signals);
  sigaddset(&signals, SIGINT);
  sigaddset(&signals, SIGQUIT);
  sigaddset(&signals, SIGALRM);
  sigaddset(&signals, SIGTERM);
  sigaddset(&signals, SIGHUP);
  if (sigprocmask(SIG_BLOCK, &signals, NULL) == -1) {
    perror("sigprocmask");
    return EXIT_FAILURE;
  }

  tty_out = fopen("/dev/tty", "w");
  terminal_fd = tty_out != NULL ? fileno(tty_out) : STDIN_FILENO;
  if (tcgetattr(terminal_fd, &saved_termios) == 0) {
    struct termios terminal = saved_termios;
    terminal.c_lflag &= ~(tcflag_t)ECHO;
#ifdef ECHOCTL
    terminal.c_lflag &= ~(tcflag_t)ECHOCTL;
#endif
    if (tcsetattr(terminal_fd, TCSANOW, &terminal) == 0)
      termios_saved = 1;
  }
  atexit(cleanup);

  morse_file = fopen(MORSE_FILE, "w");
  if (morse_file == NULL) {
    perror(MORSE_FILE);
    return EXIT_FAILURE;
  }

  struct itimerval timer;
  timer.it_value.tv_sec = 1;
  timer.it_value.tv_usec = 0;
  timer.it_interval.tv_sec = 1;
  timer.it_interval.tv_usec = 0;
  if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
    perror("setitimer");
    return EXIT_FAILURE;
  }

  printf("PID: %ld. Ctrl-C: beep, Ctrl-\\: print count and exit.\n",
         (long)getpid());
  printf("Morse log: %s ('.' each second, '-' on Ctrl-C)\n", MORSE_FILE);
  fflush(stdout);

  for (;;) {
    int number = sigwaitinfo(&signals, NULL);

    if (number == -1) {
      if (errno == EINTR)
        continue;
      perror("sigwaitinfo");
      return EXIT_FAILURE;
    }
    if (number == SIGQUIT || number == SIGTERM || number == SIGHUP) {
      fputc('\n', morse_file);
      if (number == SIGQUIT)
        printf("\nSignals: %llu, ticks: %llu\n", count, ticks);
      fflush(stdout);
      break;
    }
    if (number == SIGALRM) {
      if (write_symbol('.') == -1)
        return EXIT_FAILURE;
      ++ticks;
      continue;
    }
    if (write_symbol('-') == -1)
      return EXIT_FAILURE;
    beep();
    ++count;
  }
  return EXIT_SUCCESS;
}

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define BUF_SIZE 1024

static struct termios orig_termios;
static int raw_enabled;

static void restore_terminal(void) {
  if (raw_enabled) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    raw_enabled = 0;
  }
}

static void on_signal(int sig) {
  restore_terminal();
  signal(sig, SIG_DFL);
  raise(sig);
}

static void setup_terminal(void) {
  struct termios raw;

  if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    return;

  atexit(restore_terminal);
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  signal(SIGHUP, on_signal);
  signal(SIGQUIT, on_signal);

  raw = orig_termios;
  raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON | IEXTEN);
  raw.c_iflag &= ~(tcflag_t)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
  raw.c_cflag |= CS8;
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    return;

  raw_enabled = 1;
}

static void erase_last(char *line, size_t *len) {
  if (*len == 0)
    return;

  size_t i = *len - 1;
  while (i > 0 && ((unsigned char)line[i] & 0xC0) == 0x80)
    --i;
  *len = i;
  (void)write(STDOUT_FILENO, "\b \b", 3);
}

static void skip_escape(void) {
  struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};

  for (int i = 0; i < 16; i++) {
    if (poll(&pfd, 1, 50) <= 0)
      return;

    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
      return;

    if (i == 0 && c != '[' && c != 'O')
      return;
    if (i > 0 && c >= '@' && c <= '~')
      return;
  }
}

static ssize_t read_line_raw(char *line, size_t capacity) {
  size_t len = 0;

  for (;;) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);

    if (n == -1) {
      if (errno == EINTR)
        continue;
      return -2;
    }
    if (n == 0) {
      if (len == 0)
        return -1;
      line[len] = '\0';
      return (ssize_t)len;
    }
    if (c == '\r' || c == '\n') {
      (void)write(STDOUT_FILENO, "\n", 1);
      line[len] = '\0';
      return (ssize_t)len;
    }
    if (c == 0x04) {
      if (len == 0)
        return -1;
      continue;
    }
    if (c == (unsigned char)orig_termios.c_cc[VERASE] || c == 127 || c == 8) {
      erase_last(line, &len);
      continue;
    }
    if (c == (unsigned char)orig_termios.c_cc[VKILL] || c == 21) {
      while (len > 0)
        erase_last(line, &len);
      continue;
    }
    if (c == 23) {
      while (len > 0 && line[len - 1] == ' ')
        erase_last(line, &len);
      while (len > 0 && line[len - 1] != ' ')
        erase_last(line, &len);
      continue;
    }
    if (c == 0x1b) {
      skip_escape();
      continue;
    }
    if (c < 0x20 || c == 0x7f)
      continue;
    if (len + 1 < capacity) {
      line[len++] = (char)c;
      (void)write(STDOUT_FILENO, &c, 1);
    }
  }
}

static ssize_t read_line_fallback(char *line, size_t capacity) {
  if (fgets(line, (int)capacity, stdin) == NULL)
    return ferror(stdin) ? -2 : -1;

  size_t len = strlen(line);
  size_t out = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)line[i];
    if (c == '\n' || c == '\r')
      continue;
    if (c >= 0x20 && c != 0x7f)
      line[out++] = (char)c;
  }
  line[out] = '\0';
  return (ssize_t)out;
}

int main(void) {
  int pipefd[2];
  pid_t pid;

  if (pipe(pipefd) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  pid = fork();
  if (pid == -1) {
    perror("fork");
    close(pipefd[0]);
    close(pipefd[1]);
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {
    close(pipefd[1]);

    printf("[CHILD] Запущен. Ожидаю данные от родителя...\n");
    fflush(stdout);

    char buffer[BUF_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes_read] = '\0';

      for (ssize_t i = 0; i < bytes_read; i++) {
        buffer[i] = (char)toupper((unsigned char)buffer[i]);
      }

      printf("[CHILD] Получено и преобразовано: %s", buffer);
      fflush(stdout);
    }

    if (bytes_read == -1) {
      perror("read");
      close(pipefd[0]);
      exit(EXIT_FAILURE);
    }

    close(pipefd[0]);
    exit(EXIT_SUCCESS);

  } else {
    close(pipefd[0]);

    printf("[PARENT] Связь через канал установлена!\n");
    printf("[PARENT] Введите текст (пустая строка или Ctrl+D — выход):\n");
    fflush(stdout);

    setup_terminal();

    char input[BUF_SIZE];

    while (1) {
      printf("> ");
      fflush(stdout);

      ssize_t len = raw_enabled ? read_line_raw(input, sizeof(input))
                                : read_line_fallback(input, sizeof(input));

      if (len == -2) {
        perror("read");
        break;
      }

      if (len == -1) {
        printf("\n[PARENT] Получен EOF (Ctrl+D) — завершаю ввод.\n");
        break;
      }

      if (len == 0) {
        printf("[PARENT] Пустая строка — завершаю ввод.\n");
        break;
      }

      if (write(pipefd[1], input, (size_t)len) == -1 ||
          write(pipefd[1], "\n", 1) == -1) {
        perror("write");
        break;
      }
    }

    close(pipefd[1]);

    wait(NULL);
    printf("[PARENT] Потомок завершился. Родитель тоже завершает работу.\n");
  }

  return EXIT_SUCCESS;
}

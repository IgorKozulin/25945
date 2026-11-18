#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAX_BUFFER 4096
#define LINE_LIMIT 40

struct termios orig_termios;
char buffer[MAX_BUFFER];
int pos = 0;

char erase_char = 127;
char kill_char = 21;

void disable_raw_mode(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode(void) {
  if (tcgetattr(STDIN_FILENO, &orig_termios) < 0) {
    perror("tcgetattr failed");
    exit(EXIT_FAILURE);
  }

  atexit(disable_raw_mode);

  struct termios raw = orig_termios;

  if (raw.c_cc[VERASE] != 0)
    erase_char = raw.c_cc[VERASE];
  if (raw.c_cc[VKILL] != 0)
    kill_char = raw.c_cc[VKILL];

  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
    perror("tcsetattr failed");
    exit(EXIT_FAILURE);
  }
}

void clear_screen_below(void) { printf("\033[J"); }

void redraw(void) {
  printf("\r\033[K");
  clear_screen_below();

  int col = 0;
  int i = 0;

  while (i < pos) {
    if (isspace((unsigned char)buffer[i])) {
      putchar(buffer[i]);
      col++;
      if (buffer[i] == '\n' || buffer[i] == '\r' || col >= LINE_LIMIT) {
        col = 0;
      }
      i++;
      continue;
    }

    int word_len = 0;
    while ((i + word_len) < pos &&
           !isspace((unsigned char)buffer[i + word_len])) {
      word_len++;
    }

    if (col + word_len > LINE_LIMIT) {
      if (col > 0) {
        printf("\n\033[K");
        col = 0;
      }
    }

    for (int w = 0; w < word_len; w++) {
      putchar(buffer[i + w]);
      col++;
      if (col >= LINE_LIMIT && (w + 1) < word_len) {
        printf("\n\033[K");
        col = 0;
      }
    }
    i += word_len;
  }
  fflush(stdout);
}

int main(void) {
  enable_raw_mode();
  memset(buffer, 0, sizeof(buffer));

  while (1) {
    char c;
    int nread = read(STDIN_FILENO, &c, 1);
    if (nread <= 0)
      break;

    if (c == 4) {
      if (pos == 0) {
        printf("\r\nExiting...\r\n");
        break;
      } else {
        printf("\a");
        fflush(stdout);
        continue;
      }
    }

    if (c == erase_char) {
      if (pos > 0) {
        pos--;
        buffer[pos] = '\0';
        redraw();
      } else {
        printf("\a");
        fflush(stdout);
      }
      continue;
    }

    if (c == kill_char) {
      pos = 0;
      buffer[0] = '\0';
      redraw();
      continue;
    }

    if (c == 23) {
      if (pos > 0) {
        while (pos > 0 && isspace((unsigned char)buffer[pos - 1])) {
          pos--;
        }
        while (pos > 0 && !isspace((unsigned char)buffer[pos - 1])) {
          pos--;
        }
        buffer[pos] = '\0';
        redraw();
      } else {
        printf("\a");
        fflush(stdout);
      }
      continue;
    }

    if (c == '\n' || c == '\r') {
      if (pos < MAX_BUFFER - 1) {
        buffer[pos++] = '\n';
        buffer[pos] = '\0';
      }
      printf("\n");
      pos = 0;
      memset(buffer, 0, sizeof(buffer));
      fflush(stdout);
      continue;
    }

    if (isprint((unsigned char)c)) {
      if (pos < MAX_BUFFER - 1) {
        buffer[pos++] = c;
        buffer[pos] = '\0';
        redraw();
      } else {
        printf("\a");
        fflush(stdout);
      }
      continue;
    }

    printf("\a");
    fflush(stdout);
  }

  return EXIT_SUCCESS;
}

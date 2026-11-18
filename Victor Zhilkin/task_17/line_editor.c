#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define COLS 40
#define BUF_SIZE 4096
#define MAX_VIS (BUF_SIZE + 2)
#define CTRL_D 4
#define CTRL_W 23

static struct termios orig_termios;
static int raw_enabled = 0;

static char buffer[BUF_SIZE];
static size_t pos = 0;
static size_t len = 0;

static struct {
  size_t start;
  int len;
} vis[MAX_VIS];

static int nvis = 0;
static int cur_lines = 0;

static void disable_raw_mode(void) {
  if (raw_enabled) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    raw_enabled = 0;
  }
}

static void enable_raw_mode(void) {
  struct termios raw;
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
    perror("tcgetattr");
    exit(EXIT_FAILURE);
  }
  raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    perror("tcsetattr");
    exit(EXIT_FAILURE);
  }
  raw_enabled = 1;
  atexit(disable_raw_mode);
}

static void layout(int *cur_v, int *cur_col) {
  nvis = 1;
  vis[0].start = 0;
  vis[0].len = 0;

  int col = 0;
  size_t i = 0;
  *cur_v = 0;
  *cur_col = 0;

  while (i < len) {
    if (buffer[i] == ' ') {
      if (i == pos) {
        *cur_v = nvis - 1;
        *cur_col = col;
      }
      if (col < COLS) {
        col++;
        vis[nvis - 1].len++;
      } else {
        nvis++;
        vis[nvis - 1].start = i;
        vis[nvis - 1].len = 1;
        col = 1;
        if (i == pos) {
          *cur_v = nvis - 1;
          *cur_col = 0;
        }
      }
      i++;
      continue;
    }

    size_t j = i;
    while (j < len && buffer[j] != ' ')
      j++;
    size_t wlen = j - i;

    if (col + (int)wlen > COLS) {
      nvis++;
      vis[nvis - 1].start = i;
      vis[nvis - 1].len = 0;
      col = 0;
    }

    while (wlen > (size_t)COLS) {
      if (pos >= i && pos < i + COLS) {
        *cur_v = nvis - 1;
        *cur_col = col + (int)(pos - i);
      }
      vis[nvis - 1].len += COLS;
      i += COLS;
      wlen -= COLS;
      nvis++;
      vis[nvis - 1].start = i;
      vis[nvis - 1].len = 0;
      col = 0;
    }

    if (pos >= i && pos < i + wlen) {
      *cur_v = nvis - 1;
      *cur_col = col + (int)(pos - i);
    }

    vis[nvis - 1].len += (int)wlen;
    col += (int)wlen;
    i = j;
  }

  if (pos == len) {
    *cur_v = nvis - 1;
    *cur_col = col;
  }

  if (*cur_col >= COLS) {
    nvis++;
    vis[nvis - 1].start = len;
    vis[nvis - 1].len = 0;
    *cur_v = nvis - 1;
    *cur_col = 0;
  }
}

static void redraw(void) {
  int cur_v, cur_col;
  layout(&cur_v, &cur_col);

  int total = nvis;
  if (cur_lines > total)
    total = cur_lines;

  if (cur_lines > 1)
    printf("\033[%dA", cur_lines - 1);

  for (int v = 0; v < total; v++) {
    if (v == 0)
      printf("\r\033[K");
    else
      printf("\n\r\033[K");

    if (v < nvis)
      printf("%.*s", vis[v].len, buffer + vis[v].start);
  }

  if (cur_lines > cur_v + 1)
    printf("\033[%dB", cur_lines - (cur_v + 1));
  else if (cur_lines < cur_v + 1)
    printf("\033[%dA", (cur_v + 1) - cur_lines);

  printf("\r");
  if (cur_col > 0)
    printf("\033[%dC", cur_col);

  cur_lines = nvis;
  fflush(stdout);
}

static void bell(void) {
  printf("\a");
  fflush(stdout);
}

static void insert_char(char c) {
  if (len < BUF_SIZE - 1) {
    memmove(buffer + pos + 1, buffer + pos, len - pos);
    buffer[pos++] = c;
    len++;
  } else {
    bell();
  }
}

static void delete_char_before(void) {
  if (pos > 0) {
    memmove(buffer + pos - 1, buffer + pos, len - pos);
    pos--;
    len--;
  }
}

static void delete_char_at(void) {
  if (pos < len) {
    memmove(buffer + pos, buffer + pos + 1, len - pos - 1);
    len--;
  }
}

static void delete_word_before(void) {
  size_t i = pos;
  while (i > 0 && buffer[i - 1] == ' ')
    i--;
  while (i > 0 && buffer[i - 1] != ' ')
    i--;
  while (i > 0 && buffer[i - 1] == ' ')
    i--;
  size_t removed = pos - i;
  memmove(buffer + i, buffer + pos, len - pos);
  pos = i;
  len -= removed;
}

static void move_left(void) {
  if (pos > 0)
    pos--;
}

static void move_right(void) {
  if (pos < len)
    pos++;
}

static void handle_escape(void) {
  unsigned char s1, s2;
  if (read(STDIN_FILENO, &s1, 1) != 1)
    return;
  if (s1 != '[')
    return;
  if (read(STDIN_FILENO, &s2, 1) != 1)
    return;

  switch (s2) {
  case 'D':
    move_left();
    break;
  case 'C':
    move_right();
    break;
  case 'A':
    pos = (pos >= COLS) ? pos - COLS : 0;
    break;
  case 'B':
    pos = (pos + COLS <= len) ? pos + COLS : len;
    break;
  case 'H':
    pos = 0;
    break;
  case 'F':
    pos = len;
    break;
  case '3': {
    unsigned char s3;
    if (read(STDIN_FILENO, &s3, 1) == 1 && s3 == '~') {
      delete_char_at();
    }
    break;
  }
  }
}

int main(void) {
  enable_raw_mode();
  cc_t erase = orig_termios.c_cc[VERASE];
  cc_t kill = orig_termios.c_cc[VKILL];

  redraw();

  for (;;) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
      break;

    if (c == CTRL_D) {
      if (len == 0)
        break;
      bell();
      continue;
    }
    if (c == '\n' || c == '\r') {
      pos = 0;
      len = 0;
      cur_lines = 1;
      printf("\n");
      fflush(stdout);
      redraw();
      continue;
    }
    if (c == erase) {
      delete_char_before();
      redraw();
      continue;
    }
    if (c == kill) {
      pos = 0;
      len = 0;
      redraw();
      continue;
    }
    if (c == CTRL_W) {
      delete_word_before();
      redraw();
      continue;
    }
    if (c == 0x1b) {
      handle_escape();
      redraw();
      continue;
    }
    if (isprint(c)) {
      insert_char((char)c);
      redraw();
      continue;
    }

    bell();
  }

  printf("\nExiting...\n");
  disable_raw_mode();
  return EXIT_SUCCESS;
}

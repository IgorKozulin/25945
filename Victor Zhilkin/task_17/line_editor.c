#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define MAX_BUF 4096
#define WRAP_COL 40
#define MAX_ROWS (MAX_BUF + 1)

typedef struct {
    int start;
    int end;
} Row;

static struct termios orig_termios;
static int raw_enabled = 0;
static Row rows[MAX_ROWS];
static int prev_row = 0;
static int fullscreen = 0;
static int view_top = 0;
static int caret_row = -1;

enum Key {
    KEY_OTHER = 0,
    KEY_ESC,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_DELETE
};

static void beep(void) {
    putchar('\a');
    fflush(stdout);
}

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

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }

    atexit(restore_terminal);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGHUP, on_signal);
    signal(SIGQUIT, on_signal);

    raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }
    raw_enabled = 1;
}

static int build_rows(const char *buf, int len) {
    int n = 0;
    int i = 0;
    int rstart = 0;
    int col = 0;

    while (i < len) {
        if (buf[i] == ' ') {
            int s = i;
            while (i < len && buf[i] == ' ')
                i++;
            int spaces = i - s;
            if (col + spaces > WRAP_COL) {
                if (col > 0) {
                    rows[n].start = rstart;
                    rows[n].end = s;
                    n++;
                    rstart = i;
                    col = 0;
                } else {
                    rstart = i;
                }
            } else {
                col += spaces;
            }
            continue;
        }

        int wstart = i;
        while (i < len && buf[i] != ' ')
            i++;
        int wlen = i - wstart;

        if (col > 0 && col + wlen > WRAP_COL) {
            rows[n].start = rstart;
            rows[n].end = wstart;
            n++;
            rstart = wstart;
            col = 0;
        }

        while (wlen > 0) {
            int avail = WRAP_COL - col;
            if (wlen <= avail) {
                col += wlen;
                wlen = 0;
            } else {
                int cut = wstart + avail;
                rows[n].start = rstart;
                rows[n].end = cut;
                n++;
                rstart = cut;
                wstart = cut;
                wlen -= avail;
                col = 0;
            }
        }
    }

    rows[n].start = rstart;
    rows[n].end = len;
    n++;
    return n;
}

static void cursor_rc(int pos, int nrows, int *out_row, int *out_col) {
    int r = 0;

    for (int k = 0; k < nrows; k++) {
        if (rows[k].start <= pos)
            r = k;
        else
            break;
    }

    if (r + 1 < nrows && rows[r].end == pos && rows[r + 1].start == pos)
        r++;

    int col = pos - rows[r].start;
    int width = rows[r].end - rows[r].start;

    if (col < 0)
        col = 0;
    if (col > width)
        col = width;

    *out_row = r;
    *out_col = col;
}

static void derive_caret(int pos, int nrows) {
    int r, c;
    cursor_rc(pos, nrows, &r, &c);
    caret_row = r;
}

static void derive_caret_prev(int pos, int nrows) {
    derive_caret(pos, nrows);
    if (caret_row > 0 && pos == rows[caret_row].start && rows[caret_row - 1].end == pos)
        caret_row--;
}

static void resolve_caret(int pos, int nrows, int *out_row, int *out_col) {
    if (caret_row < 0 || caret_row >= nrows) {
        derive_caret(pos, nrows);
    } else if (pos < rows[caret_row].start || pos > rows[caret_row].end) {
        derive_caret(pos, nrows);
    }

    *out_row = caret_row;
    *out_col = pos - rows[caret_row].start;
}

static int terminal_rows(void) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0)
        return (int)ws.ws_row;

    return 24;
}

static void print_rows(const char *buf, int first, int last) {
    for (int r = first; r < last; r++) {
        if (r > first)
            putchar('\n');
        if (r == 0)
            fputs("> ", stdout);
        fwrite(buf + rows[r].start, 1, (size_t)(rows[r].end - rows[r].start), stdout);
    }
}

static void redraw(const char *buf, int pos) {
    int len = (int)strlen(buf);
    int nrows = build_rows(buf, len);
    int crow, ccol;
    int height = terminal_rows();

    resolve_caret(pos, nrows, &crow, &ccol);

    if (nrows > height) {
        if (!fullscreen) {
            fullscreen = 1;
            view_top = 0;
        }

        if (crow < view_top)
            view_top = crow;
        if (crow >= view_top + height)
            view_top = crow - height + 1;
        if (view_top > nrows - height)
            view_top = nrows - height;
        if (view_top > crow)
            view_top = crow;
        if (view_top < 0)
            view_top = 0;

        int last = view_top + height;
        if (last > nrows)
            last = nrows;

        fputs("\033[2J\033[H", stdout);
        print_rows(buf, view_top, last);

        int sr = crow - view_top + 1;
        int sc = ccol + (crow == 0 ? 2 : 0) + 1;
        printf("\033[%d;%dH", sr, sc);

        fflush(stdout);
        prev_row = crow;
        return;
    }

    if (fullscreen) {
        fullscreen = 0;
        fputs("\033[2J\033[H", stdout);
        print_rows(buf, 0, nrows);

        int sr = crow + 1;
        int sc = ccol + (crow == 0 ? 2 : 0) + 1;
        printf("\033[%d;%dH", sr, sc);

        fflush(stdout);
        prev_row = crow;
        return;
    }

    if (prev_row > 0)
        printf("\033[%dA", prev_row);

    printf("\r\033[J");

    print_rows(buf, 0, nrows);

    int up = (nrows - 1) - crow;
    if (up > 0)
        printf("\033[%dA", up);

    printf("\r");
    int target_col = ccol + (crow == 0 ? 2 : 0);
    if (target_col > 0)
        printf("\033[%dC", target_col);

    fflush(stdout);
    prev_row = crow;
}

static int read_byte_timeout(unsigned char *c) {
    struct pollfd pfd;

    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;

    if (poll(&pfd, 1, 50) <= 0)
        return 0;

    return read(STDIN_FILENO, c, 1) == 1;
}

static int parse_key(void) {
    unsigned char ch;

    if (!read_byte_timeout(&ch))
        return KEY_ESC;

    if (ch != '[' && ch != 'O')
        return KEY_OTHER;

    unsigned char seq[8];
    int n = 0;

    while (n < (int)sizeof(seq) - 1 && read_byte_timeout(&seq[n])) {
        unsigned char cur = seq[n++];
        if ((cur >= 'A' && cur <= 'Z') || (cur >= 'a' && cur <= 'z') || cur == '~')
            break;
    }

    if (n == 0)
        return KEY_OTHER;

    unsigned char fin = seq[n - 1];

    if (ch == 'O') {
        switch (fin) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            case 'H': return KEY_HOME;
            case 'F': return KEY_END;
        }
        return KEY_OTHER;
    }

    switch (fin) {
        case 'A': return KEY_UP;
        case 'B': return KEY_DOWN;
        case 'C': return KEY_RIGHT;
        case 'D': return KEY_LEFT;
        case 'H': return KEY_HOME;
        case 'F': return KEY_END;
        case '~': {
            int num = 0;
            for (int i = 0; i < n - 1 && isdigit(seq[i]); i++)
                num = num * 10 + (seq[i] - '0');
            if (num == 1 || num == 7)
                return KEY_HOME;
            if (num == 4 || num == 8)
                return KEY_END;
            if (num == 3)
                return KEY_DELETE;
            return KEY_OTHER;
        }
    }

    return KEY_OTHER;
}

static void remove_range(char *buf, int *len, int start, int end) {
    if (start < 0)
        start = 0;
    if (end > *len)
        end = *len;
    if (start >= end)
        return;

    memmove(buf + start, buf + end, (size_t)(*len - end + 1));
    *len -= end - start;
}

static int insert_char(char *buf, int *len, int pos, char ch) {
    if (*len >= MAX_BUF - 1)
        return 0;

    memmove(buf + pos + 1, buf + pos, (size_t)(*len - pos + 1));
    buf[pos] = ch;
    (*len)++;
    return 1;
}

static void move_vertical(int pos, int nrows, int dir, int *out_pos) {
    int r, c;
    resolve_caret(pos, nrows, &r, &c);

    int nr = r + dir;
    if (nr < 0 || nr >= nrows)
        return;

    int width = rows[nr].end - rows[nr].start;
    int nc = c < width ? c : width;

    caret_row = nr;
    *out_pos = rows[nr].start + nc;
}

int main(void) {
    char buf[MAX_BUF];
    int len = 0;
    int pos = 0;
    int insert_mode = 0;

    setup_terminal();

    buf[0] = '\0';
    prev_row = 0;
    caret_row = -1;

    fputs("> ", stdout);
    fflush(stdout);

    for (;;) {
        unsigned char c;

        if (read(STDIN_FILENO, &c, 1) != 1)
            break;

        if (c == 27) {
            int key = parse_key();

            if (key == KEY_ESC) {
                if (insert_mode) {
                    insert_mode = 0;
                    pos = len;
                    caret_row = -1;
                    redraw(buf, pos);
                } else {
                    beep();
                }
                continue;
            }

            if (key == KEY_OTHER) {
                beep();
                continue;
            }

            if (!insert_mode) {
                beep();
                continue;
            }

            int nrows = build_rows(buf, len);
            int r, col;
            int newpos = pos;

            resolve_caret(pos, nrows, &r, &col);

            switch (key) {
                case KEY_LEFT:
                    if (pos > rows[r].start) {
                        newpos = pos - 1;
                    } else if (r > 0) {
                        caret_row = r - 1;
                        newpos = rows[caret_row].end;
                    }
                    break;
                case KEY_RIGHT:
                    if (pos < rows[r].end) {
                        newpos = pos + 1;
                    } else if (r + 1 < nrows) {
                        caret_row = r + 1;
                        newpos = rows[caret_row].start;
                    }
                    break;
                case KEY_HOME:
                    newpos = rows[r].start;
                    break;
                case KEY_END:
                    newpos = rows[r].end;
                    break;
                case KEY_UP:
                    move_vertical(pos, nrows, -1, &newpos);
                    break;
                case KEY_DOWN:
                    move_vertical(pos, nrows, 1, &newpos);
                    break;
                case KEY_DELETE:
                    if (pos < len) {
                        remove_range(buf, &len, pos, pos + 1);
                        derive_caret(pos, build_rows(buf, len));
                    }
                    break;
                default:
                    break;
            }

            pos = newpos;
            redraw(buf, pos);
            continue;
        }

        if (c == 16) {
            insert_mode = !insert_mode;
            if (!insert_mode)
                pos = len;
            caret_row = -1;
            redraw(buf, pos);
            continue;
        }

        if (c == 4) {
            if (len == 0)
                break;
            beep();
            continue;
        }

        if (c == '\n' || c == '\r') {
            pos = len;
            redraw(buf, pos);
            putchar('\n');
            fflush(stdout);

            len = 0;
            pos = 0;
            buf[0] = '\0';
            insert_mode = 0;
            prev_row = 0;
            fullscreen = 0;
            view_top = 0;
            caret_row = -1;

            fputs("> ", stdout);
            fflush(stdout);
            continue;
        }

        if (c == orig_termios.c_cc[VERASE] || c == 127 || c == 8) {
            if (pos > 0) {
                remove_range(buf, &len, pos - 1, pos);
                pos--;
                derive_caret_prev(pos, build_rows(buf, len));
                redraw(buf, pos);
            }
            continue;
        }

        if (c == orig_termios.c_cc[VKILL] || c == 21) {
            len = 0;
            pos = 0;
            buf[0] = '\0';
            caret_row = -1;
            redraw(buf, pos);
            continue;
        }

        if (c == 23) {
            int p = pos;
            while (p > 0 && buf[p - 1] == ' ')
                p--;
            while (p > 0 && buf[p - 1] != ' ')
                p--;
            if (p < pos) {
                remove_range(buf, &len, p, pos);
                pos = p;
                caret_row = -1;
                redraw(buf, pos);
            }
            continue;
        }

        if (isprint((int)c)) {
            if (insert_mode && pos < len) {
                buf[pos] = (char)c;
                pos++;
                redraw(buf, pos);
            } else if (insert_char(buf, &len, pos, (char)c)) {
                pos++;
                redraw(buf, pos);
            } else {
                beep();
            }
            continue;
        }

        beep();
    }

    restore_terminal();
    printf("\nExiting...\n");
    return 0;
}

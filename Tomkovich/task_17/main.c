#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>

#define MAX_BUFFER 4096
#define LINE_WIDTH 40

static struct termios original;
static size_t rows = 1;

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
}

int enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &original) == -1)
        return -1;

    struct termios raw = original;

    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        return -1;

    if (atexit(disable_raw_mode) != 0) {
        disable_raw_mode();
        return -1;
    }

    return 0;
}

void beep(void) {
    putchar('\a');
    fflush(stdout);
}

void new_row(size_t *col) {
    printf("\r\n");
    *col = 0;
    rows++;
}

void redraw(const char *buffer, size_t pos) {
    printf("\r");

    if (rows > 1)
        printf("\033[%zuA", rows - 1);

    printf("\033[J> ");

    rows = 1;
    size_t col = 2;
    size_t i = 0;

    while (i < pos) {
        if (buffer[i] == ' ') {
            if (col >= LINE_WIDTH)
                new_row(&col);

            putchar(' ');
            col++;
            i++;
            continue;
        }

        size_t start = i;

        while (i < pos && buffer[i] != ' ')
            i++;

        size_t length = i - start;

        if (length <= LINE_WIDTH &&
            col > 0 &&
            col + length > LINE_WIDTH) {
            new_row(&col);
        }

        for (size_t j = start; j < i; j++) {
            if (col >= LINE_WIDTH)
                new_row(&col);

            putchar(buffer[j]);
            col++;
        }
    }

    fflush(stdout);
}

int main(void) {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "Terminal required\n");
        return 1;
    }

    unsigned char erase;
    unsigned char kill;

    if (tcgetattr(STDIN_FILENO, &original) == -1) {
        perror("tcgetattr");
        return 1;
    }

    erase = original.c_cc[VERASE];
    kill = original.c_cc[VKILL];

    if (enable_raw_mode() == -1) {
        perror("tcsetattr");
        return 1;
    }

    char buffer[MAX_BUFFER];
    size_t pos = 0;
    unsigned char c;

    redraw(buffer, pos);

    while (1) {
        ssize_t n = read(STDIN_FILENO, &c, 1);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            perror("read");
            break;
        }

        if (n == 0)
            break;

        if (c == 4) {
            if (pos == 0) {
                printf("\r\nExiting...\r\n");
                break;
            }

            beep();
        }
        else if (c == erase  c == 127  c == 8) {
            if (pos > 0) {
                pos--;
                redraw(buffer, pos);
            }
            else {
                beep();
            }
        }
        else if (c == kill) {
            pos = 0;
            redraw(buffer, pos);
        }
        else if (c == 23) {
            if (pos == 0) {
                beep();
                continue;
            }

            while (pos > 0 && buffer[pos - 1] == ' ')
                pos--;

            while (pos > 0 && buffer[pos - 1] != ' ')
                pos--;

            while (pos > 0 && buffer[pos - 1] == ' ')
                pos--;

            redraw(buffer, pos);
        }
        else if (c == '\n' || c == '\r') {
            printf("\r\n");
            pos = 0;
            rows = 1;
            redraw(buffer, pos);
        }
        else if (isprint((unsigned char)c)) {
            if (pos < MAX_BUFFER - 1) {
                buffer[pos++] = (char)c;
                redraw(buffer, pos);
            }
            else {
                beep();
            }
        }
        else {
            beep();
        }
    }

    return 0;
}A

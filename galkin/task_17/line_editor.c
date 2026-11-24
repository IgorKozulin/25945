#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <locale.h>
#include <errno.h>

#define WIDTH 40

struct termios old;

void restore(void)
{
    tcsetattr(0, TCSANOW, &old);
}

void erase(int n)
{
    while (n-- > 0)
        printf("\b \b");
}

int main(void)
{
    struct termios term;
    unsigned char line[WIDTH], c;
    int n = 0, k, i;
    ssize_t got;

    setlocale(LC_CTYPE, "");
    if (tcgetattr(0, &old) == -1) {
        perror("tcgetattr");
        return 1;
    }
    if (atexit(restore) != 0)
        return 1;
    term = old;
    term.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    term.c_iflag &= ~(IXON | ICRNL | INLCR | IGNCR);
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSAFLUSH, &term) == -1) {
        perror("tcsetattr");
        return 1;
    }

    for (;;) {
        got = read(0, &c, 1);
        if (got == 0)
            break;
        if (got == -1) {
            if (errno == EINTR)
                continue;
            perror("read");
            return 1;
        }
        if (c == 4) {
            if (n == 0)
                break;
        } else if (c == old.c_cc[VERASE]) {
            if (n > 0) {
                n--;
                erase(1);
            }
        } else if (c == old.c_cc[VKILL]) {
            erase(n);
            n = 0;
        } else if (c == 23) {
            k = n;
            while (n > 0 && isspace(line[n - 1]))
                n--;
            while (n > 0 && !isspace(line[n - 1]))
                n--;
            erase(k - n);
        } else if (c == '\n' || c == '\r') {
            printf("\r\n");
            n = 0;
        } else if (isprint(c)) {
            if (n == WIDTH) {
                k = n;
                while (k > 0 && !isspace(line[k - 1]))
                    k--;
                if (!isspace(c) && k > 0 && k < n) {
                    erase(n - k);
                    printf("\r\n");
                    for (i = k; i < n; i++) {
                        line[i - k] = line[i];
                        putchar(line[i]);
                    }
                    n -= k;
                } else {
                    printf("\r\n");
                    n = 0;
                }
            }
            line[n++] = c;
            putchar(c);
        } else {
            putchar('\a');
        }
        fflush(stdout);
    }
    printf("\r\n");
    return 0;
}

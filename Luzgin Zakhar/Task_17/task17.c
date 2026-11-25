#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAX_INPUT 4096
#define WIDTH 40
static struct termios saved;
static int terminal_ready = 0;
static size_t visible_rows = 1;

static void restore_terminal(void) {
    if (terminal_ready) tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
}

/* Переносим целые слова, если они не помещаются в 40 колонок. */
static size_t draw_wrapped(const char *line, size_t len) {
    size_t pos = 0, col = 0, rows = 1;
    while (pos < len) {
        size_t next = pos, i;
        if (line[pos] == ' ') {
            if (col == WIDTH) { putchar('\n'); col = 0; ++rows; }
            putchar(' '); ++col; ++pos;
            continue;
        }
        while (next < len && line[next] != ' ') ++next;
        if (col > 0 && next - pos <= WIDTH && col + next - pos > WIDTH) {
            putchar('\n'); col = 0; ++rows;
        }
        for (i = pos; i < next; ++i) {
            if (col == WIDTH) { putchar('\n'); col = 0; ++rows; }
            putchar(line[i]); ++col;
        }
        pos = next;
    }
    if (col == WIDTH) { putchar('\n'); ++rows; }
    return rows;
}

/* Перерисовываем только редактируемую строку. */
static void redraw(const char *line, size_t len) {
    printf("\r");
    if (visible_rows > 1) printf("\033[%zuA", visible_rows - 1);
    printf("\033[J");
    visible_rows = draw_wrapped(line, len);
    fflush(stdout);
}

static void erase_word(char *line, size_t *len) {
    while (*len > 0 && line[*len - 1] == ' ') --*len;
    while (*len > 0 && line[*len - 1] != ' ') --*len;
    line[*len] = '\0';
}

int main(void) {
    struct termios raw;
    char line[MAX_INPUT];
    size_t len = 0;
    unsigned char erase_ch, kill_ch;
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fputs("Запускайте программу в обычном терминале.\n", stderr);
        return 1;
    }
    if (tcgetattr(STDIN_FILENO, &saved) < 0) { perror("tcgetattr"); return 1; }
    erase_ch = saved.c_cc[VERASE];
    kill_ch = saved.c_cc[VKILL];
    raw = saved;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG);
    raw.c_iflag &= (tcflag_t)~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr"); return 1;
    }
    terminal_ready = 1;
    atexit(restore_terminal);
    puts("Редактор: Backspace — стереть, Ctrl+U — строка, Ctrl+W — слово, Ctrl+D — выход в начале строки.");
    fflush(stdout);
    for (;;) {
        unsigned char ch;
        ssize_t n = read(STDIN_FILENO, &ch, 1);
        if (n < 0) { perror("read"); return 1; }
        if (n == 0) break;
        if (ch == 4) {
            if (len == 0) { putchar('\n'); break; }
            putchar('\a'); fflush(stdout); continue;
        }
        if (ch == erase_ch || ch == 127 || ch == 8) {
            if (len > 0) --len;
            line[len] = '\0'; redraw(line, len); continue;
        }
        if (ch == kill_ch || ch == 21) {
            len = 0; line[0] = '\0'; redraw(line, len); continue;
        }
        if (ch == 23) {
            erase_word(line, &len); redraw(line, len); continue;
        }
        if (ch == '\r' || ch == '\n') {
            putchar('\n'); fflush(stdout);
            len = 0; line[0] = '\0'; visible_rows = 1;
            continue;
        }
        if (ch < 32 || ch == 127) {
            putchar('\a'); fflush(stdout); continue;
        }
        if (len + 1 >= sizeof(line)) { putchar('\a'); fflush(stdout); continue; }
        line[len++] = (char)ch; line[len] = '\0';
        redraw(line, len);
    }
    return 0;
}

/* Сборка: gcc -std=c99 -Wall -Wextra task17.c -o task17
   Запуск: ./task17  (только в интерактивном терминале)
   Ввод строк: Enter; Ctrl+D в начале строки — завершить. */

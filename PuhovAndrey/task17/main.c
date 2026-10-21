#include <ctype.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAX_LENGTH 40
#define CTRL_D 4
#define CTRL_W 23

static void print_char(char c)
{
    write(STDOUT_FILENO, &c, 1);
}

static void erase_char(void)
{
    write(STDOUT_FILENO, "\b \b", 3);
}

int main(void)
{
    struct termios old_settings;
    struct termios new_settings;
    char line[MAX_LENGTH];
    size_t length = 0;
    unsigned char c;

    tcgetattr(STDIN_FILENO, &old_settings);

    new_settings = old_settings;
    new_settings.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_settings);

    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == CTRL_D && length == 0) {
            break;
        }

        if (c == old_settings.c_cc[VERASE]) {
            if (length > 0) {
                length--;
                erase_char();
            }
            continue;
        }

        if (c == old_settings.c_cc[VKILL]) {
            while (length > 0) {
                length--;
                erase_char();
            }
            continue;
        }

        if (c == CTRL_W) {
            while (length > 0 && line[length - 1] == ' ') {
                length--;
                erase_char();
            }
            while (length > 0 && line[length - 1] != ' ') {
                length--;
                erase_char();
            }
            continue;
        }

        if (c == '\n') {
            print_char('\n');
            length = 0;
            continue;
        }

        if (!isprint(c)) {
            print_char('\a');
            continue;
        }

        if (length == MAX_LENGTH) {
            size_t word_start = length;

            while (word_start > 0 && line[word_start - 1] != ' ') {
                word_start--;
            }

            if (c != ' ' && word_start > 0) {
                size_t word_length = length - word_start;
                size_t i;

                for (i = 0; i < word_length; i++) {
                    erase_char();
                }

                print_char('\n');
                memmove(line, line + word_start, word_length);
                length = word_length;
                write(STDOUT_FILENO, line, length);
            } else {
                print_char('\n');
                length = 0;
            }
        }

        line[length++] = (char)c;
        print_char((char)c);
    }

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_settings);
    return 0;
}

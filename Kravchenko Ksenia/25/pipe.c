/* Лабораторная работа №25: родитель передаёт текст через pipe, ребёнок переводит латиницу и кириллицу в верхний регистр и печатает. */
#include <stdio.h>      /* printf, fgets, stdin */
#include <stdlib.h>     /* exit, EXIT_* */
#include <unistd.h>     /* pipe, fork, read, write, close */
#include <string.h>     /* strlen */
#include <ctype.h>      /* toupper */
#include <sys/wait.h>   /* wait */

#define BUF_SIZE 1024   /* размер буферов для чтения и ввода */

/* Верхний регистр: ASCII через toupper, кириллицу UTF-8 переводим вручную,
   потому что toupper в C-locale знает только ASCII */
static void to_upper(char *buf, ssize_t len) {
    for (ssize_t i = 0; i < len; i++) {            /* идём по байтам строки */
        unsigned char c = (unsigned char)buf[i];   /* текущий байт */

        if (c < 0x80) {                            /* ASCII: обычный toupper */
            buf[i] = (char)toupper(c);
        } else if (c == 0xD0 && i + 1 < len) {     /* кириллица а-п: D0 B0..BF */
            unsigned char c2 = (unsigned char)buf[i + 1];   /* второй байт */
            if (c2 >= 0xB0 && c2 <= 0xBF) {        /* это строчная а-п */
                buf[i + 1] = (char)(c2 - 0x20);    /* -> А-П (D0 90..9F) */
            }
            i++;                                   /* второй байт уже обработан */
        } else if (c == 0xD1 && i + 1 < len) {     /* кириллица р-я и ё */
            unsigned char c2 = (unsigned char)buf[i + 1];   /* второй байт */
            if (c2 >= 0x80 && c2 <= 0x8F) {        /* строчная р-я */
                buf[i] = (char)0xD0;               /* первый байт -> D0 */
                buf[i + 1] = (char)(c2 + 0x20);    /* -> Р-Я (D0 A0..AF) */
            } else if (c2 == 0x91) {               /* строчная ё */
                buf[i] = (char)0xD0;               /* первый байт -> D0 */
                buf[i + 1] = (char)0x81;           /* -> Ё (D0 81) */
            }
            i++;                                   /* второй байт уже обработан */
        }
    }
}

int main() {                            /* точка входа */
    int pipefd[2];                      /* дескрипторы канала: [0] чтение, [1] запись */
    pid_t pid;                          /* pid дочернего процесса */

    if (pipe(pipefd) == -1) {           /* создаём канал */
        perror("pipe");                 /* печатаем причину ошибки */
        exit(EXIT_FAILURE);             /* завершаемся с ошибкой */
    }

    pid = fork();                       /* создаём дочерний процесс */
    if (pid == -1) {                    /* fork не удался? */
        perror("fork");                 /* сообщаем об ошибке */
        exit(EXIT_FAILURE);             /* выходим */
    }

    if (pid == 0) {                     /* pid == 0 — это потомок */
        close(pipefd[1]);               /* потомку не нужен конец записи */
        char buffer[BUF_SIZE];          /* буфер для принятых данных */
        ssize_t n;                      /* сколько байт прочитали */
        printf("[CHILD] Процесс запущен, жду данные из канала...\n");   /* лог потомка */
        fflush(stdout);                 /* сразу выводим лог */

        while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) { /* читаем до EOF */
            buffer[n] = '\0';           /* завершаем строку для printf */
            ssize_t j = 0;              /* длина строки после чистки */
            for (ssize_t i = 0; i < n; i++) {   /* убираем спецсимволы */
                unsigned char c = (unsigned char)buffer[i];   /* текущий байт */

                if (c == 0x1B) {        /* ESC: выкидываем всю escape-последовательность */
                    while (j > 0 && buffer[j - 1] == ' ') j--;          /* пробел перед */

                    if (i + 1 < n && buffer[i + 1] == '[') {            /* CSI: ESC [ ... финальный байт */
                        i += 1;                                         /* встаём на '[' */
                        while (i + 1 < n && (buffer[i + 1] < '@' || buffer[i + 1] > '~')) i++;
                        if (i + 1 < n) i += 1;                          /* перепрыгиваем финальный байт */
                    } else if (i + 1 < n && buffer[i + 1] == 'O') {     /* SS3: ESC O x */
                        i += 2;                                         /* пропускаем оба байта */
                    } else {
                        /* одиночный ESC: сам байт пропустит цикл */
                    }

                    while (i + 1 < n && buffer[i + 1] == ' ') i++;      /* пробелы после */

                } else if ((c >= 0x20 && c != 0x7F) || c == '\n' || c == '\t') {  /* обычный символ? */
                    buffer[j++] = buffer[i];    /* оставляем его */
                } else {
                    /* прочий спецсимвол: убираем его вместе с пробелами рядом */
                    while (j > 0 && buffer[j - 1] == ' ') j--;          /* пробел перед ним */
                    while (i + 1 < n && buffer[i + 1] == ' ') i++;      /* пробелы после него */
                }
            }
            buffer[j] = '\0';           /* новая граница строки */
            to_upper(buffer, j);        /* переводим весь текст в верхний регистр */
            printf("[CHILD] Принято и переведено в верхний регистр: %s", buffer);  /* печатаем результат */
            fflush(stdout);             /* сбрасываем буфер вывода */
        }

        close(pipefd[0]);               /* данные закончились — закрываем чтение */
        exit(EXIT_SUCCESS);             /* exit, а не return: не выполнять код родителя */
    } else {                            /* иначе — ветка родителя */
        close(pipefd[0]);               /* родителю не нужен конец чтения */
        printf("[PARENT] Канал создан, связь установлена.\n");   /* лог родителя */
        printf("[PARENT] Вводите строки; пустая строка или Ctrl+D - конец ввода:\n");  /* подсказка */

        char input[BUF_SIZE];           /* буфер для введённой строки */
        while (1) {                     /* бесконечный цикл ввода */
            printf("> ");               /* приглашение ко вводу */
            fflush(stdout);             /* показываем приглашение сразу */

            if (fgets(input, sizeof(input), stdin) == NULL) {   /* читаем строку, NULL = EOF/Ctrl+D */
                break;                  /* конец ввода */
            }

            if (input[0] == '\n' || input[0] == '\0') {   /* пустая строка? */
                printf("[PARENT] Пустая строка получена, ввод завершён.\n");  /* лог */
                break;                  /* выходим из цикла */
            }

            /* отправляем строку как есть: кириллица в UTF-8 — это байты >= 128,
               и фильтр isprint() их выбрасывал */
            write(pipefd[1], input, strlen(input));   /* пишем строку в канал */
        }

        close(pipefd[1]);               /* закрываем запись — потомок получит EOF */
        wait(NULL);                     /* ждём потомка, чтобы не остался зомби */
        printf("[PARENT] Дочерний процесс завершился, родитель выходит.\n");  /* финальный лог */
    }

    return 0;                           /* успешное завершение программы */
}

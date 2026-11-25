/* Лабораторная работа №25: родитель передаёт текст через pipe, ребёнок переводит его в верхний регистр и печатает. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

#define BUF_SIZE 1024   // размер буферов для чтения и ввода

int main() {
    int pipefd[2];   // два конца канала
    pid_t pid;

    /* pipefd[0] — читать, pipefd[1] — писать */
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    /* После fork оба процесса видят оба конца канала */
    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    /* ---------- Ветка ребёнка: читает из канала ---------- */
    if (pid == 0) {
        /* Закрываем пишущий конец: иначе read не увидит EOF */
        close(pipefd[1]);
        char buffer[BUF_SIZE];
        ssize_t n;
        printf("[CHILD] Запущен. Ожидаю данные от родителя...\n");
        fflush(stdout);

        /* read возвращает 0, когда родитель закрыл свой конец канала */
        while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[n] = '\0';   // завершаем строку для printf
            /* toupper только для печатаемых, служебные байты не трогаем */
            for (ssize_t i = 0; i < n; i++) {
                unsigned char ch = (unsigned char)buffer[i];
                if (isprint(ch) || ch == '\n' || ch == '\t' || ch == ' ') {
                    buffer[i] = toupper(ch);
                }
            }
            printf("[CHILD] Получено и преобразовано: %s", buffer);
            fflush(stdout);
        }

        close(pipefd[0]);   // данные закончились — закрываем свой конец
        exit(EXIT_SUCCESS);   // exit, а не return: не выполнять код родителя
    } else {
        /* ---------- Ветка родителя: пишет в канал ---------- */
        /* Родителю читающий конец не нужен */
        close(pipefd[0]);
        printf("[PARENT] Связь через канал установлена!\n");
        printf("[PARENT] Введите текст (пустая строка или Ctrl+D — выход):\n");

        char input[BUF_SIZE];
        while (1) {
            printf("> ");   // приглашение ко вводу
            fflush(stdout);

            /* NULL — Ctrl+D или конец потока ввода */
            if (fgets(input, sizeof(input), stdin) == NULL) {
                break;
            }

            /* Пустая строка тоже завершает ввод */
            if (input[0] == '\n' || input[0] == '\0') {
                printf("[PARENT] Пустая строка — завершаю ввод.\n");
                break;
            }

            /* Отфильтровываем непечатаемые символы перед отправкой */
            char filtered[BUF_SIZE];
            size_t j = 0;
            for (size_t i = 0; input[i] != '\0'; i++) {
                unsigned char ch = (unsigned char)input[i];
                if (isprint(ch) || ch == '\n' || ch == '\t' || ch == ' ') {
                    filtered[j++] = ch;
                }
            }
            filtered[j] = '\0';

            /* Пишем в канал только если после фильтра что-то осталось */
            if (j > 0) {
                write(pipefd[1], filtered, j);
            }
        }

        close(pipefd[1]);   // закрытие даёт ребёнку EOF
        wait(NULL);         // ждём ребёнка, чтобы не остался зомби
        printf("[PARENT] Потомок завершился. Родитель тоже завершает работу.\n");
    }

    return 0;
}

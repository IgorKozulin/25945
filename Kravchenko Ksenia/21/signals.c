/* Лабораторная работа №21: «пищалка» — по SIGINT издаёт звук и считает сигналы, по SIGQUIT печатает счётчик и завершается. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define MAX_MORSE 1024   // максимальная длина шаблона морзе

/* Флаги меняются в обработчиках сигналов, поэтому volatile sig_atomic_t:
   volatile - компилятор не закеширует значение в регистре,
   sig_atomic_t - запись/чтение не разорвётся сигналом */
static volatile sig_atomic_t int_count = 0;     // сколько раз пришёл SIGINT
static volatile sig_atomic_t quit_requested = 0; // пора завершаться
static volatile sig_atomic_t play_morse = 0;    // нужно проиграть шаблон

static char morse_pattern[MAX_MORSE];   // шаблон из файла или "-.-"
static size_t morse_len = 0;

/* Обработчик SIGINT: считаем сигнал и просим main издать звук.
   В обработчике можно только простые действия — никаких printf и malloc. */
static void on_sigint(int sig) {
    (void)sig;   // аргумент не используется
    if (int_count < 2147483647) int_count++;   // защита от переполнения
    play_morse = 1;   // сам звук издаёт main, обработчик только ставит флаг
}

/* Обработчик SIGQUIT: просим main выйти из цикла */
static void on_sigquit(int sig) {
    (void)sig;
    quit_requested = 1;   // main увидит флаг и выйдет из цикла
}

/* Читает шаблон из файла целиком; -1 — если файл не открылся */
static int load_morse(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    /* читаем не больше MAX_MORSE-1 байт, чтобы осталось место под '\0' */
    morse_len = fread(morse_pattern, 1, MAX_MORSE - 1, f);
    morse_pattern[morse_len] = '\0';   // завершаем строку
    fclose(f);
    return 0;
}

int main(int argc, char *argv[]) {
    struct sigaction sa_int, sa_quit;

    /* Шаблон можно задать файлом — первым аргументом командной строки */
    if (argc > 1) {
        if (load_morse(argv[1]) == -1) {
            perror("load_morse");
            return EXIT_FAILURE;
        }
        printf("[DEBUG] Загружено %zu символов из %s\n", morse_len, argv[1]);
    } else {
        /* По умолчанию — буква K (-.-) */
        strcpy(morse_pattern, "-.-");
        morse_len = 3;
        printf("[DEBUG] Файл не указан, использую стандартный '-.-'\n");
    }

    /* sigaction надёжнее signal: обработчик не сбрасывается после сигнала */
    sa_int.sa_handler = on_sigint;
    sigemptyset(&sa_int.sa_mask);   // другие сигналы не блокируем
    sa_int.sa_flags = 0;            // поведение по умолчанию

    sa_quit.sa_handler = on_sigquit;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;

    /* Регистрируем обработчики: SIGINT — Ctrl+C, SIGQUIT — Ctrl+\ */
    if (sigaction(SIGINT, &sa_int, NULL) == -1 ||
        sigaction(SIGQUIT, &sa_quit, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    printf("Press Ctrl+C to play Morse; Ctrl+\\ to exit.\n");
    fflush(stdout);   // иначе приглашение может остаться в буфере

    /* Основной цикл: спим в pause(), пока не придёт сигнал */
    while (!quit_requested) {
        /* Флаг выставил обработчик — проигрываем шаблон */
        if (play_morse) {
            play_morse = 0;
            char bell = '\a';
            struct timespec ts_beep = {0, 200000 * 1000};   // 200 мс
            struct timespec ts_pause = {1, 0};              // 1 с

            /* '-' — бип, '.' — пауза; nanosleep не жжёт процессор */
            for (size_t i = 0; i < morse_len; i++) {
                if (morse_pattern[i] == '-') {
                    /* BEL пишем напрямую в stdout, без буфера stdio */
                    write(STDOUT_FILENO, &bell, 1);
                    nanosleep(&ts_beep, NULL);
                } else if (morse_pattern[i] == '.') {
                    nanosleep(&ts_pause, NULL);
                }
            }
        }
        pause();   // спим до следующего сигнала
    }

    /* Сюда попадаем после SIGQUIT: печатаем, сколько было SIGINT */
    printf("\nSIGINT received %d time(s). Exiting.\n", (int)int_count);
    return EXIT_SUCCESS;
}

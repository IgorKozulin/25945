/* Лабораторная работа №21: «пищалка» — по SIGINT издаёт звук не дольше -t секунд и не больше -c раз, по SIGQUIT печатает счётчик и завершается. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define DEFAULT_BEEP_SECONDS 1.0   // длительность писка по умолчанию
#define BELL_INTERVAL_MS 200       // интервал между звуковыми сигналами

/* Флаги меняются в обработчиках сигналов, поэтому volatile sig_atomic_t:
   volatile - компилятор не закеширует значение в регистре,
   sig_atomic_t - запись/чтение не разорвётся сигналом */
static volatile sig_atomic_t int_count = 0;      // сколько раз пришёл SIGINT
static volatile sig_atomic_t quit_requested = 0; // пора завершаться
static volatile sig_atomic_t beep_request = 0;   // нужно издать звук

/* Обработчик SIGINT: считаем сигнал и просим main пискнуть.
   В обработчике можно только простые действия - никаких printf и malloc. */
static void handle_sigint(int sig) {
    (void)sig;   // аргумент не используется
    if (int_count < 2147483647) int_count++;   // защита от переполнения
    beep_request = 1;   // писк издаёт main, обработчик только ставит флаг
}

/* Обработчик SIGQUIT: просим main выйти из цикла */
static void handle_sigquit(int sig) {
    (void)sig;
    quit_requested = 1;   // main увидит флаг и выйдет из цикла
}

/* Подсказка по запуску: -t длительность писка, -c сколько писков разрешено */
static void usage(const char *prog) {
    fprintf(stderr, "Использование: %s [-t секунды] [-c количество]\n", prog);
    fprintf(stderr, "  -t  длительность одного писка в секундах (по умолчанию %.1f)\n",
            DEFAULT_BEEP_SECONDS);
    fprintf(stderr, "  -c  сколько всего писков разрешено, 0 - без ограничения\n");
}

/* Пищим с интервалом BELL_INTERVAL_MS, пока не выйдет время
   или не придёт SIGQUIT (тогда выходим из писка досрочно) */
static void beep_for(double seconds) {
    struct timespec interval = {0, BELL_INTERVAL_MS * 1000000L};
    /* 1 писк = BELL_INTERVAL_MS мс, значит на seconds секунд нужно столько сигналов */
    long bells = (long)(seconds * 1000.0 / BELL_INTERVAL_MS + 0.5);
    const char bell = '\a';

    if (bells < 1) bells = 1;   // даже очень короткий писк - хотя бы один BEL

    for (long i = 0; i < bells && !quit_requested; i++) {
        /* BEL пишем напрямую в stdout, без буфера stdio */
        write(STDOUT_FILENO, &bell, 1);
        nanosleep(&interval, NULL);   // не жжёт процессор
    }
}

int main(int argc, char *argv[]) {
    double beep_seconds = DEFAULT_BEEP_SECONDS;
    long beep_limit = 0;   // 0 - без ограничения
    int opt;

    /* Разбираем опции: -t <секунды>, -c <количество> */
    while ((opt = getopt(argc, argv, "t:c:h")) != -1) {
        char *end = NULL;

        switch (opt) {
        case 't':
            errno = 0;
            beep_seconds = strtod(optarg, &end);
            if (errno != 0 || end == optarg || *end != '\0' || beep_seconds <= 0) {
                fprintf(stderr, "-t: нужно положительное число секунд\n");
                return EXIT_FAILURE;
            }
            break;

        case 'c':
            errno = 0;
            beep_limit = strtol(optarg, &end, 10);
            if (errno != 0 || end == optarg || *end != '\0' || beep_limit < 0) {
                fprintf(stderr, "-c: нужно целое неотрицательное число\n");
                return EXIT_FAILURE;
            }
            break;

        case 'h':
            usage(argv[0]);
            return EXIT_SUCCESS;

        default:
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* Лишние позиционные аргументы не поддерживаются: всё задаётся опциями */
    if (optind < argc) {
        fprintf(stderr, "Лишние аргументы не поддерживаются\n");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* sigaction надёжнее signal: обработчик не сбрасывается после сигнала */
    struct sigaction sa_int, sa_quit;

    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);   // другие сигналы не блокируем
    sa_int.sa_flags = 0;            // поведение по умолчанию

    sa_quit.sa_handler = handle_sigquit;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;

    /* Регистрируем обработчики: SIGINT - Ctrl+C, SIGQUIT - Ctrl+\ */
    if (sigaction(SIGINT, &sa_int, NULL) == -1 ||
        sigaction(SIGQUIT, &sa_quit, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    /* Рассказываем, с какими настройками работаем */
    printf("Пищалка: 1 писк = %.2f с", beep_seconds);
    if (beep_limit > 0) {
        printf(", всего не более %ld писков", beep_limit);
    }
    printf(".\nCtrl+C - писк, Ctrl+\\ - выход.\n");
    fflush(stdout);   // иначе приглашение может остаться в буфере

    for (;;) {
        if (quit_requested) break;

        /* Флаг выставил обработчик - пищим */
        if (beep_request) {
            beep_request = 0;
            beep_for(beep_seconds);

            /* Лимит писков исчерпан - завершаемся */
            if (beep_limit > 0 && int_count >= beep_limit) break;
            continue;
        }

        /* Блокируем сигналы, чтобы не потерять тот, что придёт между
           проверкой флагов и sigsuspend (иначе можно заснуть навсегда) */
        sigset_t block, empty;
        sigemptyset(&block);
        sigaddset(&block, SIGINT);
        sigaddset(&block, SIGQUIT);
        sigemptyset(&empty);

        sigprocmask(SIG_BLOCK, &block, NULL);
        if (!beep_request && !quit_requested) {
            sigsuspend(&empty);   // атомарно ждём сигнал
        }
        sigprocmask(SIG_UNBLOCK, &block, NULL);
    }

    /* Сюда попадаем после SIGQUIT или после исчерпания лимита писков */
    if (beep_limit > 0 && int_count >= beep_limit) {
        printf("\nЛимит писков исчерпан (%ld).", beep_limit);
    }
    printf("\nСигналов SIGINT получено: %d. Выход.\n", (int)int_count);
    return EXIT_SUCCESS;
}

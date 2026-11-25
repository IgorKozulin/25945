#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t beeps = 0;
static volatile sig_atomic_t finished = 0;

static void handler(int signo) {
    if (signo == SIGINT) {
        ++beeps;
        (void)write(STDOUT_FILENO, "\a", 1);
    } else if (signo == SIGQUIT) {
        finished = 1;
    }
}

int main(void) {
    struct sigaction sa;
    sigset_t blocked, oldmask, waitmask;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = handler;
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGQUIT, &sa, NULL) < 0) {
        perror("sigaction"); return 1;
    }
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGQUIT);
    if (sigprocmask(SIG_BLOCK, &blocked, &oldmask) < 0) {
        perror("sigprocmask"); return 1;
    }
    waitmask = oldmask;
    sigdelset(&waitmask, SIGINT);
    sigdelset(&waitmask, SIGQUIT);
    puts("Нажимайте Ctrl+C для сигнала; Ctrl+\\ — завершение.");
    fflush(stdout);
    while (!finished) sigsuspend(&waitmask);
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    printf("\nСигнал прозвучал %d раз(а)\n", (int)beeps);
    return 0;
}

/* Сборка: gcc -std=c99 -Wall -Wextra task21.c -o task21
   Запуск в терминале: ./task21
   Ctrl+C — звуковой сигнал, Ctrl+\\ — завершение и счётчик. */

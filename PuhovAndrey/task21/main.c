#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t signal_count = 0;
static volatile sig_atomic_t finished = 0;

static void handle_sigint(int signal_number)
{
    (void)signal_number;
    signal_count++;
    write(STDOUT_FILENO, "\a", 1);
}

static void handle_sigquit(int signal_number)
{
    (void)signal_number;
    finished = 1;
}

int main(void)
{
    struct sigaction action = {0};

    sigemptyset(&action.sa_mask);
    action.sa_handler = handle_sigint;
    sigaction(SIGINT, &action, NULL);

    action.sa_handler = handle_sigquit;
    sigaction(SIGQUIT, &action, NULL);

    while (!finished) {
        pause();
    }

    printf("\nЗвуковой сигнал прозвучал %d раз(а).\n", (int)signal_count);
    return 0;
}

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

volatile sig_atomic_t count, done;

void on_int(int sig)
{
    (void)sig;
    if (write(1, "\a", 1) == 1)
        count++;
}

void on_quit(int sig)
{
    (void)sig;
    done = 1;
}

int main(void)
{
    struct sigaction sa;
    sigset_t set, oldmask;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);
    if (sigprocmask(SIG_BLOCK, &set, &oldmask) == -1) {
        perror("sigprocmask");
        return 1;
    }
    sa.sa_mask = set;
    sa.sa_flags = 0;
    sa.sa_handler = on_int;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
    sa.sa_handler = on_quit;
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
    sigdelset(&oldmask, SIGINT);
    sigdelset(&oldmask, SIGQUIT);
    while (!done)
        sigsuspend(&oldmask);
    printf("Beeps: %d\n", (int)count);
    return 0;
}

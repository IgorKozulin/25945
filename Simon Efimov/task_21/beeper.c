#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

/* Global state: beep counter and on/off flag */
static volatile sig_atomic_t beep_count = 0;
static volatile sig_atomic_t beep_enabled = 1;  /* 1 = on, 0 = off */

/* SIGINT handler: beep if enabled, always count the signal */
void sigint_handler(int sig) {
    (void)sig;
    if (beep_enabled) {
        write(STDOUT_FILENO, "\a", 1);
    }
    beep_count++;
}

/* SIGTSTP (Ctrl+Z) handler: toggle beep on/off */
void sigtstp_handler(int sig) {
    (void)sig;
    beep_enabled = !beep_enabled;
    if (beep_enabled) {
        write(STDOUT_FILENO, "\n[Beeper ON]\n", 13);
    } else {
        write(STDOUT_FILENO, "\n[Beeper OFF]\n", 14);
    }
}

/* SIGQUIT handler: report and exit */
void sigquit_handler(int sig) {
    (void)sig;
    char buffer[128];
    int len = snprintf(buffer, sizeof(buffer),
                       "\nTotal beeps: %d\nProgram terminated by SIGQUIT\n",
                       (int)beep_count);
    write(STDOUT_FILENO, buffer, len);
    _exit(EXIT_SUCCESS);
}

int main(void) {
    struct sigaction sa_int, sa_tstp, sa_quit;

    /* SIGINT (Ctrl+C) */
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction SIGINT");
        return 1;
    }

    /* SIGTSTP (Ctrl+Z) — toggle beeper */
    memset(&sa_tstp, 0, sizeof(sa_tstp));
    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    if (sigaction(SIGTSTP, &sa_tstp, NULL) == -1) {
        perror("sigaction SIGTSTP");
        return 1;
    }

    /* SIGQUIT (Ctrl+\) */
    memset(&sa_quit, 0, sizeof(sa_quit));
    sa_quit.sa_handler = sigquit_handler;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;
    if (sigaction(SIGQUIT, &sa_quit, NULL) == -1) {
        perror("sigaction SIGQUIT");
        return 1;
    }

    printf("Beeper started!\n");
    printf("Press Ctrl+C  - beep (if enabled)\n");
    printf("Press Ctrl+Z  - toggle beeper ON/OFF\n");
    printf("Press Ctrl+\\ - exit with count\n");
    printf("Waiting for signals...\n");
    fflush(stdout);

    while (1) {
        pause();
    }

    return 0;
}

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int main(void) {
    if (setenv("TZ", "America/Los_Angeles", 1) < 0) { perror("setenv"); return 1; }
    tzset();
    time_t now = time(NULL);
    if (now == (time_t)-1) { perror("time"); return 1; }
    struct tm *local = localtime(&now);
    if (!local) { perror("localtime"); return 1; }
    char text[128];
    if (!strftime(text, sizeof text, "%m/%d/%Y %H:%M:%S %Z", local)) return 1;
    puts(text); return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    time_t now = time(NULL);
    struct tm california;
    char text[64];

    if (now == (time_t)-1) {
        perror("time");
        return EXIT_FAILURE;
    }
    if (setenv("TZ", "PST8PDT", 1) == -1) {
        perror("setenv");
        return EXIT_FAILURE;
    }

    tzset();
    if (localtime_r(&now, &california) == NULL ||
        strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S %Z", &california) == 0) {
        fprintf(stderr, "Cannot format time\n");
        return EXIT_FAILURE;
    }

    puts(text);
    return EXIT_SUCCESS;
}

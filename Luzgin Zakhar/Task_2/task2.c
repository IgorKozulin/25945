#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* В Калифорнии действует PST (UTC-8) зимой и PDT (UTC-7) летом. */
/* POSIX TZ: правила перехода на летнее время в США с 2007 года. */
#define CALIFORNIA_TZ "PST8PDT,M3.2.0/2,M11.1.0/2"

static int print_time(const char *label, const struct tm *broken_down)
{
    char buffer[128];
    if (strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %Z (%z)",
                 broken_down) == 0)
        return -1;
    printf("%s: %s\n", label, buffer);
    return 0;
}

int main(void)
{
    time_t current = time(NULL);
    struct tm utc_time;
    struct tm california_time;

    if (current == (time_t)-1) {
        perror("time");
        return EXIT_FAILURE;
    }
    if (gmtime_r(&current, &utc_time) == NULL) {
        fputs("Не удалось получить время UTC\n", stderr);
        return EXIT_FAILURE;
    }
    if (setenv("TZ", CALIFORNIA_TZ, 1) == -1) {
        perror("setenv(TZ)");
        return EXIT_FAILURE;
    }
    tzset();
    if (localtime_r(&current, &california_time) == NULL) {
        fputs("Не удалось получить время в Калифорнии\n", stderr);
        return EXIT_FAILURE;
    }
    if (print_time("UTC", &utc_time) == -1 ||
        print_time("Калифорния", &california_time) == -1) {
        fputs("Ошибка форматирования даты\n", stderr);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

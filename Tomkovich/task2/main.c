#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

extern char *tzname[];

int main(void) {
    time_t stamp;
    struct tm *parts;

    setenv("TZ", "PST8PDT", 1);
    tzset();

    stamp = time(NULL);

    fputs(ctime(&stamp), stdout);

    parts = localtime(&stamp);
    if (!parts) return 1;

    printf("%02d.%02d.%04d %02d:%02d:%02d %s\n",
           parts->tm_mday,
           parts->tm_mon + 1,
           parts->tm_year + 1900,
           parts->tm_hour,
           parts->tm_min,
           parts->tm_sec,
           tzname[parts->tm_isdst]);

    return 0;
}

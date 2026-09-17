#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int main(void)
{
    time_t now;
    struct tm *sp;

    setenv("TZ", "PST8", 1);
    tzset();

    time(&now);
    sp = localtime(&now);

    printf("%d/%d/%02d %d:%02d PST\n",
           sp->tm_mon + 1,
           sp->tm_mday,
           sp->tm_year,
           sp->tm_hour,
           sp->tm_min);

    return 0;
}

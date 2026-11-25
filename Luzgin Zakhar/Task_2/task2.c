#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#define _POSIX_C_SOURCE 200809L
int main(void){
    setenv("TZ", "America/Los_Angeles",1);
    tzset();
    time_t current_time;
    struct tm *sp;
    time(&current_time);
    sp = localtime(&current_time);
    printf("%02d/%02d/%04d %02d:%02d %s\n", 
            sp -> tm_mday,
            sp -> tm_mon + 1,
            sp -> tm_year + 1900,
            sp -> tm_hour,
            sp -> tm_min,
            tzname[sp -> tm_isdst]);
    return 0;
}

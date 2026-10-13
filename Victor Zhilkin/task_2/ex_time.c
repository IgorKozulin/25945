#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int main() {
    int offsets[] = {0, 1, 3, -5, 9};
    char tz_buf[16];
    
    for (int i = 0; i < 5; i++) {
        snprintf(tz_buf, sizeof(tz_buf), "GMT%+d", -offsets[i]);
        setenv("TZ", tz_buf, 1);
        tzset();
        
        time_t now;
        time(&now);
        
        struct tm *sp = localtime(&now);
        
        printf("%02d/%02d/%04d %02d:%02d\n", 
               sp->tm_mday, 
               sp->tm_mon + 1, 
               sp->tm_year + 1900, 
               sp->tm_hour, 
               sp->tm_min);
    }    return 0;
}


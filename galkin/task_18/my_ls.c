#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <libgen.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int i, j, result = 0;
    mode_t bits[] = {
        S_IRUSR, S_IWUSR, S_IXUSR,
        S_IRGRP, S_IWGRP, S_IXGRP,
        S_IROTH, S_IWOTH, S_IXOTH
    };

    if (argc < 2) {
        fprintf(stderr, "Usage: %s path [path...]\n", argv[0]);
        return 1;
    }
    for (i = 1; i < argc; i++) {
        struct stat st;
        struct passwd *user;
        struct group *group;
        struct tm *tm;
        char rights[] = "----------";
        char size[32] = "", uid[32], gid[32], date[64];
        double age;

        if (lstat(argv[i], &st) == -1) {
            perror(argv[i]);
            result = 1;
            continue;
        }
        rights[0] = S_ISDIR(st.st_mode) ? 'd' : S_ISREG(st.st_mode) ? '-' : '?';
        for (j = 0; j < 9; j++)
            if (st.st_mode & bits[j])
                rights[j + 1] = "rwxrwxrwx"[j];

        user = getpwuid(st.st_uid);
        group = getgrgid(st.st_gid);
        snprintf(uid, sizeof(uid), "%lu", (unsigned long)st.st_uid);
        snprintf(gid, sizeof(gid), "%lu", (unsigned long)st.st_gid);
        if (S_ISREG(st.st_mode))
            snprintf(size, sizeof(size), "%lld", (long long)st.st_size);

        tm = localtime(&st.st_mtime);
        age = difftime(time(NULL), st.st_mtime);
        if (tm == NULL || strftime(date, sizeof(date),
            age < 0 || age > 183.0 * 24 * 60 * 60
                ? "%b %e  %Y" : "%b %e %H:%M", tm) == 0)
            strcpy(date, "?");

        printf("%s %4lu %-16s %-16s %12s %-12s %s\n",
               rights, (unsigned long)st.st_nlink,
               user ? user->pw_name : uid,
               group ? group->gr_name : gid,
               size, date, basename(argv[i]));
    }
    return result;
}

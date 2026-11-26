#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <libgen.h>

void getPermissions(mode_t mode, char permissions[])
{
    permissions[0] = S_ISDIR(mode) ? 'd' : S_ISREG(mode) ? '-' : '?';

    permissions[1] = (mode & S_IRUSR) ? 'r' : '-';
    permissions[2] = (mode & S_IWUSR) ? 'w' : '-';
    permissions[3] = (mode & S_IXUSR) ? 'x' : '-';

    permissions[4] = (mode & S_IRGRP) ? 'r' : '-';
    permissions[5] = (mode & S_IWGRP) ? 'w' : '-';
    permissions[6] = (mode & S_IXGRP) ? 'x' : '-';

    permissions[7] = (mode & S_IROTH) ? 'r' : '-';
    permissions[8] = (mode & S_IWOTH) ? 'w' : '-';
    permissions[9] = (mode & S_IXOTH) ? 'x' : '-';

    permissions[10] = '\0';
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s file [files...]\n", argv[0]);
        return 1;
    }

    time_t currentTime = time(NULL);
    time_t sixMonths = 180 * 24 * 60 * 60;

    for (int i = 1; i < argc; i++) {
        struct stat fileInfo;

        if (lstat(argv[i], &fileInfo) == -1) {
            perror(argv[i]);
            continue;
        }

        char permissions[11];
        getPermissions(fileInfo.st_mode, permissions);

        struct passwd *user = getpwuid(fileInfo.st_uid);
        struct group *group = getgrgid(fileInfo.st_gid);

        const char *userName = user != NULL ? user->pw_name : "?";
        const char *groupName = group != NULL ? group->gr_name : "?";

        char fileSize[32] = "";

        if (S_ISREG(fileInfo.st_mode)) {
            snprintf(fileSize, sizeof(fileSize), "%lld",
                     (long long)fileInfo.st_size);
        }

        char date[32];
        struct tm *fileTime = localtime(&fileInfo.st_mtime);

        if (fileTime == NULL) {
            strcpy(date, "?");
        }
        else if (currentTime - fileInfo.st_mtime > sixMonths ||
                 fileInfo.st_mtime > currentTime) {
            strftime(date, sizeof(date), "%b %e  %Y", fileTime);
        }
        else {
            strftime(date, sizeof(date), "%b %e %H:%M", fileTime);
        }

        char filePath[4096];
        snprintf(filePath, sizeof(filePath), "%s", argv[i]);

        char *fileName = basename(filePath);

        printf("%-10s %4lu %-8s %-8s %10s %-12s %s\n",
               permissions,
               (unsigned long)fileInfo.st_nlink,
               userName,
               groupName,
               fileSize,
               date,
               fileName);
    }

    return 0;
}

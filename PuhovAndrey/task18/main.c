#define _POSIX_C_SOURCE 200809L

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static void make_mode_string(mode_t mode, char text[11])
{
    text[0] = S_ISDIR(mode) ? 'd' : (S_ISREG(mode) ? '-' : '?');
    text[1] = mode & S_IRUSR ? 'r' : '-';
    text[2] = mode & S_IWUSR ? 'w' : '-';
    text[3] = mode & S_IXUSR ? 'x' : '-';
    text[4] = mode & S_IRGRP ? 'r' : '-';
    text[5] = mode & S_IWGRP ? 'w' : '-';
    text[6] = mode & S_IXGRP ? 'x' : '-';
    text[7] = mode & S_IROTH ? 'r' : '-';
    text[8] = mode & S_IWOTH ? 'w' : '-';
    text[9] = mode & S_IXOTH ? 'x' : '-';
    text[10] = '\0';
}

static void print_file(const char *path)
{
    struct stat info;
    struct passwd *owner;
    struct group *group;
    struct tm modification_time;
    char mode[11];
    char size[20] = "";
    char date[20];
    const char *name;

    if (lstat(path, &info) == -1) {
        perror(path);
        return;
    }

    make_mode_string(info.st_mode, mode);
    owner = getpwuid(info.st_uid);
    group = getgrgid(info.st_gid);

    if (S_ISREG(info.st_mode)) {
        snprintf(size, sizeof(size), "%ld", (long)info.st_size);
    }

    localtime_r(&info.st_mtime, &modification_time);
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M", &modification_time);

    name = strrchr(path, '/');
    name = name == NULL ? path : name + 1;

    printf("%s %3lu %-12s %-12s %10s %s %s\n",
           mode,
           (unsigned long)info.st_nlink,
           owner->pw_name,
           group->gr_name,
           size,
           date,
           name);
}

int main(int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++) {
        print_file(argv[i]);
    }

    return 0;
}

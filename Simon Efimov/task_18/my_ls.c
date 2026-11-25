#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <string.h>
#include <libgen.h>

/* Build the 10-char permission string, e.g. "-rw-r--r--" */
void format_mode(mode_t mode, char *out) {
    if (S_ISDIR(mode))       out[0] = 'd';
    else if (S_ISREG(mode))  out[0] = '-';
    else if (S_ISLNK(mode))  out[0] = 'l';
    else if (S_ISCHR(mode))  out[0] = 'c';
    else if (S_ISBLK(mode))  out[0] = 'b';
    else if (S_ISFIFO(mode)) out[0] = 'p';
    else if (S_ISSOCK(mode)) out[0] = 's';
    else                     out[0] = '?';

    out[1] = (mode & S_IRUSR) ? 'r' : '-';
    out[2] = (mode & S_IWUSR) ? 'w' : '-';
    out[3] = (mode & S_IXUSR) ? 'x' : '-';
    out[4] = (mode & S_IRGRP) ? 'r' : '-';
    out[5] = (mode & S_IWGRP) ? 'w' : '-';
    out[6] = (mode & S_IXGRP) ? 'x' : '-';
    out[7] = (mode & S_IROTH) ? 'r' : '-';
    out[8] = (mode & S_IWOTH) ? 'w' : '-';
    out[9] = (mode & S_IXOTH) ? 'x' : '-';
    out[10] = '\0';
}

/* Format modification time: recent files show HH:MM, old files show year */
void format_time(time_t mtime, char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&mtime);
    double diff = difftime(now, mtime);

    /* 6 months ~ 15552000 seconds */
    if (diff > 15552000.0 || diff < 0) {
        strftime(out, out_size, "%b %e  %Y", tm_info);
    } else {
        strftime(out, out_size, "%b %e %H:%M", tm_info);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file|dir> ...\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (lstat(argv[i], &st) == -1) {
            perror(argv[i]);
            continue;
        }

        char mode_str[11];
        format_mode(st.st_mode, mode_str);

        /* Owner name */
        struct passwd *pw = getpwuid(st.st_uid);
        const char *owner = pw ? pw->pw_name : "?";

        /* Group name */
        struct group *gr = getgrgid(st.st_gid);
        const char *group = gr ? gr->gr_name : "?";

        /* Size: show only for regular files */
        char size_str[32];
        if (S_ISREG(st.st_mode)) {
            snprintf(size_str, sizeof(size_str), "%lld", (long long)st.st_size);
        } else {
            size_str[0] = '\0';
        }

        /* Modification time */
        char time_str[64];
        format_time(st.st_mtime, time_str, sizeof(time_str));

        /* File name only (strip the directory part) */
        char path_copy[1024];
        strncpy(path_copy, argv[i], sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';
        char *name = basename(path_copy);

        printf("%s %4ld %-8s %-8s %8s %s %s\n",
               mode_str,
               (long)st.st_nlink,
               owner,
               group,
               size_str,
               time_str,
               name);
    }

    return 0;
}

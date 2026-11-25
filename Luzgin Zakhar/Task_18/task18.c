#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static void permissions(mode_t mode, char out[11]) {
    static const mode_t bits[9] = {
        S_IRUSR,S_IWUSR,S_IXUSR,S_IRGRP,S_IWGRP,S_IXGRP,S_IROTH,S_IWOTH,S_IXOTH
    };
    const char letters[] = "rwxrwxrwx";
    int i;
    out[0] = S_ISDIR(mode) ? 'd' : (S_ISREG(mode) ? '-' : '?');
    for (i = 0; i < 9; ++i) out[i+1] = (mode & bits[i]) ? letters[i] : '-';
    out[10] = '\0';
}

static const char *base_name(const char *path) {
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

static int print_info(const char *path) {
    struct stat st;
    struct passwd *pw;
    struct group *gr;
    struct tm tm_value;
    char perms[11], stamp[40], uidbuf[32], gidbuf[32];
    const char *owner, *group, *name;
    size_t length = strlen(path);
    char *trimmed = NULL;

    if (length > 1 && path[length-1] == '/') {
        trimmed = strdup(path);
        if (!trimmed) { perror("strdup"); return 1; }
        while (length > 1 && trimmed[length-1] == '/') trimmed[--length] = '\0';
        path = trimmed;
    }
    if (lstat(path, &st) < 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        free(trimmed); return 1;
    }
    permissions(st.st_mode, perms);
    pw = getpwuid(st.st_uid);
    if (pw) owner = pw->pw_name;
    else { snprintf(uidbuf, sizeof(uidbuf), "%lu", (unsigned long)st.st_uid); owner = uidbuf; }
    gr = getgrgid(st.st_gid);
    if (gr) group = gr->gr_name;
    else { snprintf(gidbuf, sizeof(gidbuf), "%lu", (unsigned long)st.st_gid); group = gidbuf; }
    if (!localtime_r(&st.st_mtime, &tm_value) ||
        !strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", &tm_value))
        strcpy(stamp, "unknown");
    name = base_name(path);
    if (*name == '\0') name = "/";
    printf("%s %3lu %-12s %-12s ", perms, (unsigned long)st.st_nlink, owner, group);
    if (S_ISREG(st.st_mode)) printf("%10lld ", (long long)st.st_size);
    else printf("%10s ", "");
    printf("%s %s\n", stamp, name);
    free(trimmed);
    return 0;
}

int main(int argc, char **argv) {
    int i, errors = 0;
    if (argc == 1) return print_info(".");
    for (i = 1; i < argc; ++i) errors |= print_info(argv[i]);
    return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* Сборка: gcc -std=c99 -Wall -Wextra task18.c -o task18
   Запуск: ./task18 task18.c . /tmp
   Сравнение: ls -ld task18.c . /tmp */

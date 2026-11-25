#include <grp.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

void format_mode(mode_t mode, char *str) {
  if (S_ISDIR(mode)) {
    str[0] = 'd';
  } else if (S_ISREG(mode)) {
    str[0] = '-';
  } else {
    str[0] = '?';
  }

  str[1] = (mode & S_IRUSR) ? 'r' : '-';
  str[2] = (mode & S_IWUSR) ? 'w' : '-';
  str[3] = (mode & S_IXUSR) ? 'x' : '-';

  str[4] = (mode & S_IRGRP) ? 'r' : '-';
  str[5] = (mode & S_IWGRP) ? 'w' : '-';
  str[6] = (mode & S_IXGRP) ? 'x' : '-';

  str[7] = (mode & S_IROTH) ? 'r' : '-';
  str[8] = (mode & S_IWOTH) ? 'w' : '-';
  str[9] = (mode & S_IXOTH) ? 'x' : '-';

  str[10] = '\0';
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <file>...\n", argv[0]);
    return 1;
  }

  time_t now = time(NULL);
  time_t six_months = 180 * 24 * 60 * 60;

  for (int i = 1; i < argc; i++) {
    struct stat st;
    if (lstat(argv[i], &st) == -1) {
      perror(argv[i]);
      continue;
    }

    char mode_str[11];
    format_mode(st.st_mode, mode_str);

    struct passwd *pw = getpwuid(st.st_uid);
    const char *user_name = pw ? pw->pw_name : "?";

    struct group *gr = getgrgid(st.st_gid);
    const char *group_name = gr ? gr->gr_name : "?";

    char size_str[32];
    if (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode)) {
      snprintf(size_str, sizeof(size_str), "%lld", (long long)st.st_size);
    } else {
      size_str[0] = '\0';
    }

    char time_str[32];
    struct tm *tm_info = localtime(&st.st_mtime);
    if (tm_info != NULL) {
      if ((now - st.st_mtime) > six_months || st.st_mtime > now) {
        strftime(time_str, sizeof(time_str), "%b %e  %Y", tm_info);
      } else {
        strftime(time_str, sizeof(time_str), "%b %e %H:%M", tm_info);
      }
    } else {
      strcpy(time_str, "?");
    }

    char path_copy[4096];
    strncpy(path_copy, argv[i], sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    char *file_name = basename(path_copy);

    printf("%-10s %4lu %-8s %-8s %10s %-12s %s\n", mode_str,
           (unsigned long)st.st_nlink, user_name, group_name, size_str,
           time_str, file_name);
  }

  return 0;
}

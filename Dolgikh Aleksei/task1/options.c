#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

extern char **environ;

typedef struct {
  int opt;
  char *arg;
} Option;

int main(int argc, char *argv[]) {
  char *optstring = "ispuU:cC:dvV:";
  int c;

  Option *opts = NULL;
  int count = 0;
  int capacity = 0;

  while ((c = getopt(argc, argv, optstring)) != -1) {
    if (c == '?') {
      continue;
    }

    if (count >= capacity) {
      capacity = (capacity == 0) ? 8 : capacity * 2;
      opts = realloc(opts, capacity * sizeof(Option));
      if (!opts) {
        perror("realloc failed");
        exit(EXIT_FAILURE);
      }
    }

    opts[count].opt = c;
    opts[count].arg = optarg;
    count++;
  }

  for (int i = count - 1; i >= 0; i--) {
    switch (opts[i].opt) {
    case 'i': {
      printf("UID: real=%d, eff=%d; GID: real=%d, eff=%d\n", (int)getuid(),
             (int)geteuid(), (int)getgid(), (int)getegid());
      break;
    }
    case 's': {
      if (setpgid(0, 0) == -1) {
        perror("setpgid failed");
      } else {
        printf("Process became group leader. New PGID: %d\n", (int)getpgrp());
      }
      break;
    }
    case 'p': {
      printf("PID: %d, PPID: %d, PGID: %d\n", (int)getpid(), (int)getppid(),
             (int)getpgrp());
      break;
    }
    case 'u': {
      struct rlimit rl;
      if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        printf("ulimit (open files): soft=%f, hard=%f\n", (double)rl.rlim_cur,
               (double)rl.rlim_max);
      } else {
        perror("getrlimit failed");
      }
      break;
    }
    case 'U': {
      char *endptr;
      long val = strtol(opts[i].arg, &endptr, 10);
      if (*endptr != '\0' || val < 0) {
        fprintf(stderr, "Error: Invalid ulimit value: %s\n", opts[i].arg);
      } else {
        struct rlimit rl;
        if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
          rl.rlim_cur = (rlim_t)val;
          if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
            perror("setrlimit ulimit failed");
          } else {
            printf("ulimit changed to %ld\n", val);
          }
        } else {
          perror("getrlimit failed");
        }
      }
      break;
    }
    case 'c': {
      struct rlimit rl;
      if (getrlimit(RLIMIT_CORE, &rl) == 0) {
        printf("Core file size limit: soft=%f bytes, hard=%f bytes\n",
               (double)rl.rlim_cur, (double)rl.rlim_max);
      } else {
        perror("getrlimit failed");
      }
      break;
    }
    case 'C': {
      char *endptr;
      long val = strtol(opts[i].arg, &endptr, 10);
      if (*endptr != '\0' || val < 0) {
        fprintf(stderr, "Error: Invalid core size value: %s\n", opts[i].arg);
      } else {
        struct rlimit rl;
        if (getrlimit(RLIMIT_CORE, &rl) == 0) {
          rl.rlim_cur = (rlim_t)val;
          if (setrlimit(RLIMIT_CORE, &rl) == -1) {
            perror("setrlimit core size failed");
          } else {
            printf("Core size changed to %ld bytes\n", val);
          }
        } else {
          perror("getrlimit failed");
        }
      }
      break;
    }
    case 'd': {
      char cwd[PATH_MAX];
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current Working Directory: %s\n", cwd);
      } else {
        perror("getcwd failed");
      }
      break;
    }
    case 'v': {
      printf("--- Environment Variables ---\n");
      for (char **env = environ; *env != NULL; env++) {
        printf("%s\n", *env);
      }
      printf("-----------------------------\n");
      break;
    }
    case 'V': {
      if (putenv(opts[i].arg) != 0) {
        perror("putenv failed");
      } else {
        printf("Environment variable set: %s\n", opts[i].arg);
      }
      break;
    }
    default:
      break;
    }
  }

  free(opts);
  return 0;
}

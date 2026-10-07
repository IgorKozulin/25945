ч#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

extern char **environ;

void print_ids(void) {
    printf("-i: UID=%d, EUID=%d, GID=%d, EGID=%d\n",
           getuid(), geteuid(), getgid(), getegid());
}

void set_leader(void) {
    if (setpgid(0, 0) == -1) {
        perror("setpgid");
    } else {
        printf("-s: process is now group leader, PGID=%d\n", getpgrp());
    }
}

void print_pids(void) {
    printf("-p: PID=%d, PPID=%d, PGID=%d\n",
           getpid(), getppid(), getpgrp());
}

void print_ulimit(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("getrlimit");
        return;
    }
    printf("-u: RLIMIT_NOFILE: soft=%llu, hard=%llu\n",
           (unsigned long long)rl.rlim_cur,
           (unsigned long long)rl.rlim_max);
}

void set_ulimit(const char *val) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("getrlimit");
        return;
    }
    long new_val = atol(val);
    if (new_val < 0) {
        fprintf(stderr, "-U: invalid value: %s\n", val);
        return;
    }
    rl.rlim_cur = (rlim_t)new_val;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("setrlimit");
    } else {
        printf("-U: RLIMIT_NOFILE set to %ld\n", new_val);
    }
}

void print_core(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == -1) {
        perror("getrlimit");
        return;
    }
    printf("-c: RLIMIT_CORE: soft=%llu, hard=%llu\n",
           (unsigned long long)rl.rlim_cur,
           (unsigned long long)rl.rlim_max);
}

void set_core(const char *val) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == -1) {
        perror("getrlimit");
        return;
    }
    long new_val = atol(val);
    if (new_val < 0) {
        fprintf(stderr, "-C: invalid value: %s\n", val);
        return;
    }
    rl.rlim_cur = (rlim_t)new_val;
    if (setrlimit(RLIMIT_CORE, &rl) == -1) {
        perror("setrlimit");
    } else {
        printf("-C: RLIMIT_CORE set to %ld\n", new_val);
    }
}

void print_cwd(void) {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        perror("getcwd");
    } else {
        printf("-d: cwd=%s\n", buf);
    }
}

void print_env(void) {
    printf("-v: environment variables:\n");
    for (char **env = environ; *env != NULL; env++) {
        printf("  %s\n", *env);
    }
}

void set_env(const char *arg) {
    char *copy = strdup(arg);
    if (copy == NULL) {
        perror("strdup");
        return;
    }
    if (putenv(copy) != 0) {
        perror("putenv");
        free(copy);
    } else {
        printf("-V: set %s\n", arg);
    }
    // Не free(copy) — putenv не копирует строку!
}

#define MAX_OPTS 100

struct opt_item {
    char opt;
    char *arg;
};

int main(int argc, char *argv[]) {
    char *options = "ispuU:cC:dvV:";
    int c;
    struct opt_item opts[MAX_OPTS];
    int opt_count = 0;

    while ((c = getopt(argc, argv, options)) != -1) {
        if (opt_count < MAX_OPTS) {
            opts[opt_count].opt = c;
            opts[opt_count].arg = optarg;
            opt_count++;
        }
        if (c == '?') {
            fprintf(stderr, "Invalid option: -%c\n", optopt);
        }
    }

    for (int i = opt_count - 1; i >= 0; i--) {
        switch (opts[i].opt) {
            case 'i': print_ids(); break;
            case 's': set_leader(); break;
            case 'p': print_pids(); break;
            case 'u': print_ulimit(); break;
            case 'U': set_ulimit(opts[i].arg); break;
            case 'c': print_core(); break;
            case 'C': set_core(opts[i].arg); break;
            case 'd': print_cwd(); break;
            case 'v': print_env(); break;
            case 'V': set_env(opts[i].arg); break;
        }
    }

    return 0;
}

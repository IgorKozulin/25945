#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

extern char **environ;

struct opt_rec {
    char  opt;
    char *arg;
};

void print_ids() {
    printf("Real UID: %d\n", (int)getuid());
    printf("Effective UID: %d\n", (int)geteuid());
    printf("Real GID: %d\n", (int)getgid());
    printf("Effective GID: %d\n", (int)getegid());
}

void make_group_leader() {
    if (setpgid(0, 0) == -1)
        perror("setpgid");
    else
        printf("Process became group leader (PGID = %d)\n", (int)getpgrp());
}

void print_pids() {
    printf("PID: %d, PPID: %d, PGID: %d\n",
           (int)getpid(), (int)getppid(), (int)getpgrp());
}

void print_ulimit() {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) { perror("getrlimit"); return; }
    printf("ulimit (max open files): %lld\n", (long long)rl.rlim_cur);
}

void set_ulimit(const char *s) {
    struct rlimit rl;
    char *end;
    long val;

    errno = 0;
    val = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || val < 0) {
        fprintf(stderr, "-U: invalid value '%s'\n", s);
        return;
    }
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) { perror("getrlimit"); return; }
    rl.rlim_cur = (rlim_t)val;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
        perror("setrlimit");
    else
        printf("ulimit set to %ld (max open files)\n", val);
}

void print_core_size() {
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == -1) { perror("getrlimit"); return; }
    if (rl.rlim_cur == RLIM_INFINITY)
        printf("Core file size: unlimited\n");
    else
        printf("Core file size: %lld bytes\n", (long long)rl.rlim_cur);
}

void set_core_size(const char *s) {
    struct rlimit rl;
    char *end;
    long val;

    errno = 0;
    val = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || val < 0) {
        fprintf(stderr, "-C: invalid size '%s'\n", s);
        return;
    }
    if (getrlimit(RLIMIT_CORE, &rl) == -1) { perror("getrlimit"); return; }
    rl.rlim_cur = (rlim_t)val;
    if (setrlimit(RLIMIT_CORE, &rl) == -1)
        perror("setrlimit");
    else
        printf("Core file size set to %ld bytes\n", val);
}

void print_cwd() {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof buf) == NULL)
        perror("getcwd");
    else
        printf("Current directory: %s\n", buf);
}

void print_env() {
    char **e;
    for (e = environ; *e != NULL; e++)
        printf("%s\n", *e);
}

void set_env(char *nv) {
    char *eq = strchr(nv, '=');
    if (eq == NULL || eq == nv) {
        fprintf(stderr, "-V: argument must be NAME=value, got '%s'\n", nv);
        return;
    }
    if (putenv(nv) != 0)
        perror("putenv");
    else
        printf("Environment set: %s\n", nv);
}

int main(int argc, char *argv[]) {
    const char *optstring = "ispuU:cC:dvV:";
    struct opt_rec *ops = NULL;
    size_t n = 0, cap = 0;
    int c, i;

    while ((c = getopt(argc, argv, optstring)) != -1) {
        switch (c) {
        case 'i': case 's': case 'p': case 'u':
        case 'c': case 'd': case 'v':
        case 'U': case 'C': case 'V':
            if (n == cap) {
                cap = cap ? cap * 2 : 8;
                ops = (struct opt_rec *)realloc(ops, cap * sizeof *ops);
                if (ops == NULL) { perror("realloc"); exit(EXIT_FAILURE); }
            }
            ops[n].opt = c;
            ops[n].arg = (c == 'U' || c == 'C' || c == 'V') ? optarg : NULL;
            n++;
            break;
        case '?':
        default:
            fprintf(stderr, "Unknown or invalid option: '%c'\n", optopt);
            break;
        }
    }

    for (i = (int)n - 1; i >= 0; i--) {
        switch (ops[i].opt) {
        case 'i': print_ids();                 break;
        case 's': make_group_leader();         break;
        case 'p': print_pids();                break;
        case 'u': print_ulimit();              break;
        case 'U': set_ulimit(ops[i].arg);      break;
        case 'c': print_core_size();           break;
        case 'C': set_core_size(ops[i].arg);   break;
        case 'd': print_cwd();                 break;
        case 'v': print_env();                 break;
        case 'V': set_env(ops[i].arg);         break;
        default:  break;
        }
    }

    free(ops);
    return 0;
}

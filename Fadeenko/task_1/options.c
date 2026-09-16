#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

extern char **environ;

#define MAX_OPTS 1024

typedef struct {
    int option;
    char *argument;
} Option;

static void print_ids(void)
{
    printf("UID: real=%d effective=%d\n", (int)getuid(), (int)geteuid());
    printf("GID: real=%d effective=%d\n", (int)getgid(), (int)getegid());
}

static void print_process_info(void)
{
    printf("PID: %d PPID: %d PGID: %d\n",
           (int)getpid(), (int)getppid(), (int)getpgrp());
}

static void print_ulimit(void)
{
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
        perror("getrlimit");
        return;
    }

    if (limit.rlim_cur == RLIM_INFINITY)
        printf("ulimit: unlimited\n");
    else
        printf("ulimit: %llu\n", (unsigned long long)limit.rlim_cur);
}

static void set_ulimit(const char *value)
{
    char *end;
    unsigned long long number;
    struct rlimit limit;

    errno = 0;
    end = NULL;
    number = strtoull(value, &end, 10);

    if (errno != 0 || end == value || *end != '\0') {
        fprintf(stderr, "Invalid value for -U: %s\n", value);
        return;
    }

    if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
        perror("getrlimit");
        return;
    }

    limit.rlim_cur = (rlim_t)number;

    if (setrlimit(RLIMIT_NOFILE, &limit) == -1)
        perror("setrlimit");
}

static void print_core_size(void)
{
    struct rlimit limit;

    if (getrlimit(RLIMIT_CORE, &limit) == -1) {
        perror("getrlimit");
        return;
    }

    if (limit.rlim_cur == RLIM_INFINITY)
        printf("core size: unlimited\n");
    else
        printf("core size: %llu bytes\n",
               (unsigned long long)limit.rlim_cur);
}

static void set_core_size(const char *value)
{
    char *end;
    unsigned long long number;
    struct rlimit limit;

    errno = 0;
    end = NULL;
    number = strtoull(value, &end, 10);

    if (errno != 0 || end == value || *end != '\0') {
        fprintf(stderr, "Invalid value for -C: %s\n", value);
        return;
    }

    if (getrlimit(RLIMIT_CORE, &limit) == -1) {
        perror("getrlimit");
        return;
    }

    limit.rlim_cur = (rlim_t)number;

    if (setrlimit(RLIMIT_CORE, &limit) == -1)
        perror("setrlimit");
}

static void print_directory(void)
{
    char buffer[PATH_MAX];

    if (getcwd(buffer, sizeof(buffer)) == NULL) {
        perror("getcwd");
        return;
    }

    printf("%s\n", buffer);
}

static void print_environment(void)
{
    char **env;

    for (env = environ; *env != NULL; ++env)
        printf("%s\n", *env);
}

static void set_environment(const char *value)
{
    char *copy;

    copy = strdup(value);
    if (copy == NULL) {
        perror("strdup");
        return;
    }

    if (strchr(copy, '=') == NULL) {
        fprintf(stderr, "Invalid value for -V: %s\n", value);
        free(copy);
        return;
    }

    if (putenv(copy) != 0) {
        perror("putenv");
        free(copy);
    }

}

static void execute_option(const Option *opt)
{
    switch (opt->option) {
        case 'i':
            print_ids();
            break;

        case 's':
            if (setpgid(0, 0) == -1)
                perror("setpgid");
            break;

        case 'p':
            print_process_info();
            break;

        case 'u':
            print_ulimit();
            break;

        case 'U':
            set_ulimit(opt->argument);
            break;

        case 'c':
            print_core_size();
            break;

        case 'C':
            set_core_size(opt->argument);
            break;

        case 'd':
            print_directory();
            break;

        case 'v':
            print_environment();
            break;

        case 'V':
            set_environment(opt->argument);
            break;

        default:
            break;
    }
}

int main(int argc, char *argv[])
{
    Option options[MAX_OPTS];
    int option_count = 0;
    int c;
    int i;

    while ((c = getopt(argc, argv, "ispuU:cC:dvV:")) != -1) {

        if (c == '?') {
            if (optopt != 'U' && optopt != 'C' && optopt != 'V') {
                fprintf(stderr, "Invalid option: -%c\n", optopt);
            } else {
                fprintf(stderr, "Option -%c requires an argument\n", optopt);
            }
            continue;
        }

        if (option_count >= MAX_OPTS) {
            fprintf(stderr, "Too many options\n");
            return EXIT_FAILURE;
        }

        options[option_count].option = c;
        options[option_count].argument = NULL;

        if (c == 'U' || c == 'C' || c == 'V')
            options[option_count].argument = optarg;

        option_count++;
    }

    for (i = option_count - 1; i >= 0; --i)
        execute_option(&options[i]);

    return EXIT_SUCCESS;
}

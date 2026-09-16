#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <limits.h>
#include <string.h>

extern char **environ;

typedef struct {
    char option;
    char *arg;
} Command;

int main(int argc, char *argv[])
{
    char *options = "ispuU:cC:dvV:";
    Command commands[argc];
    int count = 0;
    int c;

    while ((c = getopt(argc, argv, options)) != -1) {
        if (c == '?') {
            printf("Invalid option: %c\n", optopt);
            continue;
        }

        commands[count].option = c;
        commands[count].arg = optarg ? strdup(optarg) : NULL;
        count++;
    }

    for (int i = count - 1; i >= 0; i--) {
        switch (commands[i].option) {
            case 'i':
                printf("UID: %ld EUID: %ld\n",
                       (long)getuid(), (long)geteuid());
                printf("GID: %ld EGID: %ld\n",
                       (long)getgid(), (long)getegid());
                break;

            case 's':
                setpgid(0, 0);
                break;

            case 'p':
                printf("PID: %ld PPID: %ld PGID: %ld\n",
                       (long)getpid(),
                       (long)getppid(),
                       (long)getpgrp());
                break;

            case 'u': {
                struct rlimit limit;
                getrlimit(RLIMIT_NOFILE, &limit);
                printf("ulimit: %ld\n", (long)limit.rlim_cur);
                break;
            }

            case 'U': {
                long value = atol(commands[i].arg);

                if (value < 0) {
                    printf("Invalid value for -U\n");
                    break;
                }

                struct rlimit limit;
                getrlimit(RLIMIT_NOFILE, &limit);
                limit.rlim_cur = value;
                setrlimit(RLIMIT_NOFILE, &limit);
                break;
            }

            case 'c': {
                struct rlimit limit;
                getrlimit(RLIMIT_CORE, &limit);
                printf("Core file size: %ld\n",
                       (long)limit.rlim_cur);
                break;
            }

            case 'C': {
                struct rlimit limit;
                getrlimit(RLIMIT_CORE, &limit);
                limit.rlim_cur = atol(commands[i].arg);
                setrlimit(RLIMIT_CORE, &limit);
                break;
            }

            case 'd': {
                char cwd[PATH_MAX];
                getcwd(cwd, sizeof(cwd));
                printf("%s\n", cwd);
                break;
            }

            case 'v':
                for (char **env = environ; *env != NULL; env++)
                    printf("%s\n", *env);
                break;

            case 'V':
                putenv(commands[i].arg);
                break;
        }
    }

    return 0;
}

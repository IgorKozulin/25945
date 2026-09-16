#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/resource.h>

extern char **environ;

struct opt_item {
    int opt;
    char *arg;
};

int main(int argc, char *argv[]) {
    char *options = "ispuU:cC:dvV:";
    int c;
    
    struct opt_item opts[100];
    int count = 0;
    
    while ((c = getopt(argc, argv, options)) != -1) {
        opts[count].opt = c;
        opts[count].arg = optarg ? strdup(optarg) : NULL;
        count++;
    }

    for (int i = count - 1; i >= 0; i--) {
        struct rlimit rlim;
        char cwd[PATH_MAX];
        
        switch (opts[i].opt) {
            case 'i':
                printf("UID: %d\nGID: %d\nEUID: %d\nEGID: %d\n", 
                    getuid(), getgid(), geteuid(), getegid());
                break;
                
            case 's':
                if (setpgid(0, 0) == 0) printf("Complete\n");
                else printf("Error\n");
                break;
                
            case 'p':
                printf("PID: %d\nPPID: %d\nPGID: %d\n",
                    getpid(), getppid(), getpgrp());
                break;
                
            case 'u':
                if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
                    if (rlim.rlim_cur == RLIM_INFINITY)
                        printf("ulimit: unlimited\n");
                    else
                        printf("ulimit: %lu\n", (unsigned long)rlim.rlim_cur);
                }
                break;
                
            case 'U': {
                char *endptr;
                errno = 0;
                long val = strtol(opts[i].arg, &endptr, 10);
                if (errno != 0 || *endptr != '\0' || val < 0) {
                    fprintf(stderr, "Invalid value for -U: %s\n", opts[i].arg);
                    break;
                }
                if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
                    rlim.rlim_cur = val;
                    if (setrlimit(RLIMIT_NOFILE, &rlim) == 0)
                        printf("ulimit changed to %ld\n", val);
                    else
                        perror("setrlimit");
                }
                break;
            }
                
            case 'c':
                if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
                    if (rlim.rlim_cur == RLIM_INFINITY)
                        printf("core size: unlimited\n");
                    else
                        printf("core size: %lu\n", (unsigned long)rlim.rlim_cur);
                }
                break;
                
            case 'C': {
                char *endptr;
                errno = 0;
                long val = strtol(opts[i].arg, &endptr, 10);
                if (errno != 0 || *endptr != '\0' || val < 0) {
                    fprintf(stderr, "Invalid value for -C: %s\n", opts[i].arg);
                    break;
                }
                if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
                    rlim.rlim_cur = val;
                    if (setrlimit(RLIMIT_CORE, &rlim) == 0)
                        printf("core size changed to %ld\n", val);
                    else
                        perror("setrlimit");
                }
                break;
            }
                
            case 'd':
                if (getcwd(cwd, sizeof(cwd)) != NULL)
                    printf("Directory: %s\n", cwd);
                else
                    perror("getcwd");
                break;
                
            case 'v':
                for (char **env = environ; *env != NULL; env++)
                    printf("%s\n", *env);
                break;
                
            case 'V':
                if (putenv(opts[i].arg) == 0)
                    printf("Set: %s\n", opts[i].arg);
                else
                    perror("putenv");
                break;
                
            case '?':
                printf("Invalid option: %c\n", optopt);
                break;
        }
    }
    
    for (int i = 0; i < count; i++)
        free(opts[i].arg);
    
    return 0;
}
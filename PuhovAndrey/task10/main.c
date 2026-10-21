#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    pid_t pid;
    int status;

    if (argc < 2) {
        printf("Использование: %s команда [аргументы...]\n", argv[0]);
        return 1;
    }

    pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execvp(argv[1], &argv[1]);
        perror(argv[1]);
        _exit(1);
    }

    waitpid(pid, &status, 0);
    printf("Код завершения: %d\n", WEXITSTATUS(status));

    return 0;
}

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execlp("cat", "cat", "long_file.txt", (char *)NULL);
        perror("cat");
        _exit(1);
    }

    printf("Родитель: подпроцесс запущен.\n");
    fflush(stdout);

    wait(NULL);
    printf("Родитель: подпроцесс завершен.\n");

    return 0;
}

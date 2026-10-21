#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    int pipe_fds[2];
    pid_t pid;
    const char text[] = "Hello from parent and child!\n";

    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        return 1;
    }

    pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        char buffer[256];
        ssize_t count;

        close(pipe_fds[1]);

        while ((count = read(pipe_fds[0], buffer, sizeof(buffer))) > 0) {
            ssize_t i;

            for (i = 0; i < count; i++) {
                buffer[i] = (char)toupper((unsigned char)buffer[i]);
            }

            write(STDOUT_FILENO, buffer, (size_t)count);
        }

        close(pipe_fds[0]);
        return 0;
    }

    close(pipe_fds[0]);
    write(pipe_fds[1], text, sizeof(text) - 1);
    close(pipe_fds[1]);
    wait(NULL);

    return 0;
}

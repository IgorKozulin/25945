#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <locale.h>
#include <sys/wait.h>

int write_all(int fd, const char *buf, size_t size)
{
    while (size > 0) {
        ssize_t n = write(fd, buf, size);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf += n;
        size -= (size_t)n;
    }
    return 0;
}

int main(void)
{
    int p[2], status, failed = 0, line_start = 1;
    pid_t pid;
    char buf[1024];
    ssize_t n, i;

    setlocale(LC_CTYPE, "");
    signal(SIGPIPE, SIG_IGN);
    if (pipe(p) == -1) {
        perror("pipe");
        return 1;
    }
    pid = fork();
    if (pid == -1) {
        perror("fork");
        close(p[0]);
        close(p[1]);
        return 1;
    }
    if (pid == 0) {
        close(p[1]);
        while ((n = read(p[0], buf, sizeof(buf))) != 0) {
            if (n == -1) {
                if (errno == EINTR)
                    continue;
                perror("read");
                _exit(1);
            }
            for (i = 0; i < n; i++)
                buf[i] = (char)toupper((unsigned char)buf[i]);
            if (write_all(1, buf, (size_t)n) == -1) {
                perror("write");
                _exit(1);
            }
        }
        close(p[0]);
        _exit(0);
    }

    close(p[0]);
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        n = strlen(buf);
        if (line_start && n == 1 && buf[0] == '\n')
            break;
        line_start = n > 0 && buf[n - 1] == '\n';
        if (write_all(p[1], buf, (size_t)n) == -1) {
            perror("write");
            failed = 1;
            break;
        }
    }
    if (ferror(stdin))
        failed = 1;
    close(p[1]);
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR)
            continue;
        perror("waitpid");
        return 1;
    }
    return failed || !WIFEXITED(status) || WEXITSTATUS(status) != 0;
}

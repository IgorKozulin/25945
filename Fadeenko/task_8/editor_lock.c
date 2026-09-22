#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int fd;
    int status;
    const char *editor;
    char command[1024];
    struct flock lock = {0};

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return EXIT_FAILURE;
    }

    fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(fd, F_SETLK, &lock) == -1) {
        if (errno == EACCES || errno == EAGAIN)
            fprintf(stderr, "File is already locked\n");
        else
            perror("fcntl");
        close(fd);
        return EXIT_FAILURE;
    }

    editor = getenv("EDITOR");
    if (editor == NULL || *editor == '\0')
        editor = "vi";

    if (strchr(argv[1], '\'') != NULL) {
        fprintf(stderr, "Apostrophe is not supported in the file name\n");
        close(fd);
        return EXIT_FAILURE;
    }

    if (snprintf(command, sizeof(command), "%s '%s'", editor, argv[1]) >= (int)sizeof(command)) {
        fprintf(stderr, "Command is too long\n");
        close(fd);
        return EXIT_FAILURE;
    }

    status = system(command);
    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, &lock) == -1)
        perror("fcntl");
    close(fd);

    if (status == -1) {
        perror("system");
        return EXIT_FAILURE;
    }
    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

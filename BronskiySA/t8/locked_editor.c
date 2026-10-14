#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
/* Quote filenames for the shell used by system(), including embedded quotes. */
static char *quote(const char *text) {
    size_t length = 3;
    for (const char *p = text; *p; ++p) length += *p == '\'' ? 4 : 1;
    char *out = malloc(length);
    if (!out) return NULL;
    char *p = out; *p++ = '\'';
    while (*text) {
        if (*text == '\'') { memcpy(p, "'\\''", 4); p += 4; }
        else *p++ = *text;
        ++text;
    }
    *p++ = '\''; *p = '\0'; return out;
}
int main(int argc, char **argv) {
    int mandatory = argc == 3 && !strcmp(argv[1], "--mandatory");
    if (argc != 2 && !mandatory) { fprintf(stderr, "Usage: locked_editor [--mandatory] file\n"); return 1; }
#if !defined(__sun)
    if (mandatory) {
        fprintf(stderr, "Mandatory locking experiment requires Solaris and a supporting filesystem.\n"); return 1;
    }
#endif
    const char *path = argv[mandatory ? 2 : 1];
    int fd = open(path, O_RDWR);
    if (fd < 0) { perror(path); return 1; }
    struct stat st;
    if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return 1; }
    if (!S_ISREG(st.st_mode)) { fprintf(stderr, "Expected a regular file\n"); close(fd); return 1; }
    if (mandatory && (!(st.st_mode & S_ISGID) || (st.st_mode & S_IXGRP))) {
        fprintf(stderr, "Prepare mandatory-lock mode manually: chmod g+s,g-x file\n"); close(fd); return 1;
    }
    /* Keep this descriptor open in the parent while system() runs. */
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) { perror("fcntl"); close(fd); return 1; }
    struct flock lock;
    memset(&lock, 0, sizeof lock);
    lock.l_type = F_WRLCK; lock.l_whence = SEEK_SET;
    lock.l_start = 0; lock.l_len = 0; /* through EOF, including future growth */
    if (fcntl(fd, F_SETLK, &lock) < 0) { perror("File is locked (or locking failed)"); close(fd); return 1; }
    puts("Whole-file lock acquired; starting editor."); fflush(stdout);
    /* A relative name beginning with '-' is made unambiguous for the editor. */
    char *safe_path = malloc(strlen(path) + 3);
    if (!safe_path) { perror("malloc"); close(fd); return 1; }
    sprintf(safe_path, "%s%s", path[0] == '-' ? "./" : "", path);
    char *quoted = quote(safe_path); free(safe_path);
    if (!quoted) { perror("malloc"); close(fd); return 1; }
    const char *editor = getenv("EDITOR");
    if (!editor || !*editor) editor = "vi";
    /* EDITOR is intentionally a shell command chosen by the caller. */
    size_t length = strlen(editor) + strlen(quoted) + 2;
    char *command = malloc(length);
    if (!command) { perror("malloc"); free(quoted); close(fd); return 1; }
    snprintf(command, length, "%s %s", editor, quoted); free(quoted);
    int status = system(command); free(command);
    lock.l_type = F_UNLCK;
    int failed = 0;
    if (fcntl(fd, F_SETLK, &lock) < 0) { perror("unlock"); failed = 1; }
    if (close(fd) < 0) { perror("close"); failed = 1; }
    if (status < 0) { perror("system"); return 1; }
    if (failed) return 1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
}

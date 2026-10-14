#define TIMED 1
#define MAPPED 1
/* Line lookup: offsets, interactive selection and resource cleanup. */
#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#ifndef TIMED
#define TIMED 0
#endif
#ifndef MAPPED
#define MAPPED 0
#endif
struct line_info { off_t offset; size_t length; };
static struct line_info *lines;
static size_t count, capacity;
static volatile sig_atomic_t expired;
static void on_alarm(int signo) { (void)signo; expired = 1; }
static int append_line(off_t offset, size_t length) {
    if (count == capacity) {
        size_t next = capacity ? capacity * 2 : 64;
        if (next < capacity || next > SIZE_MAX / sizeof(*lines)) { errno = ENOMEM; return -1; }
        void *p = realloc(lines, next * sizeof(*lines));
        if (!p) return -1;
        lines = p; capacity = next;
    }
    lines[count++] = (struct line_info){offset, length}; return 0;
}
/* SIGALRM is blocked outside pselect, so expiry cannot be lost between
   checking the flag and waiting. The handler only changes sig_atomic_t. */
static int input_number(char *text, size_t size, const sigset_t *waitmask) {
    size_t used = 0; int overflow = 0;
    expired = 0;
    if (TIMED) alarm(5);
    for (;;) {
        fd_set ready; FD_ZERO(&ready); FD_SET(STDIN_FILENO, &ready);
        if (expired) { alarm(0); return 2; }
        int rc = pselect(STDIN_FILENO + 1, &ready, NULL, NULL, NULL, waitmask);
        if (rc < 0) {
            if (errno == EINTR) continue;
            perror("pselect"); alarm(0); return -1;
        }
        if (expired) { alarm(0); return 2; }
        int ch = fgetc(stdin); /* stdin is unbuffered, including for pipes. */
        if (ch == EOF) {
            alarm(0);
            if (ferror(stdin)) { perror("stdin"); return -1; }
            if (!used && !overflow) return 0;
            break;
        }
        if (ch == '\n') break;
        if (used + 1 < size) text[used++] = (char)ch;
        else overflow = 1;
    }
    alarm(0); text[used] = '\0'; return overflow ? 3 : 1;
}
#if !MAPPED
static int print_range(int fd, off_t offset, size_t length) {
    if (lseek(fd, offset, SEEK_SET) < 0) return -1;
    char *buf = malloc(length + 1);
    if (!buf) return -1;
    size_t used = 0;
    while (used < length) {
        size_t chunk = length - used;
        if (chunk > 8192) chunk = 8192;
        ssize_t n = read(fd, buf + used, chunk);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { if (!n) errno = EIO; free(buf); return -1; }
        used += (size_t)n;
    }
    buf[length] = '\0';
    int rc = printf("%s", buf) < 0 ? -1 : 0;
    free(buf); return rc;
}
#endif
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Usage: %s file\n", argv[0]); return 1; }
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror(argv[1]); return 1; }
    struct stat st;
    if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return 1; }
    if (!S_ISREG(st.st_mode) || st.st_size < 0 || (uintmax_t)st.st_size >= SIZE_MAX) {
        fprintf(stderr, "Expected a regular file of supported size\n"); close(fd); return 1;
    }
    size_t size = (size_t)st.st_size;
    int result = 0;
#if MAPPED
    const char *map = NULL;
    if (size) {
        map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (map == MAP_FAILED) { perror("mmap"); close(fd); return 1; }
    }
    close(fd); fd = -1;
    size_t start = 0;
    for (size_t i = 0; i < size; ++i) {
        if (map[i] == '\n') {
            if (append_line((off_t)start, i + 1 - start) < 0) goto fail;
            start = i + 1;
        }
    }
    if (start < size && append_line((off_t)start, size - start) < 0) goto fail;
#else
    off_t start = 0; size_t length = 0; char ch;
    for (;;) {
        ssize_t n = read(fd, &ch, 1);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) goto fail;
        if (!n) break;
        ++length;
        if (ch == '\n') {
            if (append_line(start, length) < 0) goto fail;
            start = lseek(fd, 0, SEEK_CUR);
            if (start < 0) goto fail;
            length = 0;
        }
    }
    if (length && append_line(start, length) < 0) goto fail;
#endif
    if (!count) { puts("File is empty."); goto done; }
    for (size_t i = 0; i < count; ++i)
        printf("Line %zu: Offset = %" PRIdMAX ", Length = %zu\n", i + 1, (intmax_t)lines[i].offset, lines[i].length);
    sigset_t blocked, oldmask;
    sigemptyset(&blocked); sigemptyset(&oldmask);
    if (TIMED) {
        sigaddset(&blocked, SIGALRM);
        if (sigprocmask(SIG_BLOCK, &blocked, &oldmask) < 0) goto fail;
        struct sigaction action;
        memset(&action, 0, sizeof action); action.sa_handler = on_alarm; sigemptyset(&action.sa_mask);
        if (sigaction(SIGALRM, &action, NULL) < 0) goto fail;
    }
    setvbuf(stdin, NULL, _IONBF, 0);
    for (;;) {
        printf("Enter line number (0 to quit%s):\n", TIMED ? ", 5 sec timeout" : "");
        fflush(stdout);
        char text[128];
        int rc = input_number(text, sizeof text, &oldmask);
        if (rc < 0) { result = 1; break; }
        if (!rc) break;
        if (rc == 2) {
            puts("[TIMEOUT] Full file:");
#if MAPPED
            if (size && fwrite(map, 1, size, stdout) != size) goto fail;
#else
            if (print_range(fd, 0, size) < 0) goto fail;
#endif
            break;
        }
        char *begin = text, *end;
        while (isspace((unsigned char)*begin)) ++begin;
        errno = 0; uintmax_t number = strtoumax(begin, &end, 10);
        while (isspace((unsigned char)*end)) ++end;
        if (rc == 3 || !*begin || begin == end || *begin == '-' || errno || *end || number > count) {
            fprintf(stderr, "Invalid line number\n"); continue;
        }
        if (!number) break;
        struct line_info line = lines[number - 1];
#if MAPPED
        if (fwrite(map + line.offset, 1, line.length, stdout) != line.length) goto fail;
        if (map[line.offset + (off_t)line.length - 1] != '\n') putchar('\n');
#else
        if (print_range(fd, line.offset, line.length) < 0) goto fail;
        /* A separator keeps the next prompt off an unterminated last line. */
        if ((uintmax_t)line.offset + line.length == size) putchar('\n');
#endif
    }
    goto done;
fail:
    perror("line lookup"); result = 1;
done:
    alarm(0);
    free(lines);
#if MAPPED
    if (size && munmap((void *)map, size) < 0) { perror("munmap"); result = 1; }
#endif
    if (fd >= 0 && close(fd) < 0) { perror("close"); result = 1; }
    return result;
}

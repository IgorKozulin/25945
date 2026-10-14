#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <ulimit.h>
#include <unistd.h>

extern char **environ;

struct action {
    int option;
    const char *argument;
};

static void usage(const char *program)
{
    fprintf(stderr,
            "Использование: %s [-i] [-s] [-p] [-u] [-Uчисло] "
            "[-c] [-Cчисло] [-d] [-v] [-Vимя=значение]\n",
            program);
}

/* Строгая проверка десятичного числа (в том числе ошибочного -U). */
static int parse_nonnegative(const char *text, unsigned long long *number)
{
    char *end;
    unsigned long long result;

    if (text == NULL || *text == '\0' || *text == '-' || *text == '+')
        return -1;
    errno = 0;
    result = strtoull(text, &end, 10);
    if (errno == ERANGE || *end != '\0')
        return -1;
    *number = result;
    return 0;
}

static void print_ulimit(void)
{
    long result;
    errno = 0;
    result = ulimit(UL_GETFSIZE, 0L);
    if (result == -1L && errno != 0)
        perror("ulimit(UL_GETFSIZE)");
    else
        printf("ulimit (блоки по 512 байт): %ld\n", result);
}

static int change_ulimit(const char *text)
{
    unsigned long long number;
    long result;

    if (parse_nonnegative(text, &number) < 0 || number > LONG_MAX) {
        fprintf(stderr, "Некорректное значение -U: %s\n", text);
        return -1;
    }
    errno = 0;
    result = ulimit(UL_SETFSIZE, (long)number);
    if (result == -1L && errno != 0) {
        perror("ulimit(UL_SETFSIZE)");
        return -1;
    }
    printf("ulimit изменён на %ld блоков по 512 байт\n", result);
    return 0;
}

static void print_core_limit(void)
{
    struct rlimit limit;
    if (getrlimit(RLIMIT_CORE, &limit) == -1) {
        perror("getrlimit(RLIMIT_CORE)");
        return;
    }
    if (limit.rlim_cur == RLIM_INFINITY)
        puts("Максимальный размер core-файла: unlimited");
    else
        printf("Максимальный размер core-файла: %llu байт\n",
               (unsigned long long)limit.rlim_cur);
}

static int change_core_limit(const char *text)
{
    struct rlimit limit;
    unsigned long long number;

    if (parse_nonnegative(text, &number) < 0) {
        fprintf(stderr, "Некорректное значение -C: %s\n", text);
        return -1;
    }
    if (getrlimit(RLIMIT_CORE, &limit) == -1) {
        perror("getrlimit(RLIMIT_CORE)");
        return -1;
    }
    if ((unsigned long long)(rlim_t)number != number ||
        (limit.rlim_max != RLIM_INFINITY &&
         (rlim_t)number > limit.rlim_max)) {
        fprintf(stderr, "Размер core-файла превышает допустимый предел\n");
        return -1;
    }
    limit.rlim_cur = (rlim_t)number;
    if (setrlimit(RLIMIT_CORE, &limit) == -1) {
        perror("setrlimit(RLIMIT_CORE)");
        return -1;
    }
    printf("Размер core-файла установлен: %llu байт\n", number);
    return 0;
}

static int update_environment(const char *pair)
{
    const char *separator = strchr(pair, '=');
    char *name;
    size_t length;
    int result;

    if (separator == NULL || separator == pair) {
        fprintf(stderr, "Для -V требуется имя=значение: %s\n", pair);
        return -1;
    }
    length = (size_t)(separator - pair);
    name = malloc(length + 1);
    if (name == NULL) {
        perror("malloc");
        return -1;
    }
    memcpy(name, pair, length);
    name[length] = '\0';
    result = setenv(name, separator + 1, 1);
    if (result == -1)
        perror("setenv");
    else
        printf("Переменная %s установлена\n", name);
    free(name);
    return result;
}

static int execute_action(const struct action *current)
{
    char directory[4096];
    char **variable;

    switch (current->option) {
    case 'i':
        printf("UID: real=%lu effective=%lu\n",
               (unsigned long)getuid(), (unsigned long)geteuid());
        printf("GID: real=%lu effective=%lu\n",
               (unsigned long)getgid(), (unsigned long)getegid());
        break;
    case 's':
        if (setpgid(0, 0) == -1) {
            perror("setpgid");
            return -1;
        }
        printf("Процесс стал лидером группы: PGID=%ld\n", (long)getpgrp());
        break;
    case 'p':
        printf("PID=%ld PPID=%ld PGID=%ld\n",
               (long)getpid(), (long)getppid(), (long)getpgrp());
        break;
    case 'u':
        print_ulimit();
        break;
    case 'U':
        return change_ulimit(current->argument);
    case 'c':
        print_core_limit();
        break;
    case 'C':
        return change_core_limit(current->argument);
    case 'd':
        if (getcwd(directory, sizeof(directory)) == NULL) {
            perror("getcwd");
            return -1;
        }
        printf("Текущая директория: %s\n", directory);
        break;
    case 'v':
        for (variable = environ; *variable != NULL; ++variable)
            puts(*variable);
        break;
    case 'V':
        return update_environment(current->argument);
    default:
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    struct action *actions;
    int option;
    int count = 0;
    int index;
    int failed = 0;

    actions = calloc((size_t)argc, sizeof(*actions));
    if (actions == NULL) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    opterr = 0;
    while ((option = getopt(argc, argv, ":ispuU:cC:dvV:")) != -1) {
        if (option == ':' || option == '?') {
            if (option == ':')
                fprintf(stderr, "Отсутствует аргумент опции -%c\n", optopt);
            else
                fprintf(stderr, "Неизвестная опция -%c\n", optopt);
            usage(argv[0]);
            free(actions);
            return EXIT_FAILURE;
        }
        actions[count].option = option;
        actions[count].argument = optarg;
        ++count;
    }
    if (optind < argc) {
        fprintf(stderr, "Неизвестный позиционный аргумент: %s\n", argv[optind]);
        usage(argv[0]);
        free(actions);
        return EXIT_FAILURE;
    }

    /* По условию задания все опции исполняются СПРАВА НАЛЕВО. */
    for (index = count - 1; index >= 0; --index) {
        if (execute_action(&actions[index]) == -1)
            failed = 1;
    }
    free(actions);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

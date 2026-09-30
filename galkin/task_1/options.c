#include <stdio.h>
#include <stdlib.h>        // strtol, putenv
#include <unistd.h>        // getopt, getpid, getuid, getcwd
#include <ulimit.h>        // ulimit
#include <sys/resource.h>  // getrlimit, setrlimit (для core)
#include <string.h>        // strchr

#define MAXOPTS 100  // больше 100 опций не влезет в массивы

extern char **environ;  // все переменные среды, в конце NULL

// выполняет одну опцию, c - буква, arg - значение (если есть)
void do_option(int c, char *arg)
{
    char buf[1024];     // сюда getcwd кладет путь
    long newlim;
    char *end;          // где strtol остановилась
    struct rlimit rl;   // rlim_cur - текущий, rlim_max - потолок
    char **env;

    switch (c) {
    case 'i':  // -i: реальные и эффективные uid, gid
        printf("uid=%d euid=%d gid=%d egid=%d\n",
               (int)getuid(), (int)geteuid(),
               (int)getgid(), (int)getegid());
        break;
    case 's':  // -s: стать лидером группы (pgid = pid)
        if (setpgid(0, 0) == -1)
            perror("setpgid");
        else
            printf("процесс стал лидером группы\n");
        break;
    case 'p':  // -p: pid, pid родителя, группа
        printf("pid=%d ppid=%d pgid=%d\n",
               (int)getpid(), (int)getppid(), (int)getpgrp());
        break;
    case 'u':  // -u: печать ulimit (в блоках по 512 байт)
        printf("ulimit=%ld\n", ulimit(UL_GETFSIZE));
        break;
    case 'U':  // -U: поменять ulimit, поднять может только root
        newlim = strtol(arg, &end, 10);
        if (*end != '\0' || newlim < 0) {  // в строке мусор или минус
            printf("плохое значение для -U: %s\n", arg);
            break;
        }
        if (ulimit(UL_SETFSIZE, newlim) == -1)
            perror("ulimit");
        break;
    case 'c':  // -c: размер core-файла
        if (getrlimit(RLIMIT_CORE, &rl) == -1)
            perror("getrlimit");
        else if (rl.rlim_cur == RLIM_INFINITY)  // на солярисе это -3
            printf("core limit=unlimited\n");
        else
            printf("core limit=%ld\n", (long)rl.rlim_cur);
        break;
    case 'C':  // -C: поменять размер core
        newlim = strtol(arg, &end, 10);
        if (*end != '\0' || newlim < 0) {
            printf("плохое значение для -C: %s\n", arg);
            break;
        }
        if (getrlimit(RLIMIT_CORE, &rl) == -1) {  // сначала читаем, чтоб rlim_max не потерять
            perror("getrlimit");
            break;
        }
        rl.rlim_cur = newlim;  // меняем только текущий
        if (setrlimit(RLIMIT_CORE, &rl) == -1)
            perror("setrlimit");
        break;
    case 'd':  // -d: текущая папка
        if (getcwd(buf, sizeof(buf)) != NULL)
            printf("cwd=%s\n", buf);
        else
            perror("getcwd");
        break;
    case 'v':  // -v: все переменные среды, идем до NULL
        for (env = environ; *env != NULL; env++)
            printf("%s\n", *env);
        break;
    case 'V':  // -V: добавить/поменять переменную, формат имя=значение
        if (strchr(arg, '=') == NULL) {  // нет '=' значит неправильно
            printf("плохое значение для -V: %s (нужно имя=значение)\n", arg);
            break;
        }
        if (putenv(arg) != 0)
            perror("putenv");
        break;
    }
}

int main(int argc, char *argv[])
{
    int c;
    int n = 0;               // сколько опций записали
    int i;
    int opts[MAXOPTS];       // буквы опций
    char *args[MAXOPTS];     // их значения, у -i -p и тд тут NULL

    if (argc == 1) {  // запустили без аргументов
        printf("Использование: %s [-i] [-s] [-p] [-u] [-Uзначение] [-c] [-Cзначение] [-d] [-v] [-Vимя=значение]\n", argv[0]);
        return 0;
    }

    // сначала собираем все опции (getopt идет только слева направо)
    // двоеточие после буквы = у опции есть значение
    while ((c = getopt(argc, argv, "ispuU:cC:dvV:")) != -1) {
        if (c == '?') {  // буквы нет в списке
            printf("неизвестная опция\n");
            continue;
        }
        if (n >= MAXOPTS) {  // чтоб не вылезти за массив
            printf("слишком много опций\n");
            return 1;
        }
        opts[n] = c;
        args[n] = optarg;  // сохраняем сразу, getopt его перезапишет
        n++;
    }

    // потом выполняем с конца, по заданию справа налево
    for (i = n - 1; i >= 0; i--) {
        do_option(opts[i], args[i]);
    }
    return 0;
}

/* Лабораторная работа №18: аналог ls -ld — тип, права, число связей, владелец, группа, размер и дата для каждого аргумента. */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>

/* Заполняет строку вида "drwxr-xr-x":
   1 символ типа файла + 3 группы по 3 права. */
static void make_permissions(mode_t mode, char p[11]){
/* Первый символ — тип файла: d каталог, - обычный, l симлинк, ? прочее */
if (S_ISDIR(mode))
p[0] = 'd';
else if (S_ISREG(mode))
p[0] = '-';
else if (S_ISLNK(mode))
p[0] = 'l';
else
p[0] = '?';

/* Права владельца: бит установлен — буква, иначе прочерк */
p[1] = (mode & S_IRUSR) ? 'r' : '-';
p[2] = (mode & S_IWUSR) ? 'w' : '-';
p[3] = (mode & S_IXUSR) ? 'x' : '-';

/* Права группы */
p[4] = (mode & S_IRGRP) ? 'r' : '-';
p[5] = (mode & S_IWGRP) ? 'w' : '-';
p[6] = (mode & S_IXGRP) ? 'x' : '-';

/* Права всех остальных */
p[7] = (mode & S_IROTH) ? 'r' : '-';
p[8] = (mode & S_IWOTH) ? 'w' : '-';
p[9] = (mode & S_IXOTH) ? 'x' : '-';

p[10] = '\0';   // без нуля printf не сможет напечатать строку
}

int main(int argc, char *argv[]){
int errors = 0;   // станет 1, если хотя бы один аргумент не удалось обработать

/* Без аргументов печатаем подсказку и выходим с ошибкой */
if (argc < 2) {
    fprintf(stderr, "Usage: %s <file> [file ...]\n", argv[0]);
    return EXIT_FAILURE;
}

/* Обрабатываем каждый аргумент; ошибка одного не прерывает остальные */
for (int i = 1; i < argc; i++) {
    struct stat st;              // информация об inode
    char permissions[11];        // "drwxr-xr-x" + '\0'
    char date[32];               // строка с датой модификации
    char path_copy[PATH_MAX];    // копия пути для basename
    char *name;                  // только имя файла, без каталогов
    const char *owner_name = "?";   // запасные значения, если имени нет
    const char *group_name = "?";
    struct passwd *pw;
    struct group *gr;
    struct tm *tm_info;
    time_t now;

    /* lstat, а не stat: для симлинка берём информацию о самой ссылке */
    if (lstat(argv[i], &st) == -1) {
        perror(argv[i]);   // печатает причину ошибки с именем файла
        errors = 1;
        continue;
    }

    make_permissions(st.st_mode, permissions);

    /* UID/GID -> имена; если записи нет, остаётся "?" */
    pw = getpwuid(st.st_uid);
    if (pw != NULL)
        owner_name = pw->pw_name;

    gr = getgrgid(st.st_gid);
    if (gr != NULL)
        group_name = gr->gr_name;

    /* Дата: как в ls — для файлов старше ~6 месяцев вместо времени печатаем год */
    tm_info = localtime(&st.st_mtime);
    if (tm_info != NULL) {
        now = time(NULL);

        if (now != (time_t)-1 &&
            difftime(now, st.st_mtime) > 183.0 * 24 * 60 * 60) {
            strftime(date, sizeof(date), "%b %e  %Y", tm_info);
        } else {
            strftime(date, sizeof(date), "%b %e %H:%M", tm_info);
        }
    } else {
        strcpy(date, "?");
    }

    /* basename может изменить строку, поэтому работаем с копией пути */
    if (strlen(argv[i]) >= sizeof(path_copy)) {
        fprintf(stderr, "%s: path is too long\n", argv[i]);
        errors = 1;
        continue;
    }

    strcpy(path_copy, argv[i]);
    name = basename(path_copy);   // /tmp/a.txt -> a.txt

    /* Поля постоянной ширины, чтобы вывод был таблицей */
    printf("%-10s %3lu %-8s %-8s ",
           permissions,           // тип и права, например drwxr-xr-x
           (unsigned long)st.st_nlink,   // число жёстких ссылок
           owner_name,
           group_name);

    printf("%10lld ", (long long)st.st_size);   // размер в байтах

    printf("%-12s %s\n", date, name);
}

/* Ошибка хотя бы с одним файлом — ненулевой код возврата */
return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

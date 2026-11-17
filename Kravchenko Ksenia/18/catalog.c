/* Лабораторная работа №18. Листинг каталога - аналог команды ls -ld.
 *
 * Для каждого аргумента программа печатает строку вроде:
 *   drwxr-xr-x 2 user group 4096 17 19:02 dirname
 * т.е. тип файла и права доступа, число связей, владельца, группу,
 * размер, дату модификации и имя файла.
 * Все поля имеют постоянную ширину, чтобы вывод выглядел таблицей.
 */

#include <stdio.h>      /* printf, snprintf               */
#include <sys/stat.h>   /* stat, struct stat, S_ISDIR,
                           S_ISREG, S_IRUSR ... S_IXOTH   */
#include <pwd.h>        /* getpwuid, struct passwd        */
#include <grp.h>        /* getgrgid, struct group         */
#include <time.h>       /* localtime, strftime            */
#include <string.h>     /* (прямых вызовов в коде нет)    */
#include <libgen.h>     /* basename                       */

int main(int argc, char *argv[]) {
    /* Нужен хотя бы один аргумент - имя файла или каталога */
    if (argc < 2) {
        printf("Usage: %s <file1> [file2] ...\n", argv[0]);
        printf("Log: No arguments provided. Exiting.\n");
        return 1;
    }

    printf("Log: Starting processing of %d files.\n", argc - 1);

    /* Обрабатываем каждый аргумент командной строки по очереди */
    for (int i = 1; i < argc; i++) {
        char *filepath = argv[i];
        /* basename отбрасывает каталоги и оставляет только имя файла:
           /tmp/test/a.txt -> a.txt. ВНИМАНИЕ: basename может испортить
           исходную строку, но filepath - лишь копия указателя argv[i] */
        char *filename = basename(filepath);  // Get only the filename


        /* stat(2) заполняет структуру st информацией об inode файла
           (не открывая его). Не удалось - сообщаем и переходим к следующему */
        struct stat st;
        if (stat(filepath, &st) != 0) {
            printf("Log: Failed to stat %s. Skipping.\n", filepath);
            perror("stat");
            continue;
        }

        // Type
        /* Первый символ строки - тип файла. Макросы S_IS* разбирают
           поле st_mode: каталог -> 'd', обычный файл -> '-',
           всё остальное (устройство, сокет, FIFO) -> '?' */
        char type = '?';
        if (S_ISDIR(st.st_mode)) type = 'd';
        else if (S_ISREG(st.st_mode)) type = '-';

        // Permissions
        /* Девять символов прав доступа: три группы по три бита
           (владелец, группа, остальные). Бит установлен -> буква,
           иначе прочерк. Индексы 0..2 - user, 3..5 - group, 6..8 - other */
        char perm[10];
        perm[0] = (st.st_mode & S_IRUSR) ? 'r' : '-';
        perm[1] = (st.st_mode & S_IWUSR) ? 'w' : '-';
        perm[2] = (st.st_mode & S_IXUSR) ? 'x' : '-';
        perm[3] = (st.st_mode & S_IRGRP) ? 'r' : '-';
        perm[4] = (st.st_mode & S_IWGRP) ? 'w' : '-';
        perm[5] = (st.st_mode & S_IXGRP) ? 'x' : '-';
        perm[6] = (st.st_mode & S_IROTH) ? 'r' : '-';
        perm[7] = (st.st_mode & S_IWOTH) ? 'w' : '-';
        perm[8] = (st.st_mode & S_IXOTH) ? 'x' : '-';
        perm[9] = '\0';   // строка для printf должна быть завершена нулём

        // Owner and group
        /* Числовые UID/GID переводим в имена через базы пользователей
           и групп; если записи нет - печатаем "unknown" вместо падения */
        struct passwd *pwd = getpwuid(st.st_uid);
        struct group *grp = getgrgid(st.st_gid);
        char *owner = pwd ? pwd->pw_name : "unknown";
        char *group = grp ? grp->gr_name : "unknown";

        // Size (only for regular files)
        /* st_size - размер в байтах: для обычного файла это его длина,
           для каталога - размер самого каталога, а не его содержимого */
        char size_str[20] = "";
        snprintf(size_str, sizeof(size_str), "%ld", (long)st.st_size);

        // Modification time
        /* st_mtime - время последней модификации в секундах от эпохи.
           localtime переводит его в локальное время, strftime - в строку
           «день месяц часы:минуты» */
        char time_str[20];
        struct tm *tm = localtime(&st.st_mtime);
        strftime(time_str, sizeof(time_str), "%d %H:%M", tm);

        // Print in table format with fixed widths
        /* Формат: тип+права, число ссылок, владелец, группа, размер, время, имя.
           %-8s/%-5s - выравнивание влево по ширине поля, чтобы колонки
           не «плясали» при разной длине имён */
        printf("%c%s %ld %-8s %-8s %-5s %s %s\n",
               type, perm, (long)st.st_nlink, owner, group, size_str, time_str, filename);
    }

    printf("Log: All files processed.\n");
    return 0;
}

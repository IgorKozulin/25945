/* Лабораторная работа №32. Асинхронный ввод-вывод (при использовании сигналов).
 *
 * Клиент для сервера из задачи 32: подключается к файлу сокета my_socket
 * и перекачивает в него всё, что приходит на стандартный ввод.
 * Сервер при этом читает данные асинхронно (aio_read) и обслуживает
 * нескольких клиентов одновременно.
 */

#include <sys/socket.h>   /* socket, connect, AF_UNIX, SOCK_STREAM */
#include <sys/un.h>       /* struct sockaddr_un, sun_path         */
#include <stdio.h>        /* perror                               */
#include <stdlib.h>       /* exit                                 */
#include <unistd.h>       /* read, write, close                   */
#include <string.h>       /* memset, strncpy                      */

#define SOCKET_PATH "my_socket"   /* путь к файлу сокета сервера */
#define BUF_SIZE    1024          /* размер порции при перекачке  */

int main(void) {
    int fd;
    struct sockaddr_un addr;
    char buf[BUF_SIZE];
    ssize_t n;

    /* 1. Создаём локальный потоковый сокет */
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) { perror("socket"); exit(1); }

    /* 2. Адрес сервера: семейство AF_UNIX и путь к файлу сокета.
       memset обнуляет структуру, чтобы sun_path был завершён '\0',
       а -1 в длине оставляет под этот нуль место */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);

    /* 3. Устанавливаем соединение с сервером */
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        exit(1);
    }

    /* Перекачиваем всё из stdin в сокет */
    /* read возвращает 0 в конце ввода - тогда цикл заканчивается */
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        /* write в сокет может записать МЕНЬШЕ, чем попросили (например,
           если буфер сокета заполнен), поэтому сравниваем результат с n:
           несовпадение - ошибка, о которой сообщаем и выходим */
        if (write(fd, buf, n) != n) {
            perror("write");
            break;
        }
    }
    /* 4. Закрываем соединение: сервер увидит конец файла и освободит слот */
    close(fd);
    return 0;
}

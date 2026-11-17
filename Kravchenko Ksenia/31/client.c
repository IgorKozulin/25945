/* Лабораторная работа №31. Связь нескольких процессов через Unix domain socket.
 *
 * Клиент подключается к серверу (файл сокета my_socket) и передаёт ему
 * текст со стандартного ввода. Несколько таких клиентов могут работать
 * одновременно - сервер обслуживает их через select(2).
 *
 * Буфер здесь специально маленький (25 байт): текст приходит на сервер
 * мелкими порциями, и видно, что сервер собирает строки сам, а не
 * рассчитывает получить их целиком.
 */

#include <sys/socket.h>   /* socket, connect, AF_UNIX, SOCK_STREAM */
#include <sys/un.h>       /* struct sockaddr_un, sun_path         */
#include <stdio.h>        /* perror                               */
#include <stdlib.h>       /* exit                                 */
#include <unistd.h>       /* read, write, close                   */
#include <string.h>       /* memset, strncpy                      */
#include <errno.h>        /* errno (в этом файле не используется) */
#include <unistd.h>       /* (повторно, ничего нового не добавляет) */

#define SOCKET_PATH "my_socket"   /* путь к файлу сокета сервера */
#define BUFFER_SIZE 25            /* маленькая порция - «рваная» передача */

int main() {
    int client_fd;
    struct sockaddr_un addr;
    char buffer[BUFFER_SIZE];
    ssize_t num_read;

    // Создание сокета
    /* Локальный потоковый сокет - как у сервера, но без bind/listen:
       клиенту достаточно соединиться с уже существующим файлом сокета */
    client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("socket");
        exit(1);
    }

    // Подключение
    /* Заполняем адрес сервера: семейство и путь к файлу сокета.
       memset нужен, чтобы sun_path завершался '\0' */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    /* sizeof(...) - 1 - оставляем место под завершающий нуль */
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        exit(1);
    }

    // Чтение из stdin и отправка (неограниченный размер)
    /* Копируем стандартный ввод в сокет порциями по 25 байт: каждая
       итерация - один write, сервер получает данные кусками */
    while ((num_read = read(STDIN_FILENO, buffer, BUFFER_SIZE)) > 0) {
        write(client_fd, buffer, num_read);
    }

    /* 0 - конец ввода (Ctrl-D), -1 - ошибка чтения stdin */
    if (num_read == -1) {
        perror("read");
    }

    // Завершение
    /* Закрываем соединение: сервер увидит read == 0 и освободит слот */
    close(client_fd);
    return 0;
}

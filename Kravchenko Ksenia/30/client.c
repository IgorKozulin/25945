/* Лабораторная работа №30. Связь через Unix domain socket.
 *
 * Задание: два процесса общаются через локальный (Unix domain) сокет.
 * Клиент устанавливает соединение с сервером и передаёт ему текст;
 * сервер переводит его в верхний регистр и печатает у себя.
 * После разрыва соединения клиентом оба процесса завершаются.
 *
 * Этот файл - клиент. В отличие от сервера здесь нет bind/listen:
 * клиентская сторона сразу делает connect(2).
 */

#include <sys/socket.h>   /* socket, connect, AF_UNIX, SOCK_STREAM */
#include <sys/un.h>       /* struct sockaddr_un, sun_path         */
#include <stdio.h>        /* perror                               */
#include <stdlib.h>       /* exit                                 */
#include <unistd.h>       /* read, write, close                   */
#include <string.h>       /* memset, strncpy                      */
#include <errno.h>        /* errno (в этом файле не используется) */

/* То же имя, что и у сервера: путь к файлу сокета */
#define SOCKET_PATH "my_socket"
/* Размер порции, читаемой из stdin за один read(2) */
#define BUFFER_SIZE 1024

int main() {
    int client_fd;
    struct sockaddr_un addr;
    char buffer[BUFFER_SIZE];
    ssize_t num_read;

    // Создание сокета
    /* Сокет тот же, что у сервера: локальный потоковый. Пока он ни к чему
       не подключён - это просто дескриптор будущего соединения */
    client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("socket");
        exit(1);
    }

    // Подключение
    /* Адрес сервера - тот же файл сокета. memset обнуляет структуру,
       чтобы sun_path завершался '\0' */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    /* sizeof(...) - 1: оставляем байт под завершающий нуль */
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    /* connect ищет файл сокета по пути и устанавливает соединение.
       Если сервер не запущен, вернётся ошибка ENOENT/ECONNREFUSED */
    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        exit(1);
    }

    // Чтение из stdin и отправка (неограниченный размер)
    /* Читаем стандартный ввод порциями и пересылаем их в сокет.
       read возвращает 0 в конце файла (Ctrl-D) - это и есть сигнал
       «текст закончился» */
    while ((num_read = read(STDIN_FILENO, buffer, BUFFER_SIZE)) > 0) {
        write(client_fd, buffer, num_read);
    }

    /* 0 - нормальный конец ввода, -1 - ошибка чтения stdin */
    if (num_read == -1) {
        perror("read");
    }

    // Завершение
    /* close закрывает соединение. Сервер при этом получит read == 0
       (разрыв), выйдет из своего цикла и тоже завершится */
    close(client_fd);
    return 0;
}

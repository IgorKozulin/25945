/* Лабораторная работа №31. Связь нескольких процессов через Unix domain socket.
 *
 * Задание: сервер создаёт сокет и слушает его; каждому подключившемуся
 * клиенту он принимает текст, переводит его в верхний регистр и печатает
 * в свой стандартный вывод. Нужно обслуживать НЕСКОЛЬКО клиентов
 * параллельно, без задержек; преобразованный текст разных клиентов может
 * перемешиваться. Для мультиплексирования соединений используется
 * select(2).
 *
 * Идея: вместо fork на каждого клиента (как в задаче 30) все соединения
 * обслуживаются одним процессом. Массив clients[] хранит дескрипторы и
 * буферы недособранных строк, а select сообщает, на каких дескрипторах
 * уже есть данные - их и читаем, ни на ком не застревая.
 */

#include <sys/socket.h>   /* socket, bind, listen, accept, AF_UNIX, SOCK_STREAM */
#include <sys/un.h>       /* struct sockaddr_un                                */
#include <sys/select.h>   /* select, fd_set, FD_ZERO, FD_SET, FD_ISSET         */
#include <stdio.h>        /* printf, perror, putchar                           */
#include <stdlib.h>       /* exit                                              */
#include <unistd.h>       /* read, write, close, unlink                        */
#include <ctype.h>        /* toupper                                           */
#include <string.h>       /* memset, strncpy, memcpy, memchr, memmove          */
#include <errno.h>        /* errno, EINTR                                      */
#include <fcntl.h>        /* fcntl, F_SETFL, O_NONBLOCK                        */
#include <time.h>         /* clock_gettime, localtime, strftime                */

#define SOCKET_PATH "my_socket"   /* путь к файлу сокета                  */
#define BUFFER_SIZE 40960         /* размер буфера на одного клиента      */
#define MAX_CLIENTS 10            /* сколько клиентов обслуживаем сразу   */

/* Состояние одного клиента: дескриптор, накопительный буфер и его длина */
typedef struct {
    int fd;                       /* дескриптор соединения, -1 - слот свободен */
    char buffer[BUFFER_SIZE];     /* сюда копятся байты, пришедшие от клиента */
    size_t buf_len;               /* сколько байт уже накоплено                */
    int id;           // номер клиента для логов
} Client;

/* Таблица всех слотов; слот со значением fd == -1 считается свободным */
Client clients[MAX_CLIENTS];

/* Печать отметки времени вида [ЧЧ:ММ:СС.ммм] - чтобы в общем выводе было
   видно, в каком порядке и с какими задержками приходят данные клиентов */
void print_time() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);          /* секунды + наносекунды */
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", localtime(&ts.tv_sec));
    /* tv_nsec / 1 000 000 - перевод наносекунд в миллисекунды */
    printf("[%s.%03ld] ", time_buf, ts.tv_nsec / 1000000);
}

int main() {
    int server_fd, max_fd;
    struct sockaddr_un addr;
    fd_set readfds;               /* набор дескрипторов, готовых к чтению */
    char temp_buf[BUFFER_SIZE];   /* сюда читаем «сырые» порции данных    */
    ssize_t num_read;

    /* Все слоты помечаем свободными (fd == -1), номера задаём по порядку */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].buf_len = 0;
        clients[i].id = i + 1;
    }

    /* 1. Создаём локальный потоковый сокет */
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) { perror("socket"); exit(1); }

    /* 2. Заполняем адрес и связываем сокет с файлом my_socket.
       unlink убирает файл от прошлого запуска, иначе bind даст EADDRINUSE */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) { perror("bind"); exit(1); }
    /* 3. Переводим сокет в режим прослушивания (очередь до 5 соединений) */
    if (listen(server_fd, 5) == -1) { perror("listen"); exit(1); }

    /* 4. Неблокирующий режим: accept не будет усыплять процесс, когда
       новых соединений нет, - сразу вернёт -1 с errno == EAGAIN */
    fcntl(server_fd, F_SETFL, O_NONBLOCK);
    printf("Task 31 server (select + line buffering) running...\n");

    /* 5. Главный цикл мультиплексирования */
    while (1) {
        /* Перед каждым select набор надо собирать заново: select его меняет */
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);            /* ждём новых подключений */
        max_fd = server_fd;                     /* select нужен максимальный номер */

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                FD_SET(clients[i].fd, &readfds);        /* ждём данных от клиента */
                if (clients[i].fd > max_fd) max_fd = clients[i].fd;
            }
        }

        /* select усыпляет процесс, пока не появится готовый дескриптор.
           Второй-четвёртый аргументы NULL - запись и исключения не следим,
           последний NULL - ждём бесконечно (без тайм-аута) */
        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) == -1) {
            /* EINTR - прерывание сигналом, это не ошибка: повторяем цикл */
            if (errno == EINTR) continue;
            perror("select"); exit(1);
        }

        // Новое соединение
        /* FD_ISSET проверяет, отмечен ли дескриптор как готовый после select */
        if (FD_ISSET(server_fd, &readfds)) {
            int new_fd = accept(server_fd, NULL, NULL);
            if (new_fd != -1) {
                /* Ищем свободный слот под нового клиента */
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == -1) {
                        clients[i].fd = new_fd;
                        clients[i].buf_len = 0;      /* буфер пуст            */
                        /* Клиентский сокет тоже делаем неблокирующим,
                           чтобы read не завис на нём в основном цикле */
                        fcntl(new_fd, F_SETFL, O_NONBLOCK);
                        break;
                    }
                }
            }
        }

        // Обработка клиентов
        for (int i = 0; i < MAX_CLIENTS; i++) {
            /* Пропускаем свободные слоты и тех, у кого ничего не пришло */
            if (clients[i].fd == -1 || !FD_ISSET(clients[i].fd, &readfds)) continue;

            num_read = read(clients[i].fd, temp_buf, BUFFER_SIZE);
            if (num_read > 0) {
                /* Дописываем прочитанное в конец накопительного буфера:
                   в потоковом сокете одна строка может прийти по частям */
                memcpy(clients[i].buffer + clients[i].buf_len, temp_buf, num_read);
                clients[i].buf_len += num_read;

                /* Разбираем буфер по строкам: печатаем только завершённые
                   строки (до '\n'), а «хвост» без перевода строки оставляем
                   до следующего чтения */
                char *line_start = clients[i].buffer;
                char *newline;
                /* memchr ищет '\n' в остатке буфера; длина остатка -
                   buf_len минус смещение line_start от начала буфера */
                while ((newline = memchr(line_start, '\n', clients[i].buf_len - (line_start - clients[i].buffer))) != NULL) {
                    /* Длина строки вместе с самим '\n' */
                    size_t line_len = newline - line_start + 1;

                    /* Метка времени и номер клиента, затем строка в верхнем
                       регистре - печатаем посимвольно, чтобы не портить
                       буфер и не добавлять лишний '\n' */
                    print_time();
                    printf("[Client %d] ", clients[i].id);
                    for (size_t j = 0; j < line_len; j++) {
                        char c = toupper((unsigned char)line_start[j]);
                        putchar(c);
                    }

                    /* Сдвигаем остаток буфера (то, что после '\n') в его
                       начало: memmove корректен и для перекрывающихся
                       областей. Длина остатка - всё, что правее newline + 1 */
                    memmove(line_start, newline + 1, clients[i].buf_len - (newline + 1 - clients[i].buffer));
                    clients[i].buf_len -= line_len;
                    /* После сдвига остаток снова начинается с начала буфера,
                       поэтому поиск продолжаем оттуда же */
                    line_start = clients[i].buffer;
                }
            } else if (num_read == 0) {
                /* read вернул 0 - клиент закрыл соединение. Если в буфере
                   осталась недопечатанная строка без '\n', выводим и её */
                if (clients[i].buf_len > 0) {
                    print_time();
                    printf("[Client %d] ", clients[i].id);
                    for (size_t j = 0; j < clients[i].buf_len; j++) {
                        putchar(toupper((unsigned char)clients[i].buffer[j]));
                    }
                    putchar('\n');
                }
                /* Освобождаем слот: закрываем дескриптор и помечаем fd = -1,
                   чтобы select больше не ждал данных от этого клиента */
                close(clients[i].fd);
                clients[i].fd = -1;
            }
        }
    }
    return 0;
}

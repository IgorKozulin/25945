/* Лабораторная работа №32. Асинхронный ввод-вывод (при использовании сигналов).
 *
 * Задание: реализовать задачу 31 (несколько клиентов через Unix domain
 * socket), но вместо select(2)/poll(2) использовать асинхронный ввод-вывод.
 * Здесь взят POSIX AIO: aio_read(3) запускает чтение и сразу возвращает
 * управление, а результат потом забирается через aio_error/aio_return.
 * Ждать готовности дескрипторов не нужно - приложение само опрашивает
 * состояние операций, поэтому ни один клиент не блокирует остальных.
 *
 * Этот файл - сервер.
 */

#include <sys/types.h>    /* ssize_t, базовые типы                        */
#include <sys/socket.h>   /* socket, bind, listen, accept, AF_UNIX        */
#include <sys/un.h>       /* struct sockaddr_un                           */
#include <aio.h>          /* struct aiocb, aio_read, aio_error, aio_return, aio_cancel */
#include <fcntl.h>        /* fcntl, F_SETFL, O_NONBLOCK                   */
#include <stdio.h>        /* printf, perror                               */
#include <stdlib.h>       /* exit                                         */
#include <unistd.h>       /* close, unlink, usleep                        */
#include <ctype.h>        /* toupper                                      */
#include <string.h>       /* memset, strncpy                              */
#include <errno.h>        /* errno, EINPROGRESS, EAGAIN, EINTR            */
#include <signal.h>       /* signal, SIGINT, SIGTERM                      */
#include <time.h>         /* clock_gettime, localtime, strftime           */

#define SOCKET_PATH   "my_socket"   /* путь к файлу сокета           */
#define MAX_CLIENTS   20            /* сколько клиентов обслуживаем  */
#define READ_SIZE     1             /* читаем по одному байту за раз */

/* Один клиент: дескриптор, управляющий блок AIO, байт-приёмник и флаг */
struct client {
    int          fd;      /* дескриптор соединения, -1 - слот свободен  */
    struct aiocb cb;      /* блок управления асинхронной операцией      */
    char         byte;    /* куда aio_read положит прочитанный байт     */
    int          active;  /* 1 - операция запущена и клиент обслуживается */
    int          id;        // номер клиента
};

/* Таблица статическая: aiocb должен жить всё время, пока операция идёт */
static struct client clients[MAX_CLIENTS];

/* Отметка времени [ЧЧ:ММ:СС.ммм] перед каждой порцией вывода */
void print_time() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", localtime(&ts.tv_sec));
    printf("[%s.%03ld] ", time_buf, ts.tv_nsec / 1000000);   /* нс -> мс */
}

/* Обработчик SIGINT/SIGTERM: аккуратно свернуть работу.
   Отменяем незавершённые AIO-операции, закрываем сокеты, удаляем файл
   сокета - иначе после Ctrl-C он останется и следующий bind упадёт */
static void cleanup(int sig) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd != -1) {
            /* aio_cancel просит отменить операцию; NULL - для всех
               запросов на этом дескрипторе. Результат здесь не важен,
               так как процесс всё равно завершается */
            aio_cancel(clients[i].fd, NULL);
            close(clients[i].fd);
        }
    unlink(SOCKET_PATH);
    exit(0);
}

int main(void) {
    int listen_fd, new_fd;
    struct sockaddr_un addr;
    int slot;   /* индекс свободного слота под нового клиента */

    /* Перехватываем сигналы завершения, чтобы убрать файл сокета */
    signal(SIGINT,  cleanup);
    signal(SIGTERM, cleanup);

    /* 1. Сокет, привязка к файлу my_socket, режим прослушивания */
    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd == -1) { perror("socket"); exit(1); }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);
    unlink(SOCKET_PATH);   /* убрать файл от прошлого запуска */

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) { perror("bind"); exit(1); }
    if (listen(listen_fd, 10) == -1) { perror("listen"); exit(1); }

    /* 2. Неблокирующий accept: когда очередь пуста, вызов сразу
       возвращает -1 с errno == EAGAIN вместо сна */
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    printf("Task 32 server - POSIX AIO + CHAOTIC BYTE MIXING (with timestamps) running...\n");

    /* 3. Все слоты свободны */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].id = i + 1;
    }

    /* 4. Основной цикл: принимаем всех, кто пришёл, и проверяем,
       какие асинхронные чтения уже завершились */
    while (1) {
        /* Выгребаем ВСЕ ожидающие соединения: цикл крутится, пока accept
           возвращает дескриптор. -1 означает «очередь пуста» (EAGAIN) */
        while ((new_fd = accept(listen_fd, NULL, NULL)) != -1) {
            /* Ищем свободный слот; если все заняты - соединение отклоняем */
            for (slot = 0; slot < MAX_CLIENTS; slot++)
                if (clients[slot].fd == -1) break;
            if (slot == MAX_CLIENTS) { close(new_fd); continue; }

            clients[slot].fd     = new_fd;
            clients[slot].active = 1;

            /* Готовим блок управления: откуда читать (aio_fildes), куда
               положить байт (aio_buf) и сколько байт (aio_nbytes).
               memset обязателен - в структуре есть служебные поля */
            memset(&clients[slot].cb, 0, sizeof(struct aiocb));
            clients[slot].cb.aio_fildes = new_fd;
            clients[slot].cb.aio_buf    = &clients[slot].byte;
            clients[slot].cb.aio_nbytes = READ_SIZE;

            /* Запуск асинхронного чтения: возвращает управление сразу,
               не дожидаясь данных. Ошибка - отменяем подключение */
            if (aio_read(&clients[slot].cb) == -1) {
                perror("aio_read");
                close(new_fd);
                clients[slot].fd = -1;
                clients[slot].active = 0;
            }
        }

        /* Проверяем, что успело завершиться у уже подключённых клиентов */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) continue;

            /* aio_error возвращает EINPROGRESS, пока операция идёт,
               0 - при успехе, код ошибки - при сбое */
            int err = aio_error(&clients[i].cb);
            if (err == EINPROGRESS) continue;

            /* Результат операции: сколько байт прочитано,
               0 - конец файла (клиент закрыл соединение), -1 - ошибка */
            ssize_t ret = aio_return(&clients[i].cb);

            if (ret == 1) {
                /* Пришёл один байт: переводим в верхний регистр и печатаем.
                   Вывод по байту - из-за READ_SIZE = 1 */
                char c = toupper((unsigned char)clients[i].byte);
                printf("%c", c);

                /* Сразу запускаем следующее чтение для этого же клиента.
                   Если оно не удалось (кроме EAGAIN/EINTR) - закрываем */
                if (aio_read(&clients[i].cb) == -1 && errno != EAGAIN && errno != EINTR) {
                    if(errno != EAGAIN && errno != EINTR){
                        close(clients[i].fd);
                        clients[i].fd = -1;
                        clients[i].active = 0;
                    }

                }
            } else {
                /* ret == 0 - клиент закрыл соединение, ret < 0 - ошибка:
                   в обоих случаях освобождаем слот */
                close(clients[i].fd);
                clients[i].fd = -1;
                clients[i].active = 0;
            }
        }
        /* Небольшая пауза, чтобы опрос состояний не занимал процессор
           на 100 %: 1 мс - компромисс между отзывчивостью и нагрузкой */
        usleep(1000);
    }
    return 0;
}

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUF_SIZE 1024

int main(void) {
  int pipefd[2];
  pid_t pid;

  if (pipe(pipefd) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  pid = fork();
  if (pid == -1) {
    perror("fork");
    close(pipefd[0]);
    close(pipefd[1]);
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {
    close(pipefd[1]);

    printf("[CHILD] Запущен. Ожидаю данные от родителя...\n");
    fflush(stdout);

    char buffer[BUF_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes_read] = '\0';

      for (ssize_t i = 0; i < bytes_read; i++) {
        buffer[i] = (char)toupper((unsigned char)buffer[i]);
      }

      printf("[CHILD] Получено и преобразовано: %s", buffer);
      fflush(stdout);
    }

    if (bytes_read == -1) {
      perror("read");
      close(pipefd[0]);
      exit(EXIT_FAILURE);
    }

    close(pipefd[0]);
    exit(EXIT_SUCCESS);

  } else {
    close(pipefd[0]);

    printf("[PARENT] Связь через канал установлена!\n");
    printf("[PARENT] Введите текст (пустая строка или Ctrl+D — выход):\n");

    char input[BUF_SIZE];

    while (1) {
      printf("> ");
      fflush(stdout);

      if (fgets(input, sizeof(input), stdin) == NULL) {
        printf("\n[PARENT] Получен EOF (Ctrl+D) — завершаю ввод.\n");
        break;
      }

      if (input[0] == '\n') {
        printf("[PARENT] Пустая строка — завершаю ввод.\n");
        break;
      }

      size_t len = strlen(input);
      if (write(pipefd[1], input, len) == -1) {
        perror("write");
        break;
      }
    }

    close(pipefd[1]);

    wait(NULL);
    printf("[PARENT] Потомок завершился. Родитель тоже завершает работу.\n");
  }

  return EXIT_SUCCESS;
}

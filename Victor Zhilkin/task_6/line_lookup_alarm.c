#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
  long offset;
  int length;
} LineInfo;

int global_fd = -1;

void alarm_handler(int sig) {
  (void)sig;
  char buffer[4096];
  ssize_t bytes;

  printf("\n[TIMEOUT] Time is up! Printing full file content...\n");

  if (lseek(global_fd, 0L, SEEK_SET) == -1) {
    _exit(EXIT_FAILURE);
  }

  while ((bytes = read(global_fd, buffer, sizeof(buffer))) > 0) {
    if (write(STDOUT_FILENO, buffer, bytes) != bytes) {
      _exit(EXIT_FAILURE);
    }
  }

  _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Использование: %s <имя_файла>\n", argv[0]);
    return EXIT_FAILURE;
  }

  global_fd = open(argv[1], O_RDONLY);

  int capacity = 10;
  int num_lines = 0;
  LineInfo *table = (LineInfo *)malloc(capacity * sizeof(LineInfo));

  char ch;
  long line_start = 0;
  int current_line_len = 0;

  while (read(global_fd, &ch, 1) > 0) {
    current_line_len++;

    if (ch == '\n') {
      if (num_lines >= capacity) {
        capacity *= 2;
        LineInfo *temp =
            (LineInfo *)realloc(table, capacity * sizeof(LineInfo));
        if (temp == NULL) {
          perror("Ошибка перераспределения памяти");
          free(table);
          close(global_fd);
          return EXIT_FAILURE;
        }
        table = temp;
      }

      table[num_lines].offset = line_start;
      table[num_lines].length = current_line_len;
      num_lines++;

      line_start = lseek(global_fd, 0L, SEEK_CUR);
      current_line_len = 0;
    }
  }

  if (current_line_len > 0) {
    if (num_lines >= capacity) {
      capacity += 1;
      LineInfo *temp = (LineInfo *)realloc(table, capacity * sizeof(LineInfo));
      if (temp == NULL) {
        perror("Ошибка перераспределения памяти");
        free(table);
        close(global_fd);
        return EXIT_FAILURE;
      }
      table = temp;
    }
    table[num_lines].offset = line_start;
    table[num_lines].length = current_line_len;
    num_lines++;
  }

  printf("--- Debug: Line Table ---\n");
  for (int i = 0; i < num_lines; i++) {
    printf("Line %d: Offset = %ld, Length = %d\n", i + 1, table[i].offset,
           table[i].length);
  }
  printf("-------------------------\n");

  if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
    perror("Ошибка установки обработчика сигнала");
    free(table);
    close(global_fd);
    return EXIT_FAILURE;
  }

  int target_line;
  while (1) {
    printf("Enter line number (0 to quit, 5 sec timeout): ");
    fflush(stdout);

    alarm(5);

    if (scanf("%d", &target_line) != 1) {
      alarm(0);
      while (getchar() != '\n')
        ;
      printf("Invalid input. Please enter a number.\n");
      continue;
    }

    alarm(0);

    if (target_line == 0) {
      break;
    }

    if (target_line < 1 || target_line > num_lines) {
      printf("Error: Line number out of range (1..%d).\n", num_lines);
      continue;
    }

    long offset = table[target_line - 1].offset;
    int length = table[target_line - 1].length;

    if (lseek(global_fd, offset, SEEK_SET) == -1) {
      perror("Ошибка позиционирования в файле");
      continue;
    }

    char *line_buf = (char *)malloc(length + 1);
    if (line_buf == NULL) {
      perror("Ошибка выделения памяти для буфера строки");
      continue;
    }

    ssize_t bytes_read = read(global_fd, line_buf, length);
    if (bytes_read == -1) {
      perror("Ошибка чтения строки");
      free(line_buf);
      continue;
    }

    line_buf[bytes_read] = '\0';
    printf("%s", line_buf);

    if (bytes_read > 0 && line_buf[bytes_read - 1] != '\n') {
      printf("\n");
    }

    free(line_buf);
  }

  free(table);
  close(global_fd);
  printf("Program finished.\n");

  return EXIT_SUCCESS;
}

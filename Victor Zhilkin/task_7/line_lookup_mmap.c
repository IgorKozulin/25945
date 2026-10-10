#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
  long offset;
  int length;
} LineInfo;

char *global_map = MAP_FAILED;
size_t global_size = 0;

void alarm_handler(int sig) {
  (void)sig;
  printf("\n[TIMEOUT] Time is up! Printing full file content...\n");
  if (global_map != MAP_FAILED && global_size > 0) {
    if (write(STDOUT_FILENO, global_map, global_size) != (ssize_t)global_size) {
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

  int fd = open(argv[1], O_RDONLY);

  struct stat st;
  if (fstat(fd, &st) == -1) {
    perror("Ошибка fstat");
    close(fd);
    return EXIT_FAILURE;
  }

  if (st.st_size == 0) {
    printf("File is empty.\n");
    close(fd);
    return EXIT_SUCCESS;
  }

  global_size = st.st_size;
  global_map = (char *)mmap(NULL, global_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);

  if (global_map == MAP_FAILED) {
    perror("Ошибка mmap");
    return EXIT_FAILURE;
  }

  int capacity = 10;
  int num_lines = 0;
  LineInfo *table = (LineInfo *)malloc(capacity * sizeof(LineInfo));
  if (table == NULL) {
    perror("Ошибка выделения памяти для таблицы");
    munmap(global_map, global_size);
    return EXIT_FAILURE;
  }

  long line_start = 0;
  int current_line_len = 0;

  for (size_t i = 0; i < global_size; i++) {
    current_line_len++;

    if (global_map[i] == '\n') {
      if (num_lines >= capacity) {
        capacity *= 2;
        LineInfo *temp =
            (LineInfo *)realloc(table, capacity * sizeof(LineInfo));
        if (temp == NULL) {
          perror("Ошибка перераспределения памяти");
          free(table);
          munmap(global_map, global_size);
          return EXIT_FAILURE;
        }
        table = temp;
      }

      table[num_lines].offset = line_start;
      table[num_lines].length = current_line_len;
      num_lines++;

      line_start = i + 1;
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
        munmap(global_map, global_size);
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
    munmap(global_map, global_size);
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

    fwrite(global_map + offset, 1, length, stdout);

    if (length > 0 && global_map[offset + length - 1] != '\n') {
      printf("\n");
    }
  }

  free(table);
  munmap(global_map, global_size);
  printf("Program finished.\n");

  return EXIT_SUCCESS;
}

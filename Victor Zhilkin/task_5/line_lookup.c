#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
  long offset;
  int length;
} LineInfo;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Использование: %s <имя_файла>\n", argv[0]);
    return EXIT_FAILURE;
  }

  int fd = open(argv[1], O_RDONLY);
  int capacity = 10;
  int num_lines = 0;
  LineInfo *table = (LineInfo *)malloc(capacity * sizeof(LineInfo));

  char ch;
  long line_start = 0;
  int current_line_len = 0;

  while (read(fd, &ch, 1) > 0) {
    current_line_len++;

    if (ch == '\n') {
      if (num_lines >= capacity) {
        capacity *= 2;
        LineInfo *temp = (LineInfo *)realloc(table, capacity * sizeof(LineInfo));
        table = temp;
      }

      table[num_lines].offset = line_start;
      table[num_lines].length = current_line_len;
      num_lines++;

      line_start = lseek(fd, 0L, SEEK_CUR);
      current_line_len = 0;
    }
  }

  if (current_line_len > 0) {
    if (num_lines >= capacity) {
      capacity += 1;
      LineInfo *temp = (LineInfo *)realloc(table, capacity * sizeof(LineInfo));
      table = temp;
    }
    table[num_lines].offset = line_start;
    table[num_lines].length = current_line_len;
    num_lines++;
  }

  printf("--- Debug: Line Table ---\n");
  for (int i = 0; i < num_lines; i++) {
    printf("Line %d: Offset = %ld, Length = %d\n", i + 1, table[i].offset, table[i].length);
  }
  printf("-------------------------\n");

  long total_bytes = 0;
  for (int i = 0; i < num_lines; i++) {
    total_bytes += table[i].length;
  }
  printf("по таблице: %ld байт\n", total_bytes);

  long real_size = lseek(fd, 0L, SEEK_END);
  printf("lseek: %ld байт\n", real_size);
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "ls -lh \"%s\"", argv[1]);
  system(cmd);

  int target_line;
  while (1) {
    printf("Enter line number (0 to quit): ");
    if (scanf("%d", &target_line) != 1) {
      while (getchar() != '\n');
      printf("Invalid input. Please enter a number.\n");
      continue;
    }

    if (target_line == 0) {
      break;
    }

    if (target_line < 1 || target_line > num_lines) {
      printf("Error: Line number out of range (1..%d).\n", num_lines);
      continue;
    }

    long offset = table[target_line - 1].offset;
    int length = table[target_line - 1].length;
    lseek(fd, offset, SEEK_SET);
    char *line_buf = (char *)malloc(length + 1);
    ssize_t bytes_read = read(fd, line_buf, length);

    line_buf[bytes_read] = '\0';
    printf("%s", line_buf);

    if (bytes_read > 0 && line_buf[bytes_read - 1] != '\n') {
      printf("\n");
    }

    free(line_buf);
  }

  free(table);
  close(fd);
  printf("Program finished.\n");

  return EXIT_SUCCESS;
}
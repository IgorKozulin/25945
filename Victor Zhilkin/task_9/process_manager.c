
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Использование: %s <имя_файла>\n", argv[0]);
    return EXIT_FAILURE;
  }

  int num_children = 0;
  printf("Сколько дочерних процессов создать? ");
  if (scanf("%d", &num_children) != 1 || num_children <= 0) {
    fprintf(stderr, "Ошибка: введено неверное количество процессов.\n");
    return EXIT_FAILURE;
  }

  printf("[PARENT] Starting program. My PID: %d\n", getpid());

  pid_t *pids = malloc(num_children * sizeof(pid_t));
  if (pids == NULL) {
    perror("Ошибка выделения памяти для массива pids");
    return EXIT_FAILURE;
  }

  for (int i = 0; i < num_children; i++) {
    pid_t pid = fork();

    if (pid < 0) {
      perror("[PARENT] Fork failed");
      free(pids);
      return EXIT_FAILURE;
    }

    if (pid == 0) {
      printf("[CHILD %d] PID: %d, Parent PID: %d\n", i + 1, getpid(),
             getppid());

      if (i == 0) {
        execlp("cat", "cat", argv[1], (char *)NULL);

        perror("[CHILD 1] execlp failed");
        exit(EXIT_FAILURE);
      } else {
        exit(EXIT_SUCCESS);
      }
    } else {
      pids[i] = pid;
    }
  }

  printf("[PARENT] Все дети запущены. Ожидаю завершения всех процессов из "
         "массива...\n");

  for (int i = 0; i < num_children; i++) {
    int status;
    if (waitpid(pids[i], &status, 0) == -1) {
      perror("[PARENT] waitpid failed");
    } else {
      if (WIFEXITED(status)) {
        printf("[PARENT] CHILD %d (PID %d) завершился с кодом: %d\n", i + 1,
               pids[i], WEXITSTATUS(status));
      } else {
        printf("[PARENT] CHILD %d (PID %d) завершился аварийно\n", i + 1,
               pids[i]);
      }
    }
  }

  printf("[PARENT] Все процессы из массива успешно завершили работу. Выход.\n");

  free(pids);
  return EXIT_SUCCESS;
}

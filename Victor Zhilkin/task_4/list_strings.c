#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
  char *data;
  struct Node *next;
} Node;

void append(Node **head, const char *str) {
  Node *new_node = (Node *)malloc(sizeof(Node));
  if (new_node == NULL) {
    perror("Ошибка выделения памяти для узла");
    exit(EXIT_FAILURE);
  }

  new_node->data = (char *)malloc(strlen(str) + 1);
  if (new_node->data == NULL) {
    perror("Ошибка выделения памяти для строки");
    free(new_node);
    exit(EXIT_FAILURE);
  }

  strcpy(new_node->data, str);
  new_node->next = NULL;

  if (*head == NULL) {
    *head = new_node;
  } else {
    Node *current = *head;
    while (current->next != NULL) {
      current = current->next;
    }
    current->next = new_node;
  }
}

void print_list(const Node *head) {
  const Node *current = head;
  while (current != NULL) {
    printf("%s\n", current->data);
    current = current->next;
  }
}

void free_list(Node *head) {
  Node *current = head;
  while (current != NULL) {
    Node *next_node = current->next;
    free(current->data);
    free(current);
    current = next_node;
  }
}

int main(void) {
  Node *head = NULL;
  char buffer[1024];

  printf("Вводите строки:\n");

  while (1) {
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
      break;
    }

    if (buffer[0] == '.') {
      break;
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
    }

    append(&head, buffer);
  }

  printf("\nСодержимое списка:\n");
  print_list(head);

  free_list(head);

  return 0;
}

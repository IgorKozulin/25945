#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
  char *data;
  struct Node *next;
} Node;

void append(Node **head, const char *str) {
  Node *new_node = (Node *)malloc(sizeof(Node));
  new_node->data = (char *)malloc(strlen(str) + 1);

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

void reverse_list(Node **head) {
  Node *prev = NULL;
  Node *current = *head;
  Node *next = NULL;

  while (current != NULL) {
    next = current->next;
    current->next = prev;
    prev = current;
    current = next;
  }

  *head = prev;
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
    fgets(buffer, sizeof(buffer), stdin);

    if (buffer[0] == '.') {
      break;
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
    }

    append(&head, buffer);
  }
  reverse_list(&head);

  printf("\nСодержимое списка:\n");
  print_list(head);

  free_list(head);

  return 0;
}

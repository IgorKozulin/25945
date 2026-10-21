#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Node {
    char *data;
    struct Node *next;
};

/* Append a new string to the end of the list */
void append(struct Node **head, const char *str) {
    struct Node *new_node = malloc(sizeof(struct Node));
    if (new_node == NULL) { perror("malloc"); return; }

    new_node->data = malloc(strlen(str) + 1);
    if (new_node->data == NULL) { perror("malloc"); free(new_node); return; }
    strcpy(new_node->data, str);
    new_node->next = NULL;

    if (*head == NULL) { *head = new_node; return; }

    struct Node *cur = *head;
    while (cur->next != NULL) cur = cur->next;
    cur->next = new_node;
}

/* Print every node */
void print_all(struct Node *head) {
    while (head != NULL) {
        printf("%s\n", head->data);
        head = head->next;
    }
}

/* Print every second node (1st, 3rd, 5th, ...) */
void print_every_second(struct Node *head) {
    int index = 0;
    while (head != NULL) {
        if (index % 2 == 1) {
            printf("%s\n", head->data);
        }
        head = head->next;
        index++;
    }
}

/* Free all memory */
void free_list(struct Node *head) {
    while (head != NULL) {
        struct Node *next = head->next;
        free(head->data);
        free(head);
        head = next;
    }
}

int main(int argc, char *argv[]) {
    int every_second = 0;
    if (argc > 1 && strcmp(argv[1], "-e") == 0) every_second = 1;

    char buffer[1024];
    struct Node *head = NULL;

    printf("Enter strings (end with '.'):\n");
    while (1) {
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') buffer[len - 1] = '\0';
        if (buffer[0] == '.') break;
        if (strlen(buffer) > 0) append(&head, buffer);
    }

    printf("\n--- List contents ---\n");
    if (every_second) print_every_second(head);
    else              print_all(head);

    free_list(head);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024

typedef struct Node {
    char *str;
    struct Node *next;
} Node;

int main(void)
{
    char buffer[MAX_LINE];

    Node *head = NULL;
    Node *tail = NULL;

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        if (buffer[0] == '.') {
            break;
        }

        size_t len = strlen(buffer);

        Node *node = malloc(sizeof(Node));
        if (node == NULL) {
            perror("malloc");
            return 1;
        }

        node->str = malloc(len + 1);
        if (node->str == NULL) {
            perror("malloc");
            free(node);
            return 1;
        }

        memcpy(node->str, buffer, len + 1);
        node->next = NULL;

        if (head == NULL) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail = node;
        }
    }

    printf("\nСписок строк:\n");

    Node *current = head;

    while (current != NULL) {
        printf("%s", current->str);
        current = current->next;
    }

    current = head;

    while (current != NULL) {
        Node *next = current->next;

        free(current->str);
        free(current);

        current = next;
    }

    return 0;
}

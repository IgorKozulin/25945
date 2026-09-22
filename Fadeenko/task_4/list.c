#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    struct Node *next;
    char text[];
} Node;

int main(void)
{
    char buffer[1024];
    Node *head = NULL;
    Node **tail = &head;
    Node *node;
    int result = EXIT_SUCCESS;

    while (fgets(buffer, sizeof(buffer), stdin) != NULL && buffer[0] != '.') {
        size_t length = strlen(buffer);

        node = malloc(sizeof(*node) + length + 1);
        if (node == NULL) {
            perror("malloc");
            result = EXIT_FAILURE;
            break;
        }

        node->next = NULL;
        memcpy(node->text, buffer, length + 1);
        *tail = node;
        tail = &node->next;
    }

    while (head != NULL) {
        node = head;
        fputs(node->text, stdout);
        head = head->next;
        free(node);
    }

    return result;
}

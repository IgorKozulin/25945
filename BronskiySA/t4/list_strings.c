#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct node { char *text; struct node *next; };
int main(void) {
    struct node *head = NULL, **tail = &head;
    char *line = NULL; size_t capacity = 0; int result = 0;
    for (;;) {
        ssize_t length = getline(&line, &capacity, stdin);
        if (length < 0) { if (!feof(stdin)) { perror("getline"); result = 1; } break; }
        if (line[0] == '.') break;
        struct node *item = malloc(sizeof *item);
        if (!item) { perror("malloc"); result = 1; break; }
        item->text = malloc((size_t)length + 1);
        if (!item->text) { free(item); perror("malloc"); result = 1; break; }
        memcpy(item->text, line, (size_t)length + 1); item->next = NULL;
        *tail = item; tail = &item->next;
    }
    free(line);
    while (head) {
        struct node *next = head->next;
        if (!result && fputs(head->text, stdout) == EOF) result = 1;
        free(head->text); free(head); head = next;
    }
    return result;
}

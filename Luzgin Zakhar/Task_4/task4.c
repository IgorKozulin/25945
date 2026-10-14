#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node {
    char *text;
    struct node *next;
};

/* fgets читает порциями: строка может быть длиннее размера буфера. */
static char *read_line(void)
{
    char buffer[1024];
    char *result = NULL;
    size_t used = 0;

    for (;;) {
        size_t part;
        char *temporary;
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            if (ferror(stdin)) {
                free(result);
                return NULL;
            }
            return result; /* EOF: последняя строка может не иметь '\n' */
        }
        part = strlen(buffer);
        if (part > (size_t)-1 - used - 1) {
            free(result);
            return NULL;
        }
        temporary = realloc(result, used + part + 1);
        if (temporary == NULL) {
            free(result);
            return NULL;
        }
        result = temporary;
        memcpy(result + used, buffer, part + 1);
        used += part;
        if (used > 0 && result[used - 1] == '\n')
            return result;
    }
}

static void free_list(struct node *head)
{
    while (head != NULL) {
        struct node *next = head->next;
        free(head->text);
        free(head);
        head = next;
    }
}

int main(void)
{
    struct node *head = NULL;
    struct node *tail = NULL;
    struct node *current;
    char *line;

    puts("Введите строки. Точка в начале строки завершает ввод:");
    while ((line = read_line()) != NULL) {
        if (line[0] == '.') {
            free(line);
            break;
        }
        current = malloc(sizeof(*current));
        if (current == NULL) {
            perror("malloc");
            free(line);
            free_list(head);
            return EXIT_FAILURE;
        }
        current->text = line;
        current->next = NULL;
        if (tail == NULL)
            head = current;
        else
            tail->next = current;
        tail = current;
    }
    if (ferror(stdin)) {
        perror("fgets");
        free_list(head);
        return EXIT_FAILURE;
    }

    puts("\nСтроки в списке:");
    for (current = head; current != NULL; current = current->next) {
        fputs(current->text, stdout);
        if (strchr(current->text, '\n') == NULL)
            putchar('\n');
    }
    free_list(head);
    return EXIT_SUCCESS;
}

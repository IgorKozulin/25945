#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLEN 1024

struct node {
    char *str;
    struct node *next;
};

int main()
{
    char buf[MAXLEN];
    struct node *head = NULL;
    struct node *tail = NULL;    // держим хвост, чтоб добавлять в конец без прохода по списку
    struct node *p;
    struct node *tmp;

    printf("Вводите строки, точка в начале строки - конец ввода\n");

    while (fgets(buf, MAXLEN, stdin) != NULL) {   // NULL при конце ввода (ctrl+D)
        if (buf[0] == '.')
            break;

        p = malloc(sizeof(struct node));
        if (p == NULL) {
            perror("malloc");
            return 1;
        }
        p->str = malloc(strlen(buf) + 1);  // +1 под '\0', strlen его не считает
        if (p->str == NULL) {
            perror("malloc");
            return 1;
        }
        strcpy(p->str, buf);   // копия обязательна, buf перезапишется на след. круге
        p->next = NULL;

        if (head == NULL)
            head = p;
        else
            tail->next = p;
        tail = p;
    }

    printf("Введенные строки:\n");
    for (p = head; p != NULL; p = p->next)
        printf("%s", p->str);   // \n не нужен, fgets его оставляет в строке

    p = head;
    while (p != NULL) {
        tmp = p->next;   // после free(p) к p->next лезть нельзя
        free(p->str);
        free(p);
        p = tmp;
    }
    return 0;
}

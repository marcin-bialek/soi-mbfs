#include <stdlib.h>
#include "list.h"


inline List listMake(void) {
    List list;
    list.head = NULL;
    list.tail = NULL;
    list.size = 0;
    return list;
}


void listDestroy(List* list) {
    for(ListNode* p = list->head, *n; p; p = n) {
        n = p->next;
        free(p);
    }
}


void listAppend(List* list, void* value) {
    ListNode* node = malloc(sizeof(ListNode));
    node->value = value;
    node->next = NULL;

    if(list->tail == NULL) {
        list->head = node;
        list->tail = node;
        list->size = 1;
        return;
    }

    list->tail->next = node;
    list->tail = node;
    list->size++;
    return;
}




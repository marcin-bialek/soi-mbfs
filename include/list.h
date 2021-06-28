#pragma once
#include <inttypes.h>
#include <stdbool.h>


typedef struct ListNode {
    void* value;
    struct ListNode* next;
} ListNode;


typedef struct {
    ListNode* head;
    ListNode* tail;
    uint64_t size;
} List;


List listMake(void);
void listDestroy(List* list);
void listAppend(List* list, void* value);

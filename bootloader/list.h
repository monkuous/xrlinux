#pragma once

#include "compiler.h"

struct BlListNode {
    struct BlListNode *Prev;
    struct BlListNode *Next;
};

struct BlList {
    struct BlListNode *Head;
    struct BlListNode *Tail;
};

#define BL_LIST_HEAD(type, name, list) BL_CONTAINER(type, name, (list).Head)
#define BL_LIST_TAIL(type, name, list) BL_CONTAINER(type, name, (list).Tail)
#define BL_LIST_PREV(type, name, value) BL_CONTAINER(type, name, (value).name.Prev)
#define BL_LIST_NEXT(type, name, value) BL_CONTAINER(type, name, (value).name.Next)

#define BL_LIST_FOREACH(list, type, name, var) \
    for (type *var = BL_LIST_HEAD(type, name, (list)); var != nullptr; var = BL_LIST_NEXT(type, name, *var))

static inline void BlListInit(struct BlList *list) {
    list->Head = nullptr;
    list->Tail = nullptr;
}

static inline void BlListInsertBefore(struct BlList *list, struct BlListNode *anchor, struct BlListNode *node) {
    node->Next = anchor;

    if (anchor) {
        node->Prev = anchor->Prev;
        anchor->Prev = node;
    } else {
        node->Prev = list->Tail;
        list->Tail = node;
    }

    if (node->Prev) node->Prev->Next = node;
    else list->Head = node;
}

static inline void BlListInsertAfter(struct BlList *list, struct BlListNode *anchor, struct BlListNode *node) {
    node->Prev = anchor;

    if (anchor) {
        node->Next = anchor->Next;
        anchor->Next = node;
    } else {
        node->Next = list->Head;
        list->Head = node;
    }

    if (node->Next) node->Next->Prev = node;
    else list->Tail = node;
}

static inline void BlListRemove(struct BlList *list, struct BlListNode *node) {
    if (node->Prev) node->Prev->Next = node->Next;
    else list->Head = node->Next;

    if (node->Next) node->Next->Prev = node->Prev;
    else list->Tail = node->Prev;
}

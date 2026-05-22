#ifndef MYOS_KERNEL_LIST_H
#define MYOS_KERNEL_LIST_H

#include <stddef.h>

typedef struct list_node {
    struct list_node *next;
    struct list_node *prev;
} list_node_t;

typedef struct list {
    list_node_t *head;
    list_node_t *tail;
} list_t;

#define list_entry(node, type, member) \
    ((type *)((char *)(node) - offsetof(type, member)))

#define list_for_each(pos, list) \
    for ((pos) = (list)->head; (pos) != NULL; (pos) = (pos)->next)

#define list_for_each_safe(pos, tmp, list) \
    for ((pos) = (list)->head, (tmp) = ((pos) ? (pos)->next : NULL); \
         (pos) != NULL; \
         (pos) = (tmp), (tmp) = ((pos) ? (pos)->next : NULL))

void list_init(list_t *list);
void list_node_init(list_node_t *node);
int list_is_empty(const list_t *list);
void list_push_back(list_t *list, list_node_t *node);
list_node_t *list_pop_front(list_t *list);
void list_remove(list_t *list, list_node_t *node);

#endif /* MYOS_KERNEL_LIST_H */

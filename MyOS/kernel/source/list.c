#include "list.h"

void list_init(list_t *list)
{
    if (list == NULL) {
        return;
    }

    list->head = NULL;
    list->tail = NULL;
}

void list_node_init(list_node_t *node)
{
    if (node == NULL) {
        return;
    }

    node->next = NULL;
    node->prev = NULL;
}

int list_is_empty(const list_t *list)
{
    return (list == NULL || list->head == NULL);
}

void list_push_back(list_t *list, list_node_t *node)
{
    if (list == NULL || node == NULL) {
        return;
    }

    node->next = NULL;
    node->prev = list->tail;

    if (list->tail != NULL) {
        list->tail->next = node;
    } else {
        list->head = node;
    }

    list->tail = node;
}

list_node_t *list_pop_front(list_t *list)
{
    if (list == NULL || list->head == NULL) {
        return NULL;
    }

    list_node_t *node = list->head;
    list->head = node->next;

    if (list->head != NULL) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;
    }

    list_node_init(node);
    return node;
}

void list_remove(list_t *list, list_node_t *node)
{
    if (list == NULL || node == NULL || list->head == NULL) {
        return;
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else if (list->head == node) {
        list->head = node->next;
    } else {
        return;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else if (list->tail == node) {
        list->tail = node->prev;
    }

    list_node_init(node);
}

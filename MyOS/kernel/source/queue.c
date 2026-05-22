#include "queue.h"
#include "task.h"

void queue_init(queue_t *q)
{
    list_init(q);
}

void queue_enqueue(queue_t *q, TCB_t *p)
{
    if (q == NULL || p == NULL) {
        return;
    }

    list_push_back(q, &p->node);
}

TCB_t *queue_dequeue(queue_t *q)
{
    list_node_t *node = list_pop_front(q);
    return node ? list_entry(node, TCB_t, node) : NULL;
}

void queue_remove(queue_t *q, TCB_t *p)
{
    if (q == NULL || p == NULL) {
        return;
    }

    list_remove(q, &p->node);
}

int queue_is_empty(queue_t *q)
{
    return list_is_empty(q);
}

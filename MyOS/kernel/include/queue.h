#ifndef QUEUE_H
#define QUEUE_H

#include "list.h"

/* ================= FORWARD DECLARE ONLY ================= */
typedef struct TCB TCB_t;

/* Transitional semantic wrapper over the single intrusive list core. */
typedef list_t queue_t;

/* ================= API ================= */
void queue_init(queue_t *q);
void queue_enqueue(queue_t *q, TCB_t *p);
TCB_t *queue_dequeue(queue_t *q);
void queue_remove(queue_t *q, TCB_t *p);
int queue_is_empty(queue_t *q);

#endif

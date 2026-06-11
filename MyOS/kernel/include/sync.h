#ifndef SYNC_H
#define SYNC_H

#include "kernel.h"
#include "queue.h"

typedef struct TCB TCB_t;

typedef struct {
    int32_t count;
    int32_t max_count;
    queue_t wait_list;
} os_sem_t;

void sem_init(os_sem_t *sem, int32_t initial_count);
os_status_t sem_init_counting(os_sem_t *sem,
                              int32_t initial_count,
                              int32_t max_count);
void binary_sem_init(os_sem_t *sem, int initially_available);
void sem_wait(os_sem_t *sem);
os_status_t sem_wait_timeout(os_sem_t *sem, uint32_t timeout_ticks);
void sem_signal(os_sem_t *sem);
void sem_signal_from_isr(os_sem_t *sem);
int32_t sem_get_count(os_sem_t *sem);

typedef struct {
    int locked;
    TCB_t *owner;
    queue_t wait_list;
} os_mutex_t;

void mutex_init(os_mutex_t *mtx);
void mutex_lock(os_mutex_t *mtx);
os_status_t mutex_lock_timeout(os_mutex_t *mtx, uint32_t timeout_ticks);
void mutex_unlock(os_mutex_t *mtx);

#endif /* SYNC_H */

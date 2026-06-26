#ifndef SYNC_H
#define SYNC_H

#include "kernel.h"
#include "list.h"

typedef struct TCB TCB_t;

typedef struct {
    int32_t count;
    int32_t max_count;
    list_t wait_list;
} os_sem_t;

os_status_t sem_init(os_sem_t *sem,
                     int32_t initial_count,
                     int32_t max_count);
os_status_t sem_wait_timeout(os_sem_t *sem, uint32_t timeout_ticks);
void sem_signal(os_sem_t *sem);
void sem_signal_from_isr(os_sem_t *sem);
int32_t sem_get_count(os_sem_t *sem);

#define binary_sem_init(sem, available) \
    ((void)sem_init((sem), (available) ? 1 : 0, 1))

#define sem_wait(sem) \
    ((void)sem_wait_timeout((sem), OS_WAIT_FOREVER))

typedef struct {
    TCB_t *owner;
    list_t wait_list;
} os_mutex_t;

void mutex_init(os_mutex_t *mtx);
os_status_t mutex_lock_timeout(os_mutex_t *mtx, uint32_t timeout_ticks);
void mutex_unlock(os_mutex_t *mtx);

#define mutex_lock(mtx) \
    ((void)mutex_lock_timeout((mtx), OS_WAIT_FOREVER))

#endif /* SYNC_H */

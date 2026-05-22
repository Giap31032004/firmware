#ifndef SYNC_H
#define SYNC_H

#include "kernel.h"
#include "queue.h"

typedef struct TCB TCB_t;

/* --- 1. SEMAPHORE --- */
typedef struct {
    int32_t count;      // Số lượng tài nguyên
    int32_t max_count;  // Giới hạn tối đa
    queue_t wait_list;  // Danh sách các task đang đợi
} os_sem_t;

void sem_init(os_sem_t *sem, int32_t initial_count);
void sem_wait(os_sem_t *sem);
os_status_t sem_wait_timeout(os_sem_t *sem, uint32_t timeout_ticks);
void sem_signal(os_sem_t *sem);
void sem_signal_from_isr(os_sem_t *sem);

/* --- 2. MUTEX --- */
typedef struct {
    int locked;         // 0: Mở, 1: Khóa
    TCB_t *owner;       // Ai đang giữ khóa? (Quan trọng cho Mutex)
    queue_t wait_list;  // Danh sách đợi
} os_mutex_t;

void mutex_init(os_mutex_t *mtx);
void mutex_lock(os_mutex_t *mtx);
os_status_t mutex_lock_timeout(os_mutex_t *mtx, uint32_t timeout_ticks);
void mutex_unlock(os_mutex_t *mtx);

#endif /* SYNC_H */

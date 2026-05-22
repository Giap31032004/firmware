#include "sync.h"
#include "critical.h"
#include "scheduler.h"
#include "task.h"

static void inherit_priority_if_needed(os_mutex_t *mtx)
{
    if (mtx->owner == NULL || current_tcb == NULL) {
        return;
    }

    if (current_tcb->sched.priority <= mtx->owner->sched.priority) {
        return;
    }

    if (mtx->owner->state == TASK_READY) {
        remove_task_from_ready_queue(mtx->owner);
        mtx->owner->sched.priority = current_tcb->sched.priority;
        add_task_to_ready_queue(mtx->owner);
    } else {
        mtx->owner->sched.priority = current_tcb->sched.priority;
    }
}

static void disinherit_priority(TCB_t *task)
{
    if (task == NULL || task->sched.priority == task->sched.base_priority) {
        return;
    }

    if (task->state == TASK_READY) {
        remove_task_from_ready_queue(task);
        task->sched.priority = task->sched.base_priority;
        add_task_to_ready_queue(task);
    } else {
        task->sched.priority = task->sched.base_priority;
    }
}

void sem_init(os_sem_t *sem, int32_t initial_count)
{
    if (sem == NULL) {
        return;
    }

    sem->count = initial_count;
    sem->max_count = initial_count;
    queue_init(&sem->wait_list);
}

os_status_t sem_wait_timeout(os_sem_t *sem, uint32_t timeout_ticks)
{
    if (sem == NULL) {
        return OS_ERROR;
    }

    while (1) {
        uint32_t irq_state = os_enter_critical();

        if (sem->count > 0) {
            sem->count--;
            os_exit_critical(irq_state);
            return OS_OK;
        }

        if (timeout_ticks == 0) {
            os_exit_critical(irq_state);
            return OS_TIMEOUT;
        }

        os_exit_critical(irq_state);

        os_status_t result = task_block_current_on(&sem->wait_list,
                                                   TASK_WAITING_OBJECT,
                                                   timeout_ticks);
        if (result == OS_OK) {
            return OS_OK;
        }

        if (result != OS_OK || timeout_ticks != OS_WAIT_FOREVER) {
            return result;
        }
    }
}

void sem_wait(os_sem_t *sem)
{
    (void)sem_wait_timeout(sem, OS_WAIT_FOREVER);
}

void sem_signal(os_sem_t *sem)
{
    if (sem == NULL) {
        return;
    }

    uint32_t irq_state = os_enter_critical();

    if (!queue_is_empty(&sem->wait_list)) {
        (void)task_wake_one(&sem->wait_list, OS_OK);
        os_exit_critical(irq_state);
        scheduler_yield_if_needed();
        return;
    }

    sem->count++;
    if (sem->max_count > 0 && sem->count > sem->max_count) {
        sem->count = sem->max_count;
    }

    os_exit_critical(irq_state);
}

void sem_signal_from_isr(os_sem_t *sem)
{
    sem_signal(sem);
}

void mutex_init(os_mutex_t *mtx)
{
    if (mtx == NULL) {
        return;
    }

    mtx->locked = 0;
    mtx->owner = NULL;
    queue_init(&mtx->wait_list);
}

os_status_t mutex_lock_timeout(os_mutex_t *mtx, uint32_t timeout_ticks)
{
    if (mtx == NULL) {
        return OS_ERROR;
    }

    while (1) {
        uint32_t irq_state = os_enter_critical();

        if (mtx->owner == current_tcb) {
            os_exit_critical(irq_state);
            return OS_OK;
        }

        if (mtx->locked == 0) {
            mtx->locked = 1;
            mtx->owner = current_tcb;
            os_exit_critical(irq_state);
            return OS_OK;
        }

        if (timeout_ticks == 0) {
            os_exit_critical(irq_state);
            return OS_TIMEOUT;
        }

        inherit_priority_if_needed(mtx);
        os_exit_critical(irq_state);

        os_status_t result = task_block_current_on(&mtx->wait_list,
                                                   TASK_WAITING_OBJECT,
                                                   timeout_ticks);
        if (result == OS_TIMEOUT || result == OS_ERROR) {
            return result;
        }

        if (mtx->owner == current_tcb) {
            return OS_OK;
        }
    }
}

void mutex_lock(os_mutex_t *mtx)
{
    (void)mutex_lock_timeout(mtx, OS_WAIT_FOREVER);
}

void mutex_unlock(os_mutex_t *mtx)
{
    if (mtx == NULL) {
        return;
    }

    uint32_t irq_state = os_enter_critical();

    if (mtx->owner == current_tcb) {
        TCB_t *old_owner = mtx->owner;
        TCB_t *next_owner = NULL;

        disinherit_priority(old_owner);

        if (!queue_is_empty(&mtx->wait_list)) {
            next_owner = task_wake_one(&mtx->wait_list, OS_OK);
            mtx->locked = 1;
            mtx->owner = next_owner;
        } else {
            mtx->locked = 0;
            mtx->owner = NULL;
        }
    }

    os_exit_critical(irq_state);
    scheduler_yield_if_needed();
}

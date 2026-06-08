#include "sync.h"
#include "critical.h"
#include "port.h"
#include "scheduler.h"
#include "os_trace.h"
#include "task.h"

static void set_effective_priority(TCB_t *task, uint8_t priority)
{
    if (task == NULL || task->sched.priority == priority) {
        return;
    }

    if (task->state == TASK_READY) {
        remove_task_from_ready_queue(task);
        task->sched.priority = priority;
        task->state = TASK_BLOCKED;
        add_task_to_ready_queue(task);
    } else {
        task->sched.priority = priority;
    }
}

static void inherit_priority_if_needed(os_mutex_t *mtx)
{
    if (mtx->owner == NULL || current_tcb == NULL) {
        return;
    }

    if (current_tcb->sched.priority <= mtx->owner->sched.priority) {
        return;
    }

    set_effective_priority(mtx->owner, current_tcb->sched.priority);
}

static uint8_t highest_waiter_priority(os_mutex_t *mtx, uint8_t priority)
{
    list_node_t *node;

    if (mtx == NULL) {
        return priority;
    }

    list_for_each(node, &mtx->wait_list) {
        TCB_t *waiter = list_entry(node, TCB_t, node);

        if (waiter->sched.priority > priority) {
            priority = waiter->sched.priority;
        }
    }

    return priority;
}

static void recompute_inherited_priority(TCB_t *task)
{
    list_node_t *node;
    uint8_t priority;

    if (task == NULL) {
        return;
    }

    priority = task->sched.base_priority;

    list_for_each(node, &task->held_mutexes) {
        os_mutex_t *mtx = list_entry(node, os_mutex_t, owner_node);
        priority = highest_waiter_priority(mtx, priority);
    }

    set_effective_priority(task, priority);
}

static void note_mutex_acquired(TCB_t *task, os_mutex_t *mtx)
{
    if (task != NULL && mtx != NULL && mtx->owner_listed == 0U) {
        list_push_back(&task->held_mutexes, &mtx->owner_node);
        mtx->owner_listed = 1U;
        task->mutexes_held_count++;
    }
}

static void note_mutex_released(TCB_t *task, os_mutex_t *mtx)
{
    if (task != NULL && mtx != NULL && mtx->owner_listed != 0U) {
        list_remove(&task->held_mutexes, &mtx->owner_node);
        mtx->owner_listed = 0U;

        if (task->mutexes_held_count == 0U) {
            return;
        }

        task->mutexes_held_count--;
    }
}

static void recover_mutex_if_owner_dead(os_mutex_t *mtx)
{
    if (mtx == NULL || mtx->locked == 0 || mtx->owner == NULL) {
        return;
    }

    if (mtx->owner->state == TASK_UNUSED ||
        mtx->owner->state == TASK_TERMINATED) {
        note_mutex_released(mtx->owner, mtx);
        mtx->locked = 0;
        mtx->owner = NULL;
        mtx->lock_count = 0;
    }
}

void sem_init(os_sem_t *sem, int32_t initial_count)
{
    int32_t max_count = initial_count > 0 ? initial_count : 1;

    (void)sem_init_counting(sem, initial_count, max_count);
}

os_status_t sem_init_counting(os_sem_t *sem,
                              int32_t initial_count,
                              int32_t max_count)
{
    if (sem == NULL) {
        return OS_ERROR;
    }

    if (max_count <= 0 || initial_count < 0 || initial_count > max_count) {
        return OS_ERROR;
    }

    sem->count = initial_count;
    sem->max_count = max_count;
    queue_init(&sem->wait_list);

    return OS_OK;
}

void binary_sem_init(os_sem_t *sem, int initially_available)
{
    (void)sem_init_counting(sem, initially_available ? 1 : 0, 1);
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
            MYOS_TRACE(OS_TRACE_SEM_WAIT,
                       current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
                       (uint32_t)sem,
                       (uint32_t)sem->count);
            os_exit_critical(irq_state);
            return OS_OK;
        }

        if (timeout_ticks == 0) {
            os_exit_critical(irq_state);
            return OS_TIMEOUT;
        }

        os_exit_critical(irq_state);

        os_status_t result = task_block_current_on(&sem->wait_list,
                                                   TASK_BLOCKED,
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
        MYOS_TRACE(OS_TRACE_SEM_SIGNAL,
                   current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
                   (uint32_t)sem,
                   1U);
        os_exit_critical(irq_state);
        scheduler_yield_if_needed();
        return;
    }

    sem->count++;
    if (sem->count > sem->max_count) {
        sem->count = sem->max_count;
    }
    MYOS_TRACE(OS_TRACE_SEM_SIGNAL,
               current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
               (uint32_t)sem,
               (uint32_t)sem->count);

    os_exit_critical(irq_state);
}

void sem_signal_from_isr(os_sem_t *sem)
{
    TCB_t *woken = NULL;

    if (sem == NULL) {
        return;
    }

    uint32_t irq_state = os_enter_critical();

    if (!queue_is_empty(&sem->wait_list)) {
        woken = task_wake_one(&sem->wait_list, OS_OK);
        MYOS_TRACE(OS_TRACE_SEM_SIGNAL, UINT32_MAX, (uint32_t)sem, 1U);
        os_exit_critical(irq_state);

        if (woken != NULL &&
            (current_tcb == NULL ||
             woken->sched.priority > current_tcb->sched.priority)) {
            port_yield_from_isr();
        }
        return;
    }

    sem->count++;
    if (sem->count > sem->max_count) {
        sem->count = sem->max_count;
    }
    MYOS_TRACE(OS_TRACE_SEM_SIGNAL, UINT32_MAX, (uint32_t)sem, (uint32_t)sem->count);

    os_exit_critical(irq_state);
}

int32_t sem_get_count(os_sem_t *sem)
{
    int32_t count;

    if (sem == NULL) {
        return -1;
    }

    uint32_t irq_state = os_enter_critical();
    count = sem->count;
    os_exit_critical(irq_state);

    return count;
}

void mutex_init(os_mutex_t *mtx)
{
    if (mtx == NULL) {
        return;
    }

    mtx->locked = 0;
    mtx->owner = NULL;
    mtx->lock_count = 0;
    mtx->owner_listed = 0U;
    list_node_init(&mtx->owner_node);
    queue_init(&mtx->wait_list);
}

os_status_t mutex_lock_timeout(os_mutex_t *mtx, uint32_t timeout_ticks)
{
    if (mtx == NULL) {
        return OS_ERROR;
    }

    if (port_in_isr()) {
        return OS_ERROR;
    }

    while (1) {
        uint32_t irq_state = os_enter_critical();

        recover_mutex_if_owner_dead(mtx);

        if (mtx->owner == current_tcb) {
            mtx->lock_count++;
            MYOS_TRACE(OS_TRACE_MUTEX_LOCK,
                       current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
                       (uint32_t)mtx,
                       (uint32_t)mtx->lock_count);
            os_exit_critical(irq_state);
            return OS_OK;
        }

        if (mtx->locked == 0) {
            mtx->locked = 1;
            mtx->owner = current_tcb;
            mtx->lock_count = 1;
            note_mutex_acquired(current_tcb, mtx);
            MYOS_TRACE(OS_TRACE_MUTEX_LOCK,
                       current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
                       (uint32_t)mtx,
                       1U);
            os_exit_critical(irq_state);
            return OS_OK;
        }

        if (timeout_ticks == 0) {
            os_exit_critical(irq_state);
            return OS_TIMEOUT;
        }

        inherit_priority_if_needed(mtx);
        MYOS_TRACE(OS_TRACE_MUTEX_WAIT,
                   current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
                   (uint32_t)mtx,
                   timeout_ticks);
        os_exit_critical(irq_state);

        os_status_t result = task_block_current_on(&mtx->wait_list,
                                                   TASK_BLOCKED,
                                                   timeout_ticks);
        if (result == OS_TIMEOUT || result == OS_ERROR) {
            uint32_t recompute_irq_state = os_enter_critical();
            recompute_inherited_priority(mtx->owner);
            os_exit_critical(recompute_irq_state);
            return result;
        }

        if (mtx->owner == current_tcb) {
            mtx->lock_count = 1;
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

    if (port_in_isr()) {
        return;
    }

    uint32_t irq_state = os_enter_critical();

    if (mtx->owner == current_tcb) {
        TCB_t *old_owner = mtx->owner;
        TCB_t *next_owner = NULL;

        if (mtx->lock_count > 1) {
            mtx->lock_count--;
            MYOS_TRACE(OS_TRACE_MUTEX_UNLOCK,
                       current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
                       (uint32_t)mtx,
                       (uint32_t)mtx->lock_count);
            os_exit_critical(irq_state);
            return;
        }

        note_mutex_released(old_owner, mtx);

        if (!queue_is_empty(&mtx->wait_list)) {
            next_owner = task_wake_one(&mtx->wait_list, OS_OK);
            mtx->locked = 1;
            mtx->owner = next_owner;
            mtx->lock_count = 1;
            note_mutex_acquired(next_owner, mtx);
        } else {
            mtx->locked = 0;
            mtx->owner = NULL;
            mtx->lock_count = 0;
        }

        recompute_inherited_priority(old_owner);
        recompute_inherited_priority(next_owner);
        MYOS_TRACE(OS_TRACE_MUTEX_UNLOCK,
                   current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
                   (uint32_t)mtx,
                   next_owner != NULL ? next_owner->tid : UINT32_MAX);
    }

    os_exit_critical(irq_state);
    scheduler_yield_if_needed();
}

os_status_t recursive_mutex_lock_timeout(os_mutex_t *mtx,
                                         uint32_t timeout_ticks)
{
    return mutex_lock_timeout(mtx, timeout_ticks);
}

void recursive_mutex_lock(os_mutex_t *mtx)
{
    (void)recursive_mutex_lock_timeout(mtx, OS_WAIT_FOREVER);
}

void recursive_mutex_unlock(os_mutex_t *mtx)
{
    mutex_unlock(mtx);
}

#include "sync.h"

#include "critical.h"
#include "os_trace.h"
#include "port.h"
#include "scheduler.h"
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

static uint8_t highest_waiter_priority(os_mutex_t *mtx, uint8_t priority)
{
    list_node_t *node;

    list_for_each(node, &mtx->wait_list) {
        TCB_t *waiter = list_entry(node, TCB_t, node);

        if (waiter->sched.priority > priority) {
            priority = waiter->sched.priority;
        }
    }

    return priority;
}

static void update_owner_priority(os_mutex_t *mtx)
{
    if (mtx == NULL || mtx->owner == NULL) {
        return;
    }

    set_effective_priority(
        mtx->owner,
        highest_waiter_priority(mtx, mtx->owner->sched.base_priority));
}

os_status_t sem_init(os_sem_t *sem,
                     int32_t initial_count,
                     int32_t max_count)
{
    if (sem == NULL ||
        max_count <= 0 ||
        initial_count < 0 ||
        initial_count > max_count) {
        return OS_ERROR;
    }

    sem->count = initial_count;
    sem->max_count = max_count;
    list_init(&sem->wait_list);
    return OS_OK;
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

        if (timeout_ticks == 0U) {
            os_exit_critical(irq_state);
            return OS_TIMEOUT;
        }

        os_exit_critical(irq_state);

        os_status_t result = task_block_current_on(&sem->wait_list,
                                                   TASK_BLOCKED,
                                                   timeout_ticks);
        return result;
    }
}

void sem_signal(os_sem_t *sem)
{
    if (sem == NULL) {
        return;
    }

    uint32_t irq_state = os_enter_critical();

    if (!list_is_empty(&sem->wait_list)) {
        (void)task_wake_one(&sem->wait_list, OS_OK);
        MYOS_TRACE(OS_TRACE_SEM_SIGNAL,
                   current_tcb != NULL ? current_tcb->tid : UINT32_MAX,
                   (uint32_t)sem,
                   1U);
        os_exit_critical(irq_state);
        scheduler_yield_if_needed();
        return;
    }

    if (sem->count < sem->max_count) {
        sem->count++;
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

    if (!list_is_empty(&sem->wait_list)) {
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

    if (sem->count < sem->max_count) {
        sem->count++;
    }

    MYOS_TRACE(OS_TRACE_SEM_SIGNAL,
               UINT32_MAX,
               (uint32_t)sem,
               (uint32_t)sem->count);
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

    mtx->owner = NULL;
    list_init(&mtx->wait_list);
}

os_status_t mutex_lock_timeout(os_mutex_t *mtx, uint32_t timeout_ticks)
{
    if (mtx == NULL || port_in_isr()) {
        return OS_ERROR;
    }

    /* Before the scheduler starts, UART is the only caller and runs alone. */
    if (current_tcb == NULL) {
        return OS_OK;
    }

    while (1) {
        uint32_t irq_state = os_enter_critical();

        if (mtx->owner == current_tcb) {
            os_exit_critical(irq_state);
            return OS_ERROR;
        }

        if (mtx->owner == NULL) {
            mtx->owner = current_tcb;
            MYOS_TRACE(OS_TRACE_MUTEX_LOCK,
                       current_tcb->tid,
                       (uint32_t)mtx,
                       1U);
            os_exit_critical(irq_state);
            return OS_OK;
        }

        if (timeout_ticks == 0U) {
            os_exit_critical(irq_state);
            return OS_TIMEOUT;
        }

        if (current_tcb->sched.priority > mtx->owner->sched.priority) {
            set_effective_priority(mtx->owner,
                                   current_tcb->sched.priority);
        }

        MYOS_TRACE(OS_TRACE_MUTEX_WAIT,
                   current_tcb->tid,
                   (uint32_t)mtx,
                   timeout_ticks);
        os_exit_critical(irq_state);

        os_status_t result = task_block_current_on(&mtx->wait_list,
                                                   TASK_BLOCKED,
                                                   timeout_ticks);
        if (result != OS_OK) {
            uint32_t update_irq_state = os_enter_critical();
            update_owner_priority(mtx);
            os_exit_critical(update_irq_state);
            return result;
        }

        if (mtx->owner == current_tcb) {
            return OS_OK;
        }
    }
}

void mutex_unlock(os_mutex_t *mtx)
{
    TCB_t *old_owner;
    TCB_t *next_owner = NULL;

    if (mtx == NULL || current_tcb == NULL || port_in_isr()) {
        return;
    }

    uint32_t irq_state = os_enter_critical();

    if (mtx->owner != current_tcb) {
        os_exit_critical(irq_state);
        return;
    }

    old_owner = mtx->owner;

    if (!list_is_empty(&mtx->wait_list)) {
        next_owner = task_wake_one(&mtx->wait_list, OS_OK);
        mtx->owner = next_owner;
        update_owner_priority(mtx);
    } else {
        mtx->owner = NULL;
    }

    set_effective_priority(old_owner, old_owner->sched.base_priority);

    MYOS_TRACE(OS_TRACE_MUTEX_UNLOCK,
               current_tcb->tid,
               (uint32_t)mtx,
               next_owner != NULL ? next_owner->tid : UINT32_MAX);

    os_exit_critical(irq_state);
    scheduler_yield_if_needed();
}

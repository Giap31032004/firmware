#include "event_group.h"

#include "critical.h"
#include "port.h"
#include "scheduler.h"
#include "task.h"

static int event_bits_match(os_event_bits_t current_bits,
                            os_event_bits_t bits_to_wait_for,
                            int wait_for_all)
{
    if (bits_to_wait_for == 0U) {
        return 0;
    }

    if (wait_for_all) {
        return ((current_bits & bits_to_wait_for) == bits_to_wait_for);
    }

    return ((current_bits & bits_to_wait_for) != 0U);
}

static void event_task_clear_wait_state(TCB_t *task)
{
    if (task == NULL) {
        return;
    }

    task->event_wait_bits = 0U;
    task->event_wait_all = 0U;
    task->event_clear_on_exit = 0U;
}

static int event_group_unblock_matching_tasks(os_event_group_t *event_group,
                                              os_event_bits_t match_bits)
{
    list_node_t *node;
    list_node_t *tmp;
    os_event_bits_t clear_mask = 0U;
    int unblocked = 0;

    list_for_each_safe(node, tmp, &event_group->wait_list) {
        TCB_t *task = list_entry(node, TCB_t, node);

        if (!event_bits_match(match_bits,
                              task->event_wait_bits,
                              task->event_wait_all)) {
            continue;
        }

        if (task->event_clear_on_exit) {
            clear_mask |= task->event_wait_bits;
        }

        list_remove(&event_group->wait_list, &task->node);
        task->wait_list = NULL;
        task->wait_result = OS_OK;
        task->wait_has_timeout = 0U;
        task->event_result_bits = match_bits;
        event_task_clear_wait_state(task);
        add_task_to_ready_queue(task);
        unblocked = 1;
    }

    event_group->bits &= ~clear_mask;
    return unblocked;
}

void event_group_init(os_event_group_t *event_group)
{
    if (event_group == NULL) {
        return;
    }

    event_group->bits = 0U;
    queue_init(&event_group->wait_list);
}

os_event_bits_t event_group_get_bits(os_event_group_t *event_group)
{
    os_event_bits_t bits;

    if (event_group == NULL) {
        return 0U;
    }

    uint32_t irq_state = os_enter_critical();
    bits = event_group->bits;
    os_exit_critical(irq_state);

    return bits;
}

os_event_bits_t event_group_set_bits(os_event_group_t *event_group,
                                     os_event_bits_t bits_to_set)
{
    os_event_bits_t bits;
    int should_yield;

    if (event_group == NULL) {
        return 0U;
    }

    uint32_t irq_state = os_enter_critical();

    event_group->bits |= bits_to_set;
    bits = event_group->bits;
    should_yield = event_group_unblock_matching_tasks(event_group, bits);
    bits = event_group->bits;

    os_exit_critical(irq_state);

    if (should_yield) {
        scheduler_yield_if_needed();
    }

    return bits;
}

os_event_bits_t event_group_set_bits_from_isr(os_event_group_t *event_group,
                                              os_event_bits_t bits_to_set)
{
    os_event_bits_t bits;
    int should_yield;

    if (event_group == NULL) {
        return 0U;
    }

    uint32_t irq_state = os_enter_critical();

    event_group->bits |= bits_to_set;
    bits = event_group->bits;
    should_yield = event_group_unblock_matching_tasks(event_group, bits);
    bits = event_group->bits;

    os_exit_critical(irq_state);

    if (should_yield) {
        port_yield_from_isr();
    }

    return bits;
}

os_event_bits_t event_group_clear_bits(os_event_group_t *event_group,
                                       os_event_bits_t bits_to_clear)
{
    os_event_bits_t bits;

    if (event_group == NULL) {
        return 0U;
    }

    uint32_t irq_state = os_enter_critical();
    event_group->bits &= ~bits_to_clear;
    bits = event_group->bits;
    os_exit_critical(irq_state);

    return bits;
}

os_event_bits_t event_group_wait_bits(os_event_group_t *event_group,
                                      os_event_bits_t bits_to_wait_for,
                                      int clear_on_exit,
                                      int wait_for_all,
                                      uint32_t timeout_ticks)
{
    os_event_bits_t bits;

    if (event_group == NULL || bits_to_wait_for == 0U) {
        return 0U;
    }

    if (port_in_isr()) {
        return 0U;
    }

    while (1) {
        uint32_t irq_state = os_enter_critical();

        bits = event_group->bits;
        if (event_bits_match(bits, bits_to_wait_for, wait_for_all)) {
            if (clear_on_exit) {
                event_group->bits &= ~bits_to_wait_for;
            }
            os_exit_critical(irq_state);
            return bits;
        }

        if (timeout_ticks == 0U || current_tcb == NULL) {
            os_exit_critical(irq_state);
            return 0U;
        }

        current_tcb->event_wait_bits = bits_to_wait_for;
        current_tcb->event_result_bits = 0U;
        current_tcb->event_wait_all = wait_for_all ? 1U : 0U;
        current_tcb->event_clear_on_exit = clear_on_exit ? 1U : 0U;
        current_tcb->wait_list = &event_group->wait_list;
        current_tcb->wait_result = OS_OK;

        if (timeout_ticks != OS_WAIT_FOREVER && timeout_ticks > INT32_MAX) {
            timeout_ticks = INT32_MAX;
        }

        current_tcb->wait_has_timeout = (timeout_ticks != OS_WAIT_FOREVER);
        if (current_tcb->wait_has_timeout) {
            current_tcb->time.wakeup_tick = os_tick_count + timeout_ticks;
        }

        current_tcb->state = TASK_WAITING_OBJECT;
        list_push_back(&event_group->wait_list, &current_tcb->node);
        os_schedule();

        os_exit_critical(irq_state);

        if (current_tcb->wait_result == OS_OK) {
            bits = current_tcb->event_result_bits;
            current_tcb->event_result_bits = 0U;
            event_task_clear_wait_state(current_tcb);
            return bits;
        }

        current_tcb->event_result_bits = 0U;
        event_task_clear_wait_state(current_tcb);
        return 0U;
    }
}

#include "critical.h"
#include "list.h"
#include "tick.h"
#include "timer.h"

static list_t active_timer_list;

static void software_timer_insert_locked(os_timer_t *timer)
{
    list_node_t *node;

    if (timer == NULL) {
        return;
    }

    list_node_init(&timer->node);

    for (node = active_timer_list.head; node != NULL; node = node->next) {
        os_timer_t *candidate = list_entry(node, os_timer_t, node);

        if (tick_after_or_equal(candidate->expiry_tick,
                                timer->expiry_tick)) {
            timer->node.next = node;
            timer->node.prev = node->prev;

            if (node->prev != NULL) {
                node->prev->next = &timer->node;
            } else {
                active_timer_list.head = &timer->node;
            }

            node->prev = &timer->node;
            return;
        }
    }

    list_push_back(&active_timer_list, &timer->node);
}

void timer_process_tick(void)
{
    while (1) {
        os_timer_t *timer;
        os_timer_callback_t callback;
        void *arg;
        uint32_t irq_state = os_enter_critical();

        if (list_is_empty(&active_timer_list)) {
            os_exit_critical(irq_state);
            break;
        }

        timer = list_entry(active_timer_list.head, os_timer_t, node);
        if (!tick_after_or_equal(os_tick_count, timer->expiry_tick)) {
            os_exit_critical(irq_state);
            break;
        }

        list_remove(&active_timer_list, &timer->node);
        timer->active = 0U;
        callback = timer->callback;
        arg = timer->arg;

        os_exit_critical(irq_state);

        if (callback != NULL) {
            callback(arg);
        }
    }
}

uint32_t timer_ticks_until_next_expiry(void)
{
    uint32_t ticks;
    uint32_t irq_state = os_enter_critical();

    if (list_is_empty(&active_timer_list)) {
        ticks = UINT32_MAX;
    } else {
        os_timer_t *timer =
            list_entry(active_timer_list.head, os_timer_t, node);

        ticks = tick_after_or_equal(os_tick_count, timer->expiry_tick)
            ? 0U
            : timer->expiry_tick - os_tick_count;
    }

    os_exit_critical(irq_state);
    return ticks;
}

void timer_init(void)
{
    list_init(&active_timer_list);
}

void os_timer_init(os_timer_t *timer,
                   os_timer_callback_t callback,
                   void *arg)
{
    if (timer == NULL) {
        return;
    }

    list_node_init(&timer->node);
    timer->callback = callback;
    timer->arg = arg;
    timer->expiry_tick = 0U;
    timer->active = 0U;
}

os_status_t os_timer_start(os_timer_t *timer,
                           uint32_t delay_ticks)
{
    uint32_t irq_state;

    if (timer == NULL ||
        timer->callback == NULL ||
        delay_ticks == 0U) {
        return OS_ERROR;
    }

    irq_state = os_enter_critical();

    if (timer->active != 0U) {
        list_remove(&active_timer_list, &timer->node);
        timer->active = 0U;
    }

    timer->expiry_tick = os_tick_count + delay_ticks;
    timer->active = 1U;
    software_timer_insert_locked(timer);

    os_exit_critical(irq_state);
    return OS_OK;
}

void os_timer_stop(os_timer_t *timer)
{
    uint32_t irq_state;

    if (timer == NULL) {
        return;
    }

    irq_state = os_enter_critical();

    if (timer->active != 0U) {
        list_remove(&active_timer_list, &timer->node);
        timer->active = 0U;
    }

    os_exit_critical(irq_state);
}

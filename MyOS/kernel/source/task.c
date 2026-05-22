#include <stdint.h>

#include "kernel.h"
#include "heap.h"   // Gọi os_malloc, os_free
#include "uart.h"   // Gọi uart_print
#include "task.h"
#include "kernel_config.h"
#include "port.h"

/* ========================================================================
 * MACROS & BIẾN TOÀN CỤC NỘI BỘ
 * ======================================================================== */
// Dành riêng slot cuối cùng trong mảng cho Idle Task để không đụng hàng
#define IDLE_TASK_TID (MAX_TASKS - 1)
TCB_t tcb_table[MAX_TASKS]; 
static int total_tasks = 0; 
static list_t terminated_list;
static void task_create_at(void (*func)(void), uint32_t tid, uint8_t priority);
static void prvCleanupTerminatedTasks(void);
extern volatile uint32_t os_tick_count;

void task_init(void)
{
    for (uint32_t tid = 0; tid < MAX_TASKS; tid++) {
        TCB_t *t = &tcb_table[tid];
        t->stack_ptr = NULL;
        t->tid = tid;
        t->entry = NULL;
        t->state = TASK_UNUSED;
        list_node_init(&t->node);
        t->sched.priority = PRIORITY_IDLE;
        t->sched.base_priority = PRIORITY_IDLE;
        t->sched.remaining_ticks = OS_TIME_SLICE_TICKS;
        t->mem.stack_base = 0;
        t->mem.stack_size = 0;
        t->mem.heap_base = 0;
        t->mem.heap_size = 0;
        t->time.wakeup_tick = 0;
        t->wait_list = NULL;
        t->wait_result = OS_OK;
        t->wait_has_timeout = 0;
        t->dynamic_allocation = 0;
        for (int i = 0; i < NUM_RESOURCES; i++) {
            t->res.held[i] = 0;
            t->res.max[i] = 0;
        }
    }

    total_tasks = 0;
    list_init(&terminated_list);
}

/* ========================================================================
 * VÒNG ĐỜI TIẾN TRÌNH: TẠO MỚI (CREATE)
 * ======================================================================== */

 void task_create(void (*func)(void), uint8_t priority)
{
    for (uint32_t tid = 0; tid < IDLE_TASK_TID; tid++) {
        if (tcb_table[tid].state == TASK_UNUSED ||
            tcb_table[tid].state == TASK_TERMINATED) {
            task_create_at(func, tid, priority);
            return;
        }
    }

    uart_print("Error: No free task slot.\r\n");
}

static void task_create_at(void (*func)(void), uint32_t tid, uint8_t priority)
{
    if (tid >= MAX_TASKS)
        return;

    if (tid == IDLE_TASK_TID && func != prvIdleTask)
        return;

    TCB_t *t = &tcb_table[tid];

    if (t->state != TASK_UNUSED && t->state != TASK_TERMINATED)
        return;

    for (int i = 0; i < NUM_RESOURCES; i++) {
        t->res.held[i] = 0;
        t->res.max[i] = 0;
    }

    uint32_t stack_size_bytes = STACK_SIZE * 4;
    void *raw_mem = os_malloc(stack_size_bytes);
    if (raw_mem == NULL) {
        uart_print("Error: OS Heap Full. Cannot create task.\r\n");
        return;
    }

    *((uint32_t *)raw_mem) = STACK_CANARY_VALUE;

    t->mem.stack_base = (uint32_t)raw_mem;
    t->mem.stack_size = stack_size_bytes;

    uint32_t sp_addr = (uint32_t)raw_mem + stack_size_bytes;
    sp_addr &= ~7UL;
    uint32_t *sp = (uint32_t *)sp_addr;

    *(--sp) = 0x01000000UL;
    *(--sp) = (uint32_t)func;
    *(--sp) = (uint32_t)task_exit;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;

    *(--sp) = 0xFFFFFFFDUL;
    for (int i = 0; i < 8; i++) {
        *(--sp) = 0;
    }

    t->stack_ptr = sp;
    t->tid = tid;
    t->entry = func;
    t->sched.priority = priority;
    t->sched.base_priority = priority;
    t->sched.remaining_ticks = OS_TIME_SLICE_TICKS;
    t->state = TASK_NEW;

    list_node_init(&t->node);

    t->mem.heap_base = 0;
    t->mem.heap_size = 0;
    t->time.wakeup_tick = 0;
    t->wait_list = NULL;
    t->wait_result = OS_OK;
    t->wait_has_timeout = 0;
    t->dynamic_allocation = 0;

    uint32_t irq_state = os_enter_critical();

    add_task_to_ready_queue(t);
    total_tasks++;
    scheduler_yield_if_needed();

    os_exit_critical(irq_state);
}
/*=====================================================================
 * VÒNG ĐỜI TIẾN TRÌNH: TIÊU DIỆT (KILL)
 * ======================================================================== */
void task_kill(uint32_t tid)
{
    uint32_t irq_state = os_enter_critical();

    if (tid >= MAX_TASKS || tcb_table[tid].state == TASK_UNUSED) {
        os_exit_critical(irq_state);
        return;
    }

    TCB_t *t = &tcb_table[tid];

    remove_task_from_ready_queue(t);
    if (t->wait_list != NULL) {
        list_remove(t->wait_list, &t->node);
        t->wait_list = NULL;
    }

    if (current_tcb != NULL && tid == current_tcb->tid) {
        t->state = TASK_TERMINATED;
        idle_add_terminated_task(t);

        os_schedule();

        os_exit_critical(irq_state);

        while (1) {
        }
    }

    if (t->mem.stack_base != 0) {
        os_free((void *)t->mem.stack_base);
        t->mem.stack_base = 0;
    }

    t->state = TASK_UNUSED;
    total_tasks--;

    os_exit_critical(irq_state);
}

/* Hàm bắt lỗi: Kích hoạt khi một hàm Task chạy đến ngoặc nhọn cuối cùng } */
void task_exit(void)
{
    if (current_tcb != NULL) {
        task_kill(current_tcb->tid);
    }

    while (1) {
    }
}

/* ========================================================================
 * VÒNG ĐỜI TIẾN TRÌNH: TẠM DỪNG (SUSPEND) & KHÔI PHỤC (RESUME)
 * ======================================================================== */
void task_suspend(uint32_t tid)
{
    uint32_t irq_state = os_enter_critical();

    if (tid >= MAX_TASKS || tcb_table[tid].state == TASK_UNUSED) {
        os_exit_critical(irq_state);
        return;
    }

    TCB_t *t = &tcb_table[tid];

    if (t->state == TASK_TERMINATED || t->state == TASK_SUSPENDED) {
        os_exit_critical(irq_state);
        return;
    }

    remove_task_from_ready_queue(t);
    t->state = TASK_SUSPENDED;

    if (current_tcb != NULL && tid == current_tcb->tid) {
        os_schedule();
    }

    os_exit_critical(irq_state);
}

void task_resume(uint32_t tid)
{
    uint32_t irq_state = os_enter_critical();

    if (tid >= MAX_TASKS || tcb_table[tid].state == TASK_UNUSED) {
        os_exit_critical(irq_state);
        return;
    }

    TCB_t *t = &tcb_table[tid];

    if (t->state != TASK_SUSPENDED) {
        os_exit_critical(irq_state);
        return;
    }

    add_task_to_ready_queue(t);

    scheduler_yield_if_needed();

    os_exit_critical(irq_state);
}


void task_yield(void)
{
    os_schedule();
}

void task_delay(uint32_t ticks)
{
    os_delay(ticks);
}

os_status_t task_block_current_on(list_t *wait_list,
                                  task_state_t wait_state,
                                  uint32_t timeout_ticks)
{
    uint32_t irq_state = os_enter_critical();

    if (current_tcb == NULL || wait_list == NULL) {
        os_exit_critical(irq_state);
        return OS_ERROR;
    }

    current_tcb->wait_list = wait_list;
    current_tcb->wait_result = OS_OK;
    current_tcb->wait_has_timeout = (timeout_ticks != OS_WAIT_FOREVER);
    current_tcb->state = wait_state;

    if (current_tcb->wait_has_timeout) {
        current_tcb->time.wakeup_tick = os_tick_count + timeout_ticks;
    }

    list_push_back(wait_list, &current_tcb->node);
    os_schedule();

    os_exit_critical(irq_state);

    os_status_t result = current_tcb->wait_result;
    current_tcb->wait_result = OS_OK;
    current_tcb->wait_has_timeout = 0;

    return result;
}

TCB_t *task_wake_one(list_t *wait_list, os_status_t result)
{
    if (wait_list == NULL) {
        return NULL;
    }

    list_node_t *node = list_pop_front(wait_list);
    TCB_t *task = node ? list_entry(node, TCB_t, node) : NULL;

    if (task != NULL) {
        task->wait_list = NULL;
        task->wait_result = result;
        task->wait_has_timeout = 0;
        add_task_to_ready_queue(task);
    }

    return task;
}

void task_process_timeouts(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        TCB_t *task = &tcb_table[i];

        if (task->wait_list != NULL &&
            task->wait_has_timeout &&
            os_tick_count >= task->time.wakeup_tick) {
            list_remove(task->wait_list, &task->node);
            task->wait_list = NULL;
            task->wait_result = OS_TIMEOUT;
            task->wait_has_timeout = 0;
            add_task_to_ready_queue(task);
        }
    }
}

void task_check_stack(TCB_t *task)
{
#if OS_ENABLE_STACK_CHECK
    if (task == NULL || task->mem.stack_base == 0) {
        return;
    }

    if (*((uint32_t *)task->mem.stack_base) != STACK_CANARY_VALUE) {
        kernel_panic("stack overflow", __FILE__, __LINE__);
    }
#else
    (void)task;
#endif
}

void task_check_all_stacks(void)
{
#if OS_ENABLE_STACK_CHECK
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tcb_table[i].state != TASK_UNUSED) {
            task_check_stack(&tcb_table[i]);
        }
    }
#endif
}

/* =========================================================
 * READY
 * ========================================================= */
void task_set_ready(uint32_t tid)
{
    if (tid >= MAX_TASKS)
        return;

    TCB_t *task = &tcb_table[tid];

    uint32_t irq_state = os_enter_critical();

    if (task->state == TASK_TERMINATED) {
        os_exit_critical(irq_state);
        return;
    }

    add_task_to_ready_queue(task);

    os_exit_critical(irq_state);
}

/* =========================================================
 * RUNNING
 * ========================================================= */
void task_set_running(uint32_t tid)
{
    if (tid >= MAX_TASKS)
        return;

    TCB_t *task = &tcb_table[tid];

    uint32_t irq_state = os_enter_critical();

    task->state = TASK_RUNNING;
    current_tcb = task;

    os_exit_critical(irq_state);
}

/* =========================================================
 * BLOCKED
 * ========================================================= */
void task_set_blocked(uint32_t tid)
{
    if (tid >= MAX_TASKS)
        return;

    TCB_t *task = &tcb_table[tid];

    uint32_t irq_state = os_enter_critical();

    if (task->state == TASK_UNUSED || task->state == TASK_TERMINATED) {
        os_exit_critical(irq_state);
        return;
    }

    remove_task_from_ready_queue(task);

    task->state = TASK_BLOCKED;

    os_exit_critical(irq_state);
}
/* =========================================================
 * SUSPENDED
 * ========================================================= */
void task_set_suspended(uint32_t tid)
{
    if (tid >= MAX_TASKS)
        return;

    TCB_t *task = &tcb_table[tid];

    uint32_t irq_state = os_enter_critical();

    if (task->state == TASK_UNUSED || task->state == TASK_TERMINATED) {
        os_exit_critical(irq_state);
        return;
    }

    remove_task_from_ready_queue(task);

    task->state = TASK_SUSPENDED;

    os_exit_critical(irq_state);
}

/* =========================================================
 * TERMINATED
 * ========================================================= */
void task_set_terminated(uint32_t tid)
{
    if (tid >= MAX_TASKS)
        return;

    TCB_t *task = &tcb_table[tid];

    uint32_t irq_state = os_enter_critical();

    if (task->state == TASK_UNUSED || task->state == TASK_TERMINATED) {
        os_exit_critical(irq_state);
        return;
    }

    remove_task_from_ready_queue(task);

    task->state = TASK_TERMINATED;

    idle_add_terminated_task(task);

    os_exit_critical(irq_state);
}
/* ========================================================================
 * TASK RẢNH RỖI HỆ THỐNG (IDLE TASK & GARBAGE COLLECTOR)
 * ======================================================================== */
/* =========================================================
 * Add terminated task
 * ========================================================= */
void idle_add_terminated_task(TCB_t *task)
{
    if (task == NULL)
        return;

    uint32_t irq_state = os_enter_critical();

    list_push_back(&terminated_list, &task->node);

    os_exit_critical(irq_state);
}

/* =========================================================
 * Cleanup dead tasks
 * ========================================================= */
static void prvCleanupTerminatedTasks(void)
{
    while (1) {
        uint32_t irq_state = os_enter_critical();

        list_node_t *node = list_pop_front(&terminated_list);
        TCB_t *task = node ? list_entry(node, TCB_t, node) : NULL;

        os_exit_critical(irq_state);

        if (task == NULL)
            break;

        if (task->mem.stack_base != 0) {
            os_free((void *)task->mem.stack_base);
            task->mem.stack_base = 0;
        }

        task->state = TASK_UNUSED;

        if (total_tasks > 0)
            total_tasks--;

        if (task->dynamic_allocation) {
            os_free(task);
        }
    }
}

/* =========================================================
 * Optional idle hook
 * ========================================================= */
__attribute__((weak))
void vApplicationIdleHook(void)
{
}

/* =========================================================
 * Idle task
 * ========================================================= */
void prvIdleTask(void)
{
    while (1) {
        prvCleanupTerminatedTasks();

        vApplicationIdleHook();

#if defined(OS_ENABLE_LOW_POWER) && OS_ENABLE_LOW_POWER == 1
        __WFI();
#endif
    }
}

void task_set_state(uint32_t tid, task_state_t state)
{
    if (tid >= MAX_TASKS)
        return;

    TCB_t *task = &tcb_table[tid];

    if (task->state == TASK_UNUSED)
        return;

    switch (state) {
    case TASK_READY:
        task_set_ready(tid);
        break;

    case TASK_RUNNING:
        task_set_running(tid);
        break;

    case TASK_BLOCKED:
        task_set_blocked(tid);
        break;

    case TASK_WAITING_OBJECT:
    case TASK_WAITING_IO:
    case TASK_WAITING_TIME:
        task_set_blocked(tid);
        task->state = state;
        break;

    case TASK_SUSPENDED:
        task_set_suspended(tid);
        break;

    case TASK_TERMINATED:
        task_set_terminated(tid);
        break;

    default:
        task->state = state;
        break;
    }
}

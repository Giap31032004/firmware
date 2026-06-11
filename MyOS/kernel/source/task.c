#include <stdint.h>

#include "kernel.h"
#include "heap.h"   // Gọi os_malloc, os_free
#include "uart.h"   // Gọi uart_print
#include "task.h"
#include "kernel_config.h"
#include "low_power.h"
#include "os_trace.h"
#include "port.h"
#include "mpu.h"
#include "runtime_stats.h"

/* ========================================================================
 * MACROS & BIẾN TOÀN CỤC NỘI BỘ
 * ======================================================================== */
// Dành riêng slot cuối cùng trong mảng cho Idle Task để không đụng hàng
#define IDLE_TASK_TID (MAX_TASKS - 1)
TCB_t tcb_table[MAX_TASKS]; 
static int total_tasks = 0; 
static list_t terminated_list;
static list_t delay_list;
static os_status_t task_setup(void (*func)(void),
                              uint32_t tid,
                              uint8_t priority,
                              uint32_t stack_words,
                              uint32_t stack_alloc_base,
                              uint32_t stack_base,
                              uint32_t stack_size_bytes,
                              uint8_t dynamic_allocation,
                              uint32_t *out_tid);
static void prvCleanupTerminatedTasks(void);
static void task_detach_from_current_list(TCB_t *task);
static void task_free_stack(TCB_t *task);
static TCB_t *task_find_highest_priority_waiter(list_t *wait_list);

static uint32_t task_round_stack_size(uint32_t requested_bytes)
{
    uint32_t size = 32U;

    while (size < requested_bytes && size <= (UINT32_MAX / 2U)) {
        size <<= 1U;
    }

    if (size < requested_bytes) {
        return 0U;
    }

    return size;
}

static void task_clear_mpu(TCB_t *task)
{
    task->mpu.stack_region.base = 0U;
    task->mpu.stack_region.size_bytes = 0U;
    task->mpu.stack_region.rasr = MPU_REGION_DISABLE;
    task->mpu.extra_region.base = 0U;
    task->mpu.extra_region.size_bytes = 0U;
    task->mpu.extra_region.rasr = MPU_REGION_DISABLE;
}

static void task_fill_stack(uint32_t stack_base, uint32_t stack_size)
{
    uint32_t *stack = (uint32_t *)stack_base;
    uint32_t words = stack_size / sizeof(uint32_t);

    for (uint32_t i = 0; i < words; i++) {
        stack[i] = STACK_FILL_VALUE;
    }

    for (uint32_t i = 0; i < OS_STACK_GUARD_WORDS && i < words; i++) {
        stack[i] = STACK_CANARY_VALUE;
    }
}

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
        t->mem.stack_alloc_base = 0;
        t->mem.stack_base = 0;
        t->mem.stack_size = 0;
        t->mem.heap_base = 0;
        t->mem.heap_size = 0;
        task_clear_mpu(t);
        t->time.wakeup_tick = 0;
        t->wait_list = NULL;
        t->wait_result = OS_OK;
        t->wait_has_timeout = 0;
        t->event_wait_bits = 0U;
        t->event_result_bits = 0U;
        t->event_wait_all = 0U;
        t->event_clear_on_exit = 0U;
        t->mutexes_held_count = 0;
        list_init(&t->held_mutexes);
#if defined(OS_GENERATE_RUN_TIME_STATS) && OS_GENERATE_RUN_TIME_STATS == 1
        t->runtime_cycles = 0U;
        t->runtime_last_start = 0U;
#endif
        t->dynamic_allocation = 0;
    }

    total_tasks = 0;
    list_init(&terminated_list);
    list_init(&delay_list);
}

/* ========================================================================
 * VÒNG ĐỜI TIẾN TRÌNH: TẠO MỚI (CREATE)
 * ======================================================================== */

os_status_t task_create_dynamic(void (*func)(void),
                                uint8_t priority,
                                uint32_t stack_words,
                                uint32_t *out_tid)
{
    uint32_t tid = TASK_INVALID_TID;
    uint32_t requested_stack_bytes;
    uint32_t stack_size_bytes;
    uint32_t stack_base;
    void *raw_mem;

    if (out_tid != NULL) {
        *out_tid = TASK_INVALID_TID;
    }

    if (func == NULL) {
        uart_print("Error: Task entry is NULL.\r\n");
        return OS_ERROR;
    }

    if (priority >= MAX_PRIORITY) {
        uart_print("Error: Invalid task priority.\r\n");
        return OS_ERROR;
    }

    if (stack_words < OS_MIN_STACK_WORDS) {
        uart_print("Error: Task stack too small.\r\n");
        return OS_ERROR;
    }

    if (stack_words > (UINT32_MAX / sizeof(uint32_t))) {
        uart_print("Error: Task stack too large.\r\n");
        return OS_ERROR;
    }

    if (func == prvIdleTask) {
        tid = IDLE_TASK_TID;
    } else {
        for (uint32_t candidate = 0; candidate < IDLE_TASK_TID; candidate++) {
            if (tcb_table[candidate].state == TASK_UNUSED ||
                tcb_table[candidate].state == TASK_TERMINATED) {
                tid = candidate;
                break;
            }
        }
    }

    if (tid == TASK_INVALID_TID) {
        uart_print("Error: No free task slot.\r\n");
        return OS_ERROR;
    }

    if (tcb_table[tid].state != TASK_UNUSED &&
        tcb_table[tid].state != TASK_TERMINATED) {
        uart_print("Error: Task slot is already in use.\r\n");
        return OS_ERROR;
    }

    requested_stack_bytes = stack_words * sizeof(uint32_t);
    stack_size_bytes = task_round_stack_size(requested_stack_bytes);
    if (stack_size_bytes == 0U ||
        stack_size_bytes > (UINT32_MAX / 2U)) {
        uart_print("Error: Task stack too large.\r\n");
        return OS_ERROR;
    }

    raw_mem = os_malloc((size_t)stack_size_bytes * 2U);
    if (raw_mem == NULL) {
        uart_print("Error: OS Heap Full. Cannot create task.\r\n");
        return OS_ERROR;
    }

    stack_base = ((uint32_t)raw_mem + stack_size_bytes - 1U) &
                 ~(stack_size_bytes - 1U);

    return task_setup(func,
                      tid,
                      priority,
                      stack_words,
                      (uint32_t)raw_mem,
                      stack_base,
                      stack_size_bytes,
                      1U,
                      out_tid);
}

os_status_t task_create_static(void (*func)(void),
                               uint8_t priority,
                               uint32_t *stack_buffer,
                               uint32_t stack_words,
                               uint32_t *out_tid)
{
    uint32_t tid = TASK_INVALID_TID;
    uint32_t stack_size_bytes;
    uint32_t stack_base;

    if (out_tid != NULL) {
        *out_tid = TASK_INVALID_TID;
    }

    if (func == NULL) {
        uart_print("Error: Task entry is NULL.\r\n");
        return OS_ERROR;
    }

    if (priority >= MAX_PRIORITY) {
        uart_print("Error: Invalid task priority.\r\n");
        return OS_ERROR;
    }

    if (stack_buffer == NULL) {
        uart_print("Error: Static task stack is NULL.\r\n");
        return OS_ERROR;
    }

    if (stack_words < OS_MIN_STACK_WORDS) {
        uart_print("Error: Task stack too small.\r\n");
        return OS_ERROR;
    }

    if (stack_words > (UINT32_MAX / sizeof(uint32_t))) {
        uart_print("Error: Task stack too large.\r\n");
        return OS_ERROR;
    }

    if (func == prvIdleTask) {
        tid = IDLE_TASK_TID;
    } else {
        for (uint32_t candidate = 0; candidate < IDLE_TASK_TID; candidate++) {
            if (tcb_table[candidate].state == TASK_UNUSED ||
                tcb_table[candidate].state == TASK_TERMINATED) {
                tid = candidate;
                break;
            }
        }
    }

    if (tid == TASK_INVALID_TID) {
        uart_print("Error: No free task slot.\r\n");
        return OS_ERROR;
    }

    if (tcb_table[tid].state != TASK_UNUSED &&
        tcb_table[tid].state != TASK_TERMINATED) {
        uart_print("Error: Task slot is already in use.\r\n");
        return OS_ERROR;
    }

    stack_size_bytes = task_round_stack_size(stack_words * sizeof(uint32_t));
    if (stack_size_bytes == 0U ||
        stack_size_bytes != (stack_words * sizeof(uint32_t))) {
        uart_print("Error: Static task stack size must be power-of-two.\r\n");
        return OS_ERROR;
    }

    stack_base = (uint32_t)stack_buffer;
    if ((stack_base & (stack_size_bytes - 1U)) != 0U) {
        uart_print("Error: Static task stack is not MPU-aligned.\r\n");
        return OS_ERROR;
    }

    return task_setup(func,
                      tid,
                      priority,
                      stack_words,
                      0U,
                      stack_base,
                      stack_size_bytes,
                      0U,
                      out_tid);
}

static os_status_t task_setup(void (*func)(void),
                              uint32_t tid,
                              uint8_t priority,
                              uint32_t stack_words,
                              uint32_t stack_alloc_base,
                              uint32_t stack_base,
                              uint32_t stack_size_bytes,
                              uint8_t dynamic_allocation,
                              uint32_t *out_tid)
{
    TCB_t *t = &tcb_table[tid];

    task_fill_stack(stack_base, stack_size_bytes);

    t->mem.stack_alloc_base = stack_alloc_base;
    t->mem.stack_base = stack_base;
    t->mem.stack_size = stack_size_bytes;
    t->mpu.stack_region.base = stack_base;
    t->mpu.stack_region.size_bytes = stack_size_bytes;
    t->mpu.stack_region.rasr = MPU_AP_FULL | MPU_MEM_NORMAL_WB_WA | MPU_XN;
    t->mpu.extra_region.base = 0U;
    t->mpu.extra_region.size_bytes = 0U;
    t->mpu.extra_region.rasr = MPU_REGION_DISABLE;

    uint32_t sp_addr = stack_base + stack_size_bytes;
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
    t->event_wait_bits = 0U;
    t->event_result_bits = 0U;
    t->event_wait_all = 0U;
    t->event_clear_on_exit = 0U;
    t->mutexes_held_count = 0;
    list_init(&t->held_mutexes);
#if defined(OS_GENERATE_RUN_TIME_STATS) && OS_GENERATE_RUN_TIME_STATS == 1
    t->runtime_cycles = 0U;
    t->runtime_last_start = 0U;
#endif
    t->dynamic_allocation = dynamic_allocation;

    uint32_t irq_state = os_enter_critical();

    add_task_to_ready_queue(t);
    total_tasks++;
    scheduler_yield_if_needed();

    os_exit_critical(irq_state);

    if (out_tid != NULL) {
        *out_tid = tid;
    }

    MYOS_TRACE(OS_TRACE_TASK_CREATE, tid, priority, stack_words);

    return OS_OK;
}

static void task_detach_from_current_list(TCB_t *task)
{
    if (task == NULL) {
        return;
    }

    if (task->wait_list != NULL) {
        list_remove(task->wait_list, &task->node);
        task->wait_list = NULL;
        task->event_wait_bits = 0U;
        task->event_result_bits = 0U;
        task->event_wait_all = 0U;
        task->event_clear_on_exit = 0U;
        return;
    }

    if (task->state == TASK_READY) {
        remove_task_from_ready_queue(task);
    }
}

static void task_free_stack(TCB_t *task)
{
    if (task == NULL) {
        return;
    }

    if (task->dynamic_allocation != 0U &&
        task->mem.stack_alloc_base != 0U) {
        os_free((void *)task->mem.stack_alloc_base);
    }

    task->mem.stack_alloc_base = 0U;
    task->mem.stack_base = 0U;
    task->mem.stack_size = 0U;
    task->dynamic_allocation = 0U;
    task_clear_mpu(task);
    task->stack_ptr = NULL;
}

/*=====================================================================
 * VÒNG ĐỜI TIẾN TRÌNH: TIÊU DIỆT (KILL)
 * ======================================================================== */
void task_kill(uint32_t tid)
{
    uint32_t irq_state = os_enter_critical();

    if (tid >= MAX_TASKS ||
        tcb_table[tid].state == TASK_UNUSED ||
        tcb_table[tid].state == TASK_TERMINATED) {
        os_exit_critical(irq_state);
        return;
    }

    TCB_t *t = &tcb_table[tid];

    task_detach_from_current_list(t);
    t->wait_result = OS_ERROR;
    t->wait_has_timeout = 0;

    if (current_tcb != NULL && tid == current_tcb->tid) {
        MYOS_TRACE(OS_TRACE_TASK_DELETE, tid, 1U, 0U);
        t->state = TASK_TERMINATED;
        idle_add_terminated_task(t);

        os_schedule();

        os_exit_critical(irq_state);

        while (1) {
        }
    }

    task_free_stack(t);

    MYOS_TRACE(OS_TRACE_TASK_DELETE, tid, 0U, 0U);
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

    task_detach_from_current_list(t);
    t->wait_result = OS_ERROR;
    t->wait_has_timeout = 0;
    MYOS_TRACE(OS_TRACE_TASK_SUSPEND, tid, t->state, 0U);
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

    MYOS_TRACE(OS_TRACE_TASK_RESUME, tid, 0U, 0U);
    add_task_to_ready_queue(t);

    scheduler_yield_if_needed();

    os_exit_critical(irq_state);
}

os_status_t task_set_mpu_extra(uint32_t tid, const mpu_region_t *region)
{
    uint32_t irq_state = os_enter_critical();

    if (tid >= MAX_TASKS ||
        region == NULL ||
        tcb_table[tid].state == TASK_UNUSED ||
        tcb_table[tid].state == TASK_TERMINATED) {
        os_exit_critical(irq_state);
        return OS_ERROR;
    }

    TCB_t *task = &tcb_table[tid];
    task->mpu.extra_region = *region;

    if (current_tcb == task) {
        mpu_switch_task(&task->mpu);
    }

    os_exit_critical(irq_state);
    return OS_OK;
}

os_status_t task_clear_mpu_extra(uint32_t tid)
{
    mpu_region_t disabled = {
        .base = 0U,
        .size_bytes = 0U,
        .rasr = MPU_REGION_DISABLE,
    };

    return task_set_mpu_extra(tid, &disabled);
}


void task_yield(void)
{
    os_schedule();
}

void task_delay(uint32_t ticks)
{
    if (ticks == 0U) {
        return;
    }

    (void)task_block_current_on(&delay_list, TASK_BLOCKED, ticks);
}

void os_delay(uint32_t ticks)
{
    task_delay(ticks);
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

    if (timeout_ticks != OS_WAIT_FOREVER && timeout_ticks > INT32_MAX) {
        timeout_ticks = INT32_MAX;
    }

    current_tcb->wait_has_timeout = (timeout_ticks != OS_WAIT_FOREVER);
    current_tcb->state = wait_state;

    if (current_tcb->wait_has_timeout) {
        current_tcb->time.wakeup_tick = os_tick_count + timeout_ticks;
    }

    MYOS_TRACE(OS_TRACE_TASK_BLOCK,
               current_tcb->tid,
               (uint32_t)wait_state,
               timeout_ticks);

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
    TCB_t *task;

    if (wait_list == NULL) {
        return NULL;
    }

    task = task_find_highest_priority_waiter(wait_list);
    if (task == NULL) {
        return NULL;
    }

    list_remove(wait_list, &task->node);
    task->wait_list = NULL;
    task->wait_result = result;
    task->wait_has_timeout = 0;
    MYOS_TRACE(OS_TRACE_TASK_READY, task->tid, (uint32_t)result, 0U);
    add_task_to_ready_queue(task);

    return task;
}

static TCB_t *task_find_highest_priority_waiter(list_t *wait_list)
{
    list_node_t *node;
    TCB_t *best = NULL;

    if (wait_list == NULL) {
        return NULL;
    }

    list_for_each(node, wait_list) {
        TCB_t *candidate = list_entry(node, TCB_t, node);

        if (best == NULL ||
            candidate->sched.priority > best->sched.priority) {
            best = candidate;
        }
    }

    return best;
}

void task_process_timeouts(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        TCB_t *task = &tcb_table[i];

        if (task->wait_list != NULL &&
            task->wait_has_timeout &&
            tick_after_or_equal(os_tick_count, task->time.wakeup_tick)) {
            list_remove(task->wait_list, &task->node);
            task->wait_list = NULL;
            task->wait_result = OS_TIMEOUT;
            task->wait_has_timeout = 0;
            task->event_wait_bits = 0U;
            task->event_result_bits = 0U;
            task->event_wait_all = 0U;
            task->event_clear_on_exit = 0U;
            MYOS_TRACE(OS_TRACE_TASK_READY, task->tid, (uint32_t)OS_TIMEOUT, 1U);
            add_task_to_ready_queue(task);
        }
    }
}

void task_check_stack(TCB_t *task)
{
#if OS_ENABLE_STACK_CHECK
    uint32_t stack_low;
    uint32_t stack_high;
    uint32_t guard_bytes;
    uint32_t saved_sp;
    uint32_t *guard;

    if (task == NULL || task->mem.stack_base == 0 || task->mem.stack_size == 0) {
        return;
    }

    stack_low = task->mem.stack_base;
    stack_high = task->mem.stack_base + task->mem.stack_size;
    guard_bytes = OS_STACK_GUARD_WORDS * sizeof(uint32_t);
    saved_sp = (uint32_t)task->stack_ptr;

    if (saved_sp != 0U &&
        (saved_sp < (stack_low + guard_bytes) || saved_sp > stack_high)) {
        vApplicationStackOverflowHook(task);
        kernel_panic("stack pointer out of range", __FILE__, __LINE__);
    }

    guard = (uint32_t *)task->mem.stack_base;
    for (uint32_t i = 0; i < OS_STACK_GUARD_WORDS; i++) {
        if (guard[i] != STACK_CANARY_VALUE) {
            vApplicationStackOverflowHook(task);
            kernel_panic("stack overflow", __FILE__, __LINE__);
        }
    }
#else
    (void)task;
#endif
}

uint32_t task_get_stack_high_water_mark(TCB_t *task)
{
    uint32_t *stack;
    uint32_t words;
    uint32_t unused_words = 0U;

    if (task == NULL || task->mem.stack_base == 0 || task->mem.stack_size == 0) {
        return 0U;
    }

    stack = (uint32_t *)task->mem.stack_base;
    words = task->mem.stack_size / sizeof(uint32_t);

    for (uint32_t i = OS_STACK_GUARD_WORDS; i < words; i++) {
        if (stack[i] != STACK_FILL_VALUE) {
            break;
        }
        unused_words++;
    }

    return unused_words;
}

__attribute__((weak))
void vApplicationStackOverflowHook(TCB_t *task)
{
    if (task != NULL) {
        uart_print("Stack overflow in task ");
        uart_print_dec(task->tid);
        uart_print("\r\n");
    } else {
        uart_print("Stack overflow in unknown task\r\n");
    }
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

    if (task->state == TASK_UNUSED ||
        task->state == TASK_TERMINATED ||
        task->state == TASK_READY) {
        os_exit_critical(irq_state);
        return;
    }

    task_detach_from_current_list(task);
    task->wait_result = OS_ERROR;
    task->wait_has_timeout = 0;
    MYOS_TRACE(OS_TRACE_TASK_READY, tid, 0U, 2U);
    add_task_to_ready_queue(task);

    os_exit_critical(irq_state);
}

os_status_t os_task_get_info(uint32_t tid, os_task_info_t *info)
{
    uint32_t irq_state;
    TCB_t *task;

    if (tid >= MAX_TASKS || info == NULL) {
        return OS_ERROR;
    }

    irq_state = os_enter_critical();
    task = &tcb_table[tid];

    if (task->state == TASK_UNUSED) {
        os_exit_critical(irq_state);
        return OS_ERROR;
    }

    info->tid = task->tid;
    info->state = task->state;
    info->priority = task->sched.priority;
    info->stack_size = task->mem.stack_size;
    info->stack_free_words = task_get_stack_high_water_mark(task);
    info->stack_ok =
        (task->mem.stack_base != 0U &&
         *((uint32_t *)task->mem.stack_base) == STACK_CANARY_VALUE)
            ? 1U
            : 0U;

    os_exit_critical(irq_state);
    return OS_OK;
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

    if (task->state == TASK_UNUSED || task->state == TASK_TERMINATED) {
        os_exit_critical(irq_state);
        return;
    }

    task_detach_from_current_list(task);
    task->wait_result = OS_ERROR;
    task->wait_has_timeout = 0;

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

    task_detach_from_current_list(task);
    task->wait_result = OS_ERROR;
    task->wait_has_timeout = 0;
    MYOS_TRACE(OS_TRACE_TASK_BLOCK, tid, TASK_BLOCKED, 0U);

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

    task_detach_from_current_list(task);
    task->wait_result = OS_ERROR;
    task->wait_has_timeout = 0;
    MYOS_TRACE(OS_TRACE_TASK_SUSPEND, tid, task->state, 1U);

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

    task_detach_from_current_list(task);
    task->wait_result = OS_ERROR;
    task->wait_has_timeout = 0;

    MYOS_TRACE(OS_TRACE_TASK_DELETE, tid, 2U, 0U);
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

        task_free_stack(task);

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
#ifdef MYOS_DIAG_DEMO
    static uint8_t diag_demo_printed = 0U;
#endif

    while (1) {
        prvCleanupTerminatedTasks();

        vApplicationIdleHook();

#ifdef MYOS_DIAG_DEMO
        if (diag_demo_printed == 0U && os_tick_count >= 2000U) {
            diag_demo_printed = 1U;
            uart_print("\r\n[DIAG_DEMO] stats\r\n");
            runtime_stats_print();
            uart_print("\r\n[DIAG_DEMO] trace\r\n");
            os_trace_dump(24U);
        }
#endif

#if defined(OS_USE_TICKLESS_IDLE) && OS_USE_TICKLESS_IDLE == 1
        os_low_power_try_sleep();
#elif defined(OS_ENABLE_LOW_POWER) && OS_ENABLE_LOW_POWER == 1
        __asm volatile ("wfi" ::: "memory");
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

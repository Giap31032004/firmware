#ifndef MYOS_KERNEL_TASK_H
#define MYOS_KERNEL_TASK_H

#include <stdint.h>
#include "kernel.h"
#include "kernel_config.h"
#include"list.h"
#include "scheduler.h"
#include "heap.h"
#include "tick.h"
#include "memory_layout.h"

/* ================= TASK STATE ================= */

typedef enum {
    TASK_UNUSED = 0,
    TASK_NEW,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SUSPENDED,
    TASK_TERMINATED
} task_state_t;

typedef struct {
    uint32_t tid;
    task_state_t state;
    uint8_t priority;
    uint32_t stack_size;
    uint32_t stack_free_words;
    uint8_t stack_ok;
} os_task_info_t;

/* ================= PCB / TCB ================= */

typedef struct TCB {
    /* context switch */
    uint32_t *stack_ptr;

    /* task identity */
    uint32_t tid;
    void (*entry)(void);
    task_state_t state;

    /*
     * Intrusive list linkage.
     * This node is used by ready list, blocked list, wait list,
     * timer list, terminated list, etc.
     */
    list_node_t node;

    /* subsystem-owned semantic data */
    scheduler_info_t sched;
    memory_info_t mem;
    task_mpu_t mpu;
    timer_info_t time;

    list_t *wait_list;
    os_status_t wait_result;
    uint8_t wait_has_timeout;
    uint32_t event_wait_bits;
    uint32_t event_result_bits;
    uint8_t event_wait_all;
    uint8_t event_clear_on_exit;
    uint32_t mutexes_held_count;
    list_t held_mutexes;

#if defined(OS_GENERATE_RUN_TIME_STATS) && OS_GENERATE_RUN_TIME_STATS == 1
    uint64_t runtime_cycles;
    uint32_t runtime_last_start;
#endif

    /* allocation policy */
    uint8_t dynamic_allocation;
} TCB_t;

/* ================= GLOBAL TASK TABLE ================= */

#define TASK_INVALID_TID UINT32_MAX
#define OS_TASK_STACK_ALIGNED(stack_words) \
    __attribute__((aligned((stack_words) * sizeof(uint32_t))))

extern TCB_t tcb_table[MAX_TASKS];

/* ================= TASK CREATION / LIFECYCLE ================= */

void task_init(void);
os_status_t task_create_dynamic(void (*func)(void),
                                uint8_t priority,
                                uint32_t stack_words,
                                uint32_t *out_tid);
os_status_t task_create_static(void (*func)(void),
                               uint8_t priority,
                               uint32_t *stack_buffer,
                               uint32_t stack_words,
                               uint32_t *out_tid);

#define task_create(func, priority) \
    task_create_dynamic((func), (priority), OS_DEFAULT_STACK_WORDS, NULL)

void task_kill(uint32_t tid);
void task_exit(void);
void task_yield(void);
void task_delay(uint32_t ticks);
void task_suspend(uint32_t tid);
void task_resume(uint32_t tid);
os_status_t task_set_mpu_extra(uint32_t tid, const mpu_region_t *region);
os_status_t task_clear_mpu_extra(uint32_t tid);

#define os_task_kill(tid)       task_kill((tid))
#define os_task_suspend(tid)    task_suspend((tid))
#define os_task_resume(tid)     task_resume((tid))

/* ================= TASK STATE MANAGEMENT ================= */

void task_set_state(uint32_t tid, task_state_t state);
os_status_t os_task_get_info(uint32_t tid, os_task_info_t *info);

void task_set_ready(uint32_t tid);
void task_set_running(uint32_t tid);
void task_set_blocked(uint32_t tid);
void task_set_suspended(uint32_t tid);
void task_set_terminated(uint32_t tid);
os_status_t task_block_current_on(list_t *wait_list,
                                  task_state_t wait_state,
                                  uint32_t timeout_ticks);
TCB_t *task_wake_one(list_t *wait_list, os_status_t result);
void task_process_timeouts(void);
void task_check_stack(TCB_t *task);
void task_check_all_stacks(void);
uint32_t task_get_stack_high_water_mark(TCB_t *task);
void vApplicationStackOverflowHook(TCB_t *task);

/* ================= IDLE / CLEANUP ================= */

void idle_add_terminated_task(TCB_t *task);
void prvIdleTask(void);

#endif /* MYOS_KERNEL_TASK_H */

#ifndef MYOS_KERNEL_TASK_H
#define MYOS_KERNEL_TASK_H

#include <stdint.h>
#include "kernel.h"
#include "kernel_config.h"
#include "list.h"
#include "scheduler.h"
#include "heap.h"
#include "tick.h"
#include "banker.h"

/* ================= TASK STATE ================= */

typedef enum {
    TASK_UNUSED = 0,
    TASK_NEW,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_WAITING_TIME,
    TASK_WAITING_OBJECT,
    TASK_WAITING_IO,
    TASK_SUSPENDED,
    TASK_TERMINATED
} task_state_t;

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
    timer_info_t time;
    resource_info_t res;

    list_t *wait_list;
    os_status_t wait_result;
    uint8_t wait_has_timeout;

    /* allocation policy */
    uint8_t dynamic_allocation;
} TCB_t;

/* ================= GLOBAL TASK TABLE ================= */

extern TCB_t tcb_table[MAX_TASKS];

/* ================= TASK CREATION / LIFECYCLE ================= */

void task_init(void);
void task_create(void (*func)(void), uint8_t priority);

void task_kill(uint32_t tid);
void task_exit(void);
void task_yield(void);
void task_delay(uint32_t ticks);
void task_suspend(uint32_t tid);
void task_resume(uint32_t tid);

#define os_task_kill(tid)       task_kill((tid))
#define os_task_suspend(tid)    task_suspend((tid))
#define os_task_resume(tid)     task_resume((tid))

/* ================= TASK STATE MANAGEMENT ================= */

void task_set_state(uint32_t tid, task_state_t state);

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

/* ================= IDLE / CLEANUP ================= */

void idle_add_terminated_task(TCB_t *task);
void prvIdleTask(void);

#endif /* MYOS_KERNEL_TASK_H */

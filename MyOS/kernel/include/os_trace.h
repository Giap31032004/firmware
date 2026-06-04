#ifndef MYOS_KERNEL_OS_TRACE_H
#define MYOS_KERNEL_OS_TRACE_H

#include <stdint.h>
#include "kernel_config.h"

typedef enum {
    OS_TRACE_TASK_CREATE = 1,
    OS_TRACE_TASK_DELETE,
    OS_TRACE_TASK_READY,
    OS_TRACE_TASK_BLOCK,
    OS_TRACE_TASK_SUSPEND,
    OS_TRACE_TASK_RESUME,
    OS_TRACE_TASK_SWITCHED_OUT,
    OS_TRACE_TASK_SWITCHED_IN,
    OS_TRACE_SEM_WAIT,
    OS_TRACE_SEM_SIGNAL,
    OS_TRACE_MUTEX_WAIT,
    OS_TRACE_MUTEX_LOCK,
    OS_TRACE_MUTEX_UNLOCK,
    OS_TRACE_QUEUE_SEND,
    OS_TRACE_QUEUE_RECEIVE,
    OS_TRACE_LOW_POWER_BEGIN,
    OS_TRACE_LOW_POWER_END,
    OS_TRACE_TICK_STEP
} os_trace_event_id_t;

typedef struct {
    uint32_t time;
    uint32_t tick;
    uint16_t event;
    uint16_t tid;
    uint32_t arg0;
    uint32_t arg1;
} os_trace_event_t;

void os_trace_init(void);
void os_trace_record(os_trace_event_id_t event, uint32_t tid, uint32_t arg0, uint32_t arg1);
void os_trace_dump(uint32_t max_events);
void os_trace_clear(void);

#if defined(OS_USE_TRACE_FACILITY) && OS_USE_TRACE_FACILITY == 1
#define MYOS_TRACE(event, tid, arg0, arg1) \
    os_trace_record((event), (tid), (arg0), (arg1))
#else
#define MYOS_TRACE(event, tid, arg0, arg1) ((void)0)
#endif

#endif /* MYOS_KERNEL_OS_TRACE_H */

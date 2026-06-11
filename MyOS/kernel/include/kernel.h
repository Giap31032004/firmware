#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include "critical.h"

#define OS_ENTER_CRITICAL() uint32_t __os_irq_state = os_enter_critical()
#define OS_EXIT_CRITICAL() os_exit_critical(__os_irq_state)
#define KERNEL_LOG(msg) uart_print(msg)
#define OS_WAIT_FOREVER UINT32_MAX

typedef enum {
    OS_OK = 0,
    OS_TIMEOUT = -1,
    OS_ERROR = -2
} os_status_t;

#if defined(OS_ENABLE_ASSERT) && OS_ENABLE_ASSERT
#define KASSERT(expr) \
    do { \
        if (!(expr)) { \
            kernel_panic("assert", __FILE__, __LINE__); \
        } \
    } while (0)
#else
#define KASSERT(expr) ((void)0)
#endif

void kernel_init(void);
void os_delay(uint32_t ticks);
void kernel_panic(const char *reason, const char *file, int line);

void uart_print(const char *s);

#include "tick.h"
#include "timer.h"

#endif /* KERNEL_H */

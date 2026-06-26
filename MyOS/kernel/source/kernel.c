#include "kernel.h"
#include "scheduler.h"
#include "task.h"
#include "heap.h"
#include "port.h"
#include "os_trace.h"
#include "runtime_stats.h"

/* ========================================================================
 * KHỞI TẠO KERNEL
 * ======================================================================== */
void kernel_init(void) {
    KERNEL_LOG("Booting MyOS Kernel...\r\n");

    runtime_stats_init();
    os_trace_init();
    scheduler_init();
    task_init();
    tick_init();
    timer_init();
    memory_init();

    if (task_create(prvIdleTask, PRIORITY_IDLE) != OS_OK) {
        kernel_panic("idle task create failed", __FILE__, __LINE__);
    }
    KERNEL_LOG("Kernel Initialized. [OK]\r\n");
}

void kernel_panic(const char *reason, const char *file, int line)
{
    port_disable_irq();

    os_log_write("\r\n[KERNEL PANIC] ");
    os_log_write(reason != NULL ? reason : "unknown");
    os_log_write(" at ");
    os_log_write(file != NULL ? file : "unknown");
    os_log_write(":");
    os_log_write_dec((uint32_t)line);
    os_log_write("\r\n");

    if (current_tcb != NULL) {
        os_log_write("Current TID: ");
        os_log_write_dec(current_tcb->tid);
        os_log_write("\r\n");
    }

    while (1) {
    }
}

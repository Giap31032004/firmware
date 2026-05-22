#include "kernel.h"
#include "uart.h"
#include "scheduler.h"
#include "task.h"
#include "heap.h"
#include "banker.h"
#include "port.h"

// Cấu trúc một bản ghi log đơn giản
typedef struct {
    uint32_t pid_in;
    uint8_t priority;
} sched_log_t;

#define LOG_BUFFER_SIZE 16
sched_log_t log_buffer[LOG_BUFFER_SIZE];
volatile uint8_t log_head = 0; // Thư ký ghi vào đây
volatile uint8_t log_tail = 0; // Task in ra từ đây

// Hàm đẩy log cực nhanh (Dùng trong Kernel)
void push_sched_log(uint32_t pid, uint8_t prio) {
    uint8_t next_head = (log_head + 1) % LOG_BUFFER_SIZE;
    if (next_head != log_tail) { // Nếu bộ đệm chưa đầy
        log_buffer[log_head].pid_in = pid;
        log_buffer[log_head].priority = prio;
        log_head = next_head;
    }
}

/* ========================================================================
 * KHỞI TẠO KERNEL
 * ======================================================================== */
void kernel_init(void) {
    KERNEL_LOG("Booting MyOS Kernel...\r\n");

    os_tick_count = 0;
    scheduler_init_queues();
    task_init();
    timer_init();
    memory_init();
    banker_init();

    task_create(prvIdleTask, PRIORITY_IDLE);
    KERNEL_LOG("Kernel Initialized. [OK]\r\n");
}

void kernel_panic(const char *reason, const char *file, int line)
{
    port_disable_irq();

    uart_print("\r\n[KERNEL PANIC] ");
    uart_print(reason != NULL ? reason : "unknown");
    uart_print(" at ");
    uart_print(file != NULL ? file : "unknown");
    uart_print(":");
    uart_print_dec(line);
    uart_print("\r\n");

    if (current_tcb != NULL) {
        uart_print("Current TID: ");
        uart_print_dec(current_tcb->tid);
        uart_print("\r\n");
    }

    while (1) {
    }
}

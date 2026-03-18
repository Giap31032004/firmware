#include "task.h"
#include "uart.h"

/* TASK 4: LOGGER */
void task_logger(void) {
    int counter = 0;
    while (1) {
        os_delay(10); 
        mutex_lock(&app_mutex);
        uart_print("    >>> [LOGGER] Checking system... Count: ");
        uart_print_dec(counter++);
        uart_print("\r\n");
        mutex_unlock(&app_mutex);
    }
}

/* TASK 5: SHELL */
static int parse_pid(char *buffer, int offset) {
    if (buffer[offset] >= '0' && buffer[offset] <= '9') {
        return buffer[offset] - '0';
    }
    return -1;
}

void task_shell(void) {
    char cmd_buffer[32];
    int cmd_index = 0;

    /* Driver UART mới đã thread-safe, không cần app_mutex ở đây */
    uart_print("\r\n[SHELL] Ready. Type 'help' for commands.\r\n");
    uart_print("MyOS> ");

    while (1) {
        /* 1. Nhận ký tự (Hàm này sẽ ngủ nếu không có dữ liệu) */
        char c = uart_getc();

        /* 2. Echo lại màn hình để người dùng thấy mình gõ gì */
        uart_putc(c);

        /* 3. Xử lý khi nhấn Enter */
        if (c == '\r') {
            uart_print("\n");
            cmd_buffer[cmd_index] = '\0'; // Kết thúc chuỗi

            /* --- BẮT ĐẦU XỬ LÝ LỆNH --- */
            
            // Lệnh: help
            if (my_strcmp(cmd_buffer, "help") == 0) {
                uart_print("Available commands:\r\n");
                uart_print("  help      : Show this help\r\n");
                uart_print("  reboot    : Restart system\r\n");
                uart_print("  ps        : List tasks (Coming soon)\r\n");
                uart_print("  kill <id> : Kill a task (e.g., kill 1)\r\n");
                uart_print("  stop <id> : Suspend a task\r\n");
                uart_print("  start <id>: Resume a task\r\n");
            } 
            // Lệnh: reboot
            else if (my_strcmp(cmd_buffer, "reboot") == 0) {
                uart_print("Rebooting...\r\n");
                // Reset Cortex-M3 thông qua AIRCR
                *(volatile uint32_t*)0xE000ED0C = 0x05FA0004;
            }
            // Lệnh: kill <pid> (So sánh 5 ký tự đầu)
            else if (my_strncmp(cmd_buffer, "kill ", 5) == 0) {
                int pid = parse_pid(cmd_buffer, 5);
                if (pid >= 0) {
                    uart_print("Command: Kill PID ");
                    uart_print_dec(pid);
                    uart_print("\r\n");
                    os_task_kill(pid); // <--- GỌI KERNEL
                } else {
                    uart_print("Error: Invalid PID.\r\n");
                }
            }
            // Lệnh: stop <pid> (Suspend)
            else if (my_strncmp(cmd_buffer, "stop ", 5) == 0) {
                int pid = parse_pid(cmd_buffer, 5);
                if (pid >= 0) {
                    uart_print("Command: Suspend PID ");
                    uart_print_dec(pid);
                    uart_print("\r\n");
                    os_task_suspend(pid); // <--- GỌI KERNEL
                }
            }
            // Lệnh: start <pid> (Resume)
            else if (my_strncmp(cmd_buffer, "start ", 6) == 0) {
                int pid = parse_pid(cmd_buffer, 6);
                if (pid >= 0) {
                    uart_print("Command: Resume PID ");
                    uart_print_dec(pid);
                    uart_print("\r\n");
                    os_task_resume(pid); // <--- GỌI KERNEL
                }
            }
            // Lệnh rỗng (chỉ nhấn Enter)
            else if (cmd_index == 0) {
                // Không làm gì cả
            }
            // Lệnh lạ
            else {
                uart_print("Unknown command: ");
                uart_print(cmd_buffer);
                uart_print("\r\n");
            }

            /* --- KẾT THÚC XỬ LÝ LỆNH --- */

            uart_print("MyOS> ");
            cmd_index = 0; // Reset buffer
        } 
        /* 4. Xử lý Backspace (Xóa ký tự) - Tùy chọn cho xịn */
        else if (c == '\b' || c == 127) { 
            if (cmd_index > 0) {
                cmd_index--;
                uart_print(" \b"); // Xóa ký tự trên màn hình
            }
        }
        /* 5. Lưu ký tự vào buffer */
        else {
            if (cmd_index < 31) {
                cmd_buffer[cmd_index++] = c;
            } else {
                uart_print("\r\n[SHELL] Buffer overflow!\r\nMyOS> ");
                cmd_index = 0;
            }
        }
    }
}
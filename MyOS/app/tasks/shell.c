#include "app_global.h"
#include "kernel.h"
#include "task.h"
#include "uart.h"
#include "utils.h"

static int parse_tid(char *buffer, int offset)
{
    int value = 0;
    int parsed = 0;

    while (buffer[offset] >= '0' && buffer[offset] <= '9') {
        value = value * 10 + (buffer[offset] - '0');
        offset++;
        parsed = 1;
    }

    return parsed ? value : -1;
}

static void shell_print_help(void)
{
    uart_print("Available commands:\r\n");
    uart_print("  help        : Show this help\r\n");
    uart_print("  ps          : List tasks\r\n");
    uart_print("  status      : Print app counters\r\n");
    uart_print("  stacks      : Check task stack canaries\r\n");
    uart_print("  signal      : Signal heartbeat semaphore\r\n");
    uart_print("  kill <id>   : Kill a task\r\n");
    uart_print("  stop <id>   : Suspend a task\r\n");
    uart_print("  start <id>  : Resume a task\r\n");
    uart_print("  reboot      : Restart system\r\n");
}

void task_shell(void)
{
    char cmd_buffer[40];
    int cmd_index = 0;

    uart_print("\r\n[SHELL] Ready. Type 'help' for commands.\r\n");
    uart_print("MyOS> ");

    while (1) {
        char c = uart_getc();
        uart_putc(c);

        if (c == '\r') {
            uart_print("\n");
            cmd_buffer[cmd_index] = '\0';

            if (my_strcmp(cmd_buffer, "help") == 0) {
                shell_print_help();
            } else if (my_strcmp(cmd_buffer, "ps") == 0) {
                app_print_task_table();
            } else if (my_strcmp(cmd_buffer, "status") == 0) {
                app_print_system_status();
            } else if (my_strcmp(cmd_buffer, "stacks") == 0) {
                task_check_all_stacks();
                app_print_line("[STACK] All active task canaries are intact.");
            } else if (my_strcmp(cmd_buffer, "signal") == 0) {
                sem_signal(&heartbeat_sem);
                app_print_line("[SHELL] Heartbeat semaphore signaled.");
            } else if (my_strcmp(cmd_buffer, "reboot") == 0) {
                uart_print("Rebooting...\r\n");
                *(volatile uint32_t *)0xE000ED0C = 0x05FA0004;
            } else if (my_strncmp(cmd_buffer, "kill ", 5) == 0) {
                int tid = parse_tid(cmd_buffer, 5);
                if (tid >= 0) {
                    task_kill((uint32_t)tid);
                }
            } else if (my_strncmp(cmd_buffer, "stop ", 5) == 0) {
                int tid = parse_tid(cmd_buffer, 5);
                if (tid >= 0) {
                    task_suspend((uint32_t)tid);
                }
            } else if (my_strncmp(cmd_buffer, "start ", 6) == 0) {
                int tid = parse_tid(cmd_buffer, 6);
                if (tid >= 0) {
                    task_resume((uint32_t)tid);
                }
            } else if (cmd_index != 0) {
                uart_print("Unknown command: ");
                uart_print(cmd_buffer);
                uart_print("\r\n");
            }

            uart_print("MyOS> ");
            cmd_index = 0;
        } else if (c == '\b' || c == 127) {
            if (cmd_index > 0) {
                cmd_index--;
                uart_print(" \b");
            }
        } else if (cmd_index < ((int)sizeof(cmd_buffer) - 1)) {
            cmd_buffer[cmd_index++] = c;
        } else {
            uart_print("\r\n[SHELL] Buffer overflow.\r\nMyOS> ");
            cmd_index = 0;
        }
    }
}

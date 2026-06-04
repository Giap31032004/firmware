#include "app_global.h"
#include "kernel.h"
#include "os_trace.h"
#include "port.h"
#include "runtime_stats.h"
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
    uart_print("  stats       : Show per-task CPU usage\r\n");
    uart_print("  trace       : Dump recent RTOS trace events\r\n");
    uart_print("  traceclear  : Clear RTOS trace buffer\r\n");
    uart_print("  status      : Print app counters\r\n");
    uart_print("  thermal     : Print thermal snapshot\r\n");
    uart_print("  heap        : Show heap usage\r\n");
    uart_print("  queues      : Show queue fill levels\r\n");
    uart_print("  power       : Show tickless idle status\r\n");
    uart_print("  events      : Show app event-group bits\r\n");
    uart_print("  demo        : Show demo flow\r\n");
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
            } else if (my_strcmp(cmd_buffer, "stats") == 0) {
                runtime_stats_print();
            } else if (my_strcmp(cmd_buffer, "trace") == 0) {
                os_trace_dump(40U);
            } else if (my_strcmp(cmd_buffer, "traceclear") == 0) {
                os_trace_clear();
                uart_print("[TRACE] cleared\r\n");
            } else if (my_strcmp(cmd_buffer, "status") == 0) {
                app_print_system_status();
            } else if (my_strcmp(cmd_buffer, "thermal") == 0) {
                app_print_system_status();
            } else if (my_strcmp(cmd_buffer, "heap") == 0) {
                app_print_heap_status();
            } else if (my_strcmp(cmd_buffer, "queues") == 0) {
                app_print_queue_status();
            } else if (my_strcmp(cmd_buffer, "power") == 0) {
                app_print_power_status();
            } else if (my_strcmp(cmd_buffer, "events") == 0) {
                app_print_event_status();
            } else if (my_strcmp(cmd_buffer, "demo") == 0) {
                app_print_demo_summary();
            } else if (my_strcmp(cmd_buffer, "stacks") == 0) {
                task_check_all_stacks();
                app_print_line("[STACK] All active task canaries are intact.");
            } else if (my_strcmp(cmd_buffer, "signal") == 0) {
                sem_signal(&heartbeat_sem);
                app_print_line("[SHELL] Heartbeat semaphore signaled.");
            } else if (my_strcmp(cmd_buffer, "reboot") == 0) {
                uart_print("Rebooting...\r\n");
                port_system_reset();
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

#ifndef APP_GLOBAL_H
#define APP_GLOBAL_H

#include <stdint.h>
#include "ipc.h"
#include "sync.h"
#include "task.h"

#define APP_TEMP_ALARM_THRESHOLD 40

extern os_msg_queue_t temp_queue;

extern os_mutex_t app_mutex;
extern os_mutex_t mutex_A;
extern os_mutex_t mutex_B;
extern os_sem_t alarm_sem;
extern os_sem_t heartbeat_sem;

extern volatile int current_temperature;
extern volatile int system_uptime;
extern volatile uint32_t sensor_samples;
extern volatile uint32_t display_updates;
extern volatile uint32_t alarm_events;
extern volatile uint32_t resource_cycles;

void service_init(void);
void app_init(void);
void app_print_line(const char *s);
void app_print_task_table(void);
void app_print_system_status(void);

#endif

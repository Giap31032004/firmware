#ifndef APP_GLOBAL_H
#define APP_GLOBAL_H

#include <stdint.h>
#include "event_group.h"
#include "ipc.h"
#include "sync.h"
#include "task.h"

#define APP_TEMP_WARN_THRESHOLD      40
#define APP_TEMP_CRITICAL_THRESHOLD  50
#define APP_TEMP_SAMPLE_PERIOD       25
#define APP_DISPLAY_PERIOD_TICKS     100
#define APP_LOG_SAMPLE_INTERVAL      8U

#define APP_EVENT_SENSOR_SAMPLE      (1U << 0)
#define APP_EVENT_FILTER_UPDATE      (1U << 1)
#define APP_EVENT_CONTROL_UPDATE     (1U << 2)
#define APP_EVENT_ALARM_ACTIVE       (1U << 3)
#define APP_EVENT_HEARTBEAT          (1U << 4)

typedef enum {
    TEMP_ZONE_NORMAL = 0,
    TEMP_ZONE_WARN,
    TEMP_ZONE_CRITICAL
} temp_zone_t;

extern os_msg_queue_t temp_raw_queue;
extern os_msg_queue_t temp_filtered_queue;
extern os_msg_queue_t temp_display_queue;
extern os_msg_queue_t temp_log_queue;

extern os_mutex_t app_mutex;
extern os_mutex_t temp_state_mutex;
extern os_mutex_t storage_mutex;
extern os_sem_t alarm_sem;
extern os_sem_t heartbeat_sem;
extern os_event_group_t app_events;

extern volatile int current_temperature;
extern volatile int filtered_temperature;
extern volatile int min_temperature;
extern volatile int max_temperature;
extern volatile int fan_pwm_percent;
extern volatile temp_zone_t temp_zone;
extern volatile int system_uptime;
extern volatile uint32_t sensor_samples;
extern volatile uint32_t filter_samples;
extern volatile uint32_t display_updates;
extern volatile uint32_t alarm_events;
extern volatile uint32_t log_records;
extern volatile uint32_t storage_cycles;
extern volatile uint32_t control_cycles;

typedef struct {
    int raw_temperature;
    int filtered_temperature;
    int min_temperature;
    int max_temperature;
    int fan_pwm_percent;
    temp_zone_t zone;
    uint32_t uptime_ticks;
    uint32_t sensor_samples;
    uint32_t filter_samples;
    uint32_t display_updates;
    uint32_t alarm_events;
    uint32_t log_records;
    uint32_t storage_cycles;
    uint32_t control_cycles;
} app_thermal_snapshot_t;

void service_init(void);
void app_init(void);
os_status_t app_get_thermal_snapshot(app_thermal_snapshot_t *snapshot,
                                     uint32_t timeout_ticks);
void app_print_line(const char *s);
void app_print_task_table(void);
void app_print_system_status(void);
void app_print_queue_status(void);
void app_print_heap_status(void);
void app_print_power_status(void);
void app_print_event_status(void);
void app_print_demo_summary(void);
const char *app_temp_zone_str(temp_zone_t zone);

#endif

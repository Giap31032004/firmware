#ifndef APP_GLOBAL_H
#define APP_GLOBAL_H

#include <stdint.h>
#include "ipc.h"
#include "sync.h"

#define APP_TEMP_WARN_THRESHOLD      40
#define APP_TEMP_CRITICAL_THRESHOLD  50
#define APP_TEMP_SAMPLE_PERIOD       25U
#define APP_LOG_PERIOD_TICKS         200U

typedef enum {
    TEMP_ZONE_NORMAL = 0,
    TEMP_ZONE_WARN,
    TEMP_ZONE_CRITICAL
} temp_zone_t;

typedef struct {
    int raw_temperature;
    int filtered_temperature;
    int min_temperature;
    int max_temperature;
    int fan_pwm_percent;
    temp_zone_t zone;
    uint32_t uptime_ticks;
    uint32_t samples;
    uint32_t alarm_events;
} app_thermal_snapshot_t;

extern os_msg_queue_t temp_queue;
extern os_mutex_t app_mutex;

void service_init(void);
void app_init(void);
void app_record_sample(int raw, int filtered);
void app_record_control(temp_zone_t zone, int fan_pwm_percent);
os_status_t app_get_thermal_snapshot(app_thermal_snapshot_t *snapshot,
                                     uint32_t timeout_ticks);
void app_print_line(const char *s);
void app_print_task_table(void);
void app_print_system_status(void);
void app_print_queue_status(void);
void app_print_heap_status(void);
void app_print_power_status(void);
void app_print_demo_summary(void);
const char *app_temp_zone_str(temp_zone_t zone);

#endif

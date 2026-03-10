#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "kernel.h"
#include "sync.h"   
#include "ipc.h"

/* ================================================= */
/* KHAI BÁO EXTERN (Cầu nối cho main.c nhìn thấy)    */
/* ================================================= */
extern volatile int current_temperature;
extern volatile int system_uptime;

extern os_msg_queue_t temp_queue; // <-- Dòng này giúp main.c hiểu temp_queue là gì
extern os_mutex_t app_mutex;      // <-- Dòng này giúp main.c hiểu app_mutex là gì
extern os_mutex_t mutex_A;
extern os_mutex_t mutex_B;

/* ================================================= */
/* PROTOTYPES CÁC TASK                               */
/* ================================================= */
void task_sensor_update(void);
void task_display(void);
void task_alarm(void);
void task_logger(void);
void task_shell(void);

void task_deadlock_1(void);
void task_deadlock_2(void);
void task_banker1(void);
void task_banker2(void);

void task_gpio_blink(void);
void task_i2c_scanner(void);
void task_dma_test(void);

#endif // TASK_H
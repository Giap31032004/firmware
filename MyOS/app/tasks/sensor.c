#include "task.h"
#include "uart.h"

/* TASK 1: SENSOR */
void task_sensor_update(void) {
    int local_temp = 25; 
    int direction = 1; 
    while (1) {
        os_delay(10); 
        
        if(direction == 1){
            local_temp += 5;
            if(local_temp >= 55) direction = -1;
        } else {
            local_temp -= 5;
            if(local_temp <= 20) direction = 1;
        }
        msg_queue_send(&temp_queue, local_temp);
    }
}

/* TASK 2: DISPLAY */
void task_display(void) {
    int received_temp; 

    while (1) {
        received_temp = msg_queue_receive(&temp_queue);
        current_temperature = received_temp; // Cập nhật global

        mutex_lock(&app_mutex);
            uart_print("----------------------\r\n");
            uart_print("| Temp: "); 
            uart_print_dec(received_temp); 
            uart_print(" C         |\r\n");
            uart_print("----------------------\r\n");
        mutex_unlock(&app_mutex);
    }
}

/* TASK 3: ALARM */
void task_alarm(void) {
    int alarm_active = 0; 

    while (1) {
        os_delay(5);
        
        mutex_lock(&app_mutex);
        if (current_temperature > 40) {
            if (alarm_active == 0) {
                uart_print("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
                uart_print("\r\n!!! [ALARM] WARNING: OVERHEAT !!!\r\n");
                uart_print("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
                alarm_active = 1; 
            }
        } 
        else {
            if (alarm_active == 1) {
                uart_print("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
                uart_print("\r\n[ALARM] Temperature Normal.\r\n");                
                uart_print("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
                alarm_active = 0; 
            }
        }
        mutex_unlock(&app_mutex);
    }
}
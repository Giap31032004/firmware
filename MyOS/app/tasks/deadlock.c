#include "task.h"
#include "uart.h"
#include "banker.h"

void task_deadlock_1(void){
    while(1){
        mutex_lock(&mutex_A);
        uart_print("Task 1: Got A. Waitting for B ... \r\n");
        os_delay(10);
        mutex_lock(&mutex_B);
        uart_print("Task 1: Got both!\r\n");
        mutex_unlock(&mutex_B);
        mutex_unlock(&mutex_A);
    }
}

void task_deadlock_2(void){
    while(1){
        mutex_lock(&mutex_B);
        uart_print("Task 2: Got B. Waitting for A ... \r\n");
        os_delay(10);
        mutex_lock(&mutex_A);
        uart_print("Task 2: Got both!\r\n");
        mutex_unlock(&mutex_A);
        mutex_unlock(&mutex_B);
    }
}

void task_banker1(void){
    int req[] = {0, 0, 1}; 
    while(1){
        uart_print(" T1 : Asking for 1 DMA ...\r\n");
        if(request_resources(req)){
            uart_print("T1 : granted 1 DMA ! Holding it .... \r\n");
            os_delay(100);
            uart_print("T1 : Releasing DMA. \r\n");
            release_resources(req);
        } else{
            uart_print("T1 denied. \r\n");
        }
        os_delay(20);
    }
}

void task_banker2(void) {
    int req[] = {0, 0, 1}; 
    while(1) {
        os_delay(10); 
        uart_print("T2: Asking for 1 DMA...\r\n");
        if (request_resources(req)) {
            uart_print("T2: GRANTED! (Strange?)\r\n");
            release_resources(req);
        } else {
            uart_print("T2: DENIED by Banker (Unsafe State)!\r\n");
        }
        os_delay(100);
    }
}
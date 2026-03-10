#include "kernel.h"

#define SCB_ICSR (*(volatile uint32_t*)0xE000ED04)
#define PENDSVSET_BIT (1UL << 28)

volatile uint32_t tick_count = 0;

void os_delay(uint32_t ticks) {
    OS_ENTER_CRITICAL();
    current_pcb->wake_up_tick = tick_count + ticks;
    current_pcb->state = PROC_WAITING_TIME;
    OS_EXIT_CRITICAL();

    process_schedule(); 
    SCB_ICSR |= PENDSVSET_BIT;
}

void process_timer_tick(void) {
    tick_count++;
    int need_schedule = 0; 

    /* 1. Quét danh sách Task để đánh thức (Wake up) */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB_t *p = &pcb_table[i];
        
        // [QUAN TRỌNG] Chỉ kiểm tra những ông đang ngủ (WAITING)
        // Nếu Task đã bị KILL (TERMINATED), nó sẽ không lọt vào if này -> Zombie bị chặn đứng!
        if (p->state == PROC_WAITING_TIME) {
            
            if (p->wake_up_tick <= tick_count) {
                p->state = PROC_READY;
                p->wake_up_tick = 0;
                add_task_to_ready_queue(p);
                
                // Nếu Task vừa dậy có quyền lực cao hơn Task đang chạy -> Cướp quyền ngay
                if (current_pcb && p->dynamic_priority > current_pcb->dynamic_priority) {
                    need_schedule = 1;
                }
            }
        }
    }

    /* 2. Xử lý Time Slice (Round-Robin cho các Task cùng độ ưu tiên) */
    if (current_pcb != NULL && current_pcb->state == PROC_RUNNING) {
        if (current_pcb->time_slice > 0) {
            current_pcb->time_slice--;
        }
        
        // Hết lượt chạy -> Nhường ghế
        if (current_pcb->time_slice == 0) {
            current_pcb->time_slice = 5; // Reset Time Slice (Ví dụ 5 tick)
            need_schedule = 1;
        }
    }

    /* 3. Nếu cần đổi Task thì kích hoạt PendSV */
    if (need_schedule) {
        SCB_ICSR |= PENDSVSET_BIT;
    }
}
#include "kernel.h"
#include "queue.h"
#include "uart.h"
#include "mpu.h"

#define SCB_ICSR (*(volatile uint32_t*)0xE000ED04)
#define PENDSVSET_BIT (1UL << 28)

queue_t ready_queue[MAX_PRIORITY];
uint32_t top_ready_priority_bitmap = 0;

PCB_t *current_pcb = NULL;
PCB_t *next_pcb = NULL;


extern void start_first_task(uint32_t *first_sp); 

// Hàm thêm Task vào hàng đợi Ready Priority 
void add_task_to_ready_queue(PCB_t *p) {

    if (p->state == PROC_TERMINATED || p->state == PROC_SUSPENDED) {
        return; 
    }

    uint8_t prio = p->dynamic_priority;
    if(prio >= MAX_PRIORITY) prio = MAX_PRIORITY - 1;

    queue_enqueue(&ready_queue[prio], p);
    top_ready_priority_bitmap |= (1UL << prio);
}

PCB_t* get_highest_priority_ready_task() {
    if (top_ready_priority_bitmap == 0) return NULL;
    
    // Tối ưu dùng __builtin_clz
    int highest_prio = 31 - __builtin_clz(top_ready_priority_bitmap);
    if (highest_prio >= MAX_PRIORITY) highest_prio = MAX_PRIORITY - 1;

    PCB_t *p = queue_dequeue(&ready_queue[highest_prio]);
    
    if(queue_is_empty(&ready_queue[highest_prio])) {
        top_ready_priority_bitmap &= ~(1UL << highest_prio);
    }
    uart_print("Selected process use priority ");
    uart_print_dec(highest_prio); // Cần thêm hàm này vào uart.h
    uart_print("\r\n");
    return p;
}

void process_schedule(void) {
    OS_ENTER_CRITICAL();
    PCB_t *pnext = NULL;
    
    while (1) {
        if (top_ready_priority_bitmap == 0) {
            if (current_pcb && current_pcb->state == PROC_RUNNING) {
                // Task hiện tại vẫn chạy tốt -> Giữ nguyên nó
                OS_EXIT_CRITICAL();
                return; 
            }
            OS_EXIT_CRITICAL();
            return;
        }

        pnext = get_highest_priority_ready_task();
        
        if (pnext != NULL) {
            if (pnext->state == PROC_TERMINATED) {
                uart_print("[SCHED] Skipping dead task PID: ");
                uart_print_dec(pnext->pid);
                uart_print("\r\n");
                continue; 
            }
            break; 
        } else {
            OS_EXIT_CRITICAL();
            return;
        }
    }

    /* ---------------------------------------------------------
       BƯỚC 4: Xử lý Task hiện tại 
       --------------------------------------------------------- */
    if (current_pcb != NULL) {
        if (current_pcb->state == PROC_RUNNING) {
            current_pcb->state = PROC_READY;
            add_task_to_ready_queue(current_pcb);
        }
        else if (current_pcb->state == PROC_TERMINATED) {
            uart_print("[SCHED] Dropping terminated task PID: ");
            uart_print_dec(current_pcb->pid);
            uart_print("\r\n");
        }
    }

    /* ---------------------------------------------------------
       [MỚI] IN LOG CHI TIẾT CHUYỂN NGỮ CẢNH
       --------------------------------------------------------- */
    if (current_pcb != pnext) {
        uart_print("[CTX] Switch: ");
        
        // In Task cũ
        if (current_pcb == NULL) {
            uart_print("BOOT");
        } else {
            uart_print("PID ");
            uart_print_dec(current_pcb->pid);
            // In thêm độ ưu tiên để dễ debug
            uart_print("(Prio ");
            uart_print_dec(current_pcb->dynamic_priority);
            uart_print(")");
        }

        uart_print(" -> ");

        // In Task mới
        uart_print("PID ");
        uart_print_dec(pnext->pid);
        uart_print("(Prio ");
        uart_print_dec(pnext->dynamic_priority);
        uart_print(")\r\n");
    }

    /* ---------------------------------------------------------
       BƯỚC 5 & 6: Cấu hình và Chuyển ngữ cảnh
       --------------------------------------------------------- */
    next_pcb = pnext;      
    pnext->state = PROC_RUNNING;
    mpu_config_for_task(pnext);

    /* Tối ưu: Nếu Task mới là Task cũ, không cần trigger PendSV làm gì */
    if (current_pcb == pnext) {
        OS_EXIT_CRITICAL();
        return;
    }
    
    if (current_pcb == NULL) {
        /* Trường hợp khởi động lần đầu */
        current_pcb = pnext;
        start_first_task(current_pcb->stack_ptr); 
    } 
    else {
        /* Chuyển ngữ cảnh bình thường */
        SCB_ICSR |= PENDSVSET_BIT;
        OS_EXIT_CRITICAL();
    }
}

/* Hàm khởi tạo riêng cho scheduler (được gọi bởi process_init) */
void scheduler_init_queues(void) {
    for(int i = 0; i < MAX_PRIORITY; i++) {
        queue_init(&ready_queue[i]);
    }
    top_ready_priority_bitmap = 0;
}
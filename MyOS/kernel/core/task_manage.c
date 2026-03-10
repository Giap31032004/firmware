#include "kernel.h"
#include "heap.h"
#include "uart.h"
#include "mpu.h"

#define SCB_ICSR (*(volatile uint32_t*)0xE000ED04)
#define PENDSVSET_BIT (1UL << 28)

#define PRIO_LOWEST      0xFF 
#define SHPR3_ADDR       0xE000ED20 
#define NUM_RESOURCES 3

PCB_t pcb_table[MAX_PROCESSES]; 
static int total_processes = 0; 

extern void scheduler_init_queues(void); 

void os_kernel_init(void) {
    /* 1. IN THÔNG BÁO KHỞI ĐỘNG */
    uart_print("Booting MyOS Kernel...\r\n");

    /* 2. CẤU HÌNH NVIC (Cực kỳ quan trọng) */
    // Set PendSV và SysTick xuống mức thấp nhất
    // Trên Cortex-M, Priority nằm ở thanh ghi SCB->SHP
    // Cách làm nhanh (Direct Register Access):
    volatile uint32_t *shpr3 = (volatile uint32_t *)SHPR3_ADDR;
    // Byte 3 là SysTick, Byte 2 là PendSV. Ghi 0xFF vào để set lowest.
    *shpr3 |= (0xFFUL << 24); // SysTick Priority
    *shpr3 |= (0xFFUL << 16); // PendSV Priority

    uart_print("[OK] NVIC Configured (PendSV/SysTick lowest).\r\n");

    /* 3. KHỞI TẠO BỘ NHỚ & MPU */
    mpu_init();     
    os_mem_init();   
    banker_init(); 

    uart_print("[OK] Banker Algorithm Initialized.\r\n");    
    uart_print("[OK] Memory & MPU Initialized.\r\n");
    
    /* 4. KHỞI TẠO LOGIC SCHEDULER */
    scheduler_init_queues();
    
    total_processes = 0;
    current_pcb = NULL;
    next_pcb = NULL;
    
    process_create(prvIdleTask, 0, 0, NULL);
    uart_print("Creating Idle Task...\r\n");
    
    uart_print("Kernel Initialized. Starting Scheduler...\r\n");
}

void process_create(void (*func)(void), uint32_t pid, uint8_t priority, int *max_res) 
{
    if (pid >= MAX_PROCESSES) return; 

    PCB_t *p = &pcb_table[pid]; 
    

    uint32_t stack_size_bytes = STACK_SIZE * 4; // 1kB stack
    uint32_t *stack_base = (uint32_t*)os_malloc(stack_size_bytes); 

    if(stack_base == NULL) {
        uart_print("Error: Heap Full\r\n");
        return;
    }

    p->stack_base = (uint32_t)stack_base;
    p->stack_size = stack_size_bytes;
    
    // Init resources
    for (int i = 0; i < NUM_RESOURCES; i++) {
         p->res_max[i] = (max_res != NULL) ? max_res[i] : 0;
    }

    // Stack alignment logic
    uint32_t addr = (uint32_t)stack_base + stack_size_bytes;
    addr = addr & 0xFFFFFFF8; // Align to 8 bytes
    uint32_t *sp = (uint32_t*)addr;

    /* Fake Context setup */
    *(--sp) = 0x01000000UL; // xPSR
    *(--sp) = (uint32_t)func; // PC
    *(--sp) = 0xFFFFFFFDUL; // LR
    *(--sp) = 0; // R12
    *(--sp) = 0; // R3
    *(--sp) = 0; // R2
    *(--sp) = 0; // R1
    *(--sp) = 0; // R0
    for (int i = 0; i < 8; ++i) *(--sp) = 0; // R11-R4

    p->stack_ptr = sp;
    p->pid = pid;
    p->entry = func;
    p->state = PROC_NEW; 
    p->dynamic_priority = priority;
    p->static_priority = priority;
    p->time_slice = 5;

    OS_ENTER_CRITICAL();
    add_task_to_ready_queue(p); 
    OS_EXIT_CRITICAL();

    uart_print("Created process ");
    uart_print_dec(pid);
    uart_print("\r\n");

    total_processes++;

    if(current_pcb && p->dynamic_priority > current_pcb->dynamic_priority) {
        SCB_ICSR |= PENDSVSET_BIT;
    }
}

/* Hàm kết thúc Task */
void os_task_kill(int pid) {

    /* 1. Vào vùng an toàn để không ai can thiệp lúc đang thao tác */
    OS_ENTER_CRITICAL();

    /* 2. Kiểm tra PID có hợp lệ không */
    if (pid < 0 || pid >= MAX_PROCESSES || pcb_table[pid].pid == -1) {
        uart_print("Error: PID not found.\r\n");
        OS_EXIT_CRITICAL();
        return;
    }

    /* 3. Đánh dấu Task là "Đã chết" */
    pcb_table[pid].state = PROC_TERMINATED;
    
    uart_print("Task Killed: ");
    uart_print_dec(pid);
    uart_print("\r\n");

    /* 4. Xử lý trường hợp đặc biệt: TỰ SÁT (Self-Kill) 
       Nếu Task đang chạy (current_pcb) tự gọi hàm kill chính nó,
       nó không thể chạy thêm dòng code nào nữa sau dòng này.
       -> Phải gọi Scheduler để chuyển sang Task khác ngay lập tức.
    */
    if (pid == current_pcb->pid) {
        OS_EXIT_CRITICAL();   // Mở khóa ngắt để PendSV có thể chạy
        process_schedule();   // Yêu cầu đổi Task ngay
        while(1);             // Đứng đây chờ chết (không bao giờ thoát ra)
    }

    /* 5. Nếu kill task khác thì cứ mở khóa và chạy tiếp bình thường */
    OS_EXIT_CRITICAL();
}

/* Hàm tạm dừng Task */
void os_task_suspend(int pid) {
    OS_ENTER_CRITICAL();
    
    if (pid >= 0 && pid < MAX_PROCESSES && pcb_table[pid].pid != -1) {
        // Chỉ suspend nếu Task chưa chết
        if (pcb_table[pid].state != PROC_TERMINATED) {
            pcb_table[pid].state = PROC_SUSPENDED;
            uart_print("Task Suspended: ");
            uart_print_dec(pid);
            uart_print("\r\n");

            // Nếu tự suspend chính mình -> Phải nhường CPU ngay
            if (pid == current_pcb->pid) {
                OS_EXIT_CRITICAL();
                process_schedule();
                return;
            }
        }
    }
    OS_EXIT_CRITICAL();
}

/* Hàm khôi phục Task */
void os_task_resume(int pid) {
    OS_ENTER_CRITICAL();

    if (pid >= 0 && pid < MAX_PROCESSES && pcb_table[pid].pid != -1) {
        // Chỉ resume nếu Task đang bị SUSPENDED
        if (pcb_table[pid].state == PROC_SUSPENDED) {
            pcb_table[pid].state = PROC_READY;
            
            // Thêm lại vào hàng đợi ưu tiên 
            add_task_to_ready_queue(&pcb_table[pid]);
            
            uart_print("Task Resumed: ");
            uart_print_dec(pid);
            uart_print("\r\n");
        }
    }
    OS_EXIT_CRITICAL();
}

void prvIdleTask(void){
    while(1){
        __asm("wfi"); 
    }
}
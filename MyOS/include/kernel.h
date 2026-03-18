
#ifndef KERNEL_H  
#define KERNEL_H

#include <stdint.h>
#include "queue.h"   
#include "banker.h" 

/* =================================================
   1. CONSTANTS & MACROS 
   ================================================= */
#define MAX_PROCESSES   20   
#define MAX_PRIORITY    10    
#define STACK_SIZE      256  
#define NUM_RESOURCES   3    

// Critical Section 
#define OS_ENTER_CRITICAL()  __asm volatile ("cpsid i" : : : "memory") // Disable interrupts
#define OS_EXIT_CRITICAL()   __asm volatile ("cpsie i" : : : "memory") // Enable interrupts

/* =================================================
   2. ENUMS & STRUCTS (Giữ nguyên)
   ================================================= */
typedef enum {
    /* --- Nhóm khởi tạo --- */
    PROC_UNUSED = 0,      // Slot PCB này đang trống, có thể tạo task mới vào đây
    PROC_NEW,             // Đã cấp phát, nhưng chưa được Scheduler sờ tới lần nào

    /* --- Nhóm hoạt động --- */
    PROC_READY,           // Đang nằm trong Ready Queue, sẵn sàng chạy
    PROC_RUNNING,         // Đang chiếm giữ CPU

    /* --- Nhóm chờ (Phân loại chi tiết Blocked) --- */
    PROC_WAITING_TIME,    // Chờ hết giờ (os_delay)
    PROC_WAITING_OBJECT,  // Chờ Mutex, Semaphore, Queue
    PROC_WAITING_IO,      // Chờ ngắt ngoại vi (Driver UART,...)

    /* --- Nhóm dừng chủ động --- */
    PROC_SUSPENDED,       // Bị task khác "đóng băng" (vẫn còn bộ nhớ nhưng không được chạy)

    /* --- Nhóm kết thúc --- */
    PROC_TERMINATED,      // Đã chạy xong, chờ OS dọn dẹp bộ nhớ (Garbage Collection)
    PROC_HARD_FAULT       // Task này gây lỗi hệ thống và đã bị OS cưỡng chế dừng
} process_state_t;

typedef struct PCB {
    /* =========================================================
       PHẦN 1: CODE HIỆN TẠI (GIỮ NGUYÊN)
       ========================================================= */
    /* --- CỐT LÕI --- */
    uint32_t *stack_ptr;    
    uint32_t pid;           
    void (*entry)(void);    
    
    /* --- TRẠNG THÁI --- */
    process_state_t state;  
    uint32_t wake_up_tick;  

    /* --- LẬP LỊCH --- */
    uint8_t static_priority; 
    uint8_t dynamic_priority;
    uint8_t time_slice;      

    /* --- MEMORY & RESOURCE --- */
    int res_held[NUM_RESOURCES]; 
    int res_max[NUM_RESOURCES];
    
    uint32_t stack_base;   
    uint32_t stack_size;   
    uint32_t heap_base;    
    uint32_t heap_size;    

    /* --- THỐNG KÊ (Optional) --- */
    uint32_t total_cpu_runtime; 


    /* =========================================================
       PHẦN 2: MỞ RỘNG CHO TƯƠNG LAI (NÂNG CẤP RTOS)
       (Bỏ comment khi bạn sẵn sàng cài đặt tính năng đó)
       ========================================================= */

    /* [NÂNG CẤP 1] DANH SÁCH LIÊN KẾT (Linked List Implementation)
       Hiện tại bạn dùng mảng queue_t[], việc xóa task ở giữa mảng rất chậm.
       Khi chuyển sang dùng Linked List, mỗi PCB sẽ tự biết ai đứng trước/sau nó.
       Giúp thao tác thêm/xóa task tốn O(1) thời gian. */
    // struct PCB *next;
    // struct PCB *prev;
    // struct PCB *list_container; // Con trỏ trỏ ngược về hàng đợi mà nó đang đứng (Ready/Block queue)

    /* [NÂNG CẤP 2] ĐỊNH DANH & DEBUG (Human Readable)
       PID là số, khó nhớ. Tên task giúp debug dễ hơn nhiều (VD: "SensorTask"). */
    // char name[16]; 

    /* [NÂNG CẤP 3] CƠ CHẾ CHỜ ĐỢI NÂNG CAO (Advanced Blocking)
       Khi task BLOCKED, nó cần biết nó đang chờ cái gì (Mutex A hay Queue B?).
       Và nếu chờ Mutex thất bại, biến return_code sẽ báo lỗi. */
    // void *waiting_object;      // Con trỏ trỏ đến Mutex/Semaphore/Queue đang chờ
    // uint32_t wait_result;      // Kết quả sau khi tỉnh dậy (OK hay Timeout?)

    /* [NÂNG CẤP 4] SỰ KIỆN & TÍN HIỆU (Event Flags / Signals)
       Thay vì Semaphore, task có thể chờ 1 bit trong thanh ghi này bật lên.
       Giống cơ chế Event Group của FreeRTOS. */
    // uint32_t event_flags;      // Các cờ sự kiện đang chờ
    // uint32_t notify_value;     // Task Notification (Cực nhanh, thay thế Queue nhẹ)

    /* [NÂNG CẤP 5] QUẢN LÝ LỖI RIÊNG BIỆT (Thread-safe Error)
       Biến 'errno' trong C là biến toàn cục -> Nguy hiểm khi đa luồng.
       Mỗi task cần một biến errno riêng. */
    // int task_errno;

    /* [NÂNG CẤP 6] CẤU TRÚC CHA - CON (Process Hierarchy)
       Nếu Task A tạo ra Task B, khi A chết, B có chết theo không?
       Hoặc A cần đợi B chạy xong (waitpid). */
    // uint32_t parent_pid;
    // int exit_code;             // Mã lỗi trả về khi task kết thúc (return)

    /* [NÂNG CẤP 7] KIỂM TRA TRÀN STACK (Stack Overflow Detection)
       Lưu địa chỉ thấp nhất mà Stack Pointer từng chạm tới.
       Nếu nó < stack_base -> Báo động đỏ! */
    // uint32_t stack_high_water_mark;

} PCB_t;

/* =================================================
   3. GLOBAL VARIABLES (EXTERN)
   ================================================= */
extern PCB_t pcb_table[MAX_PROCESSES];
extern PCB_t* current_pcb;
extern PCB_t* next_pcb;
extern volatile uint32_t tick_count;

extern queue_t ready_queue[MAX_PRIORITY];
extern queue_t job_queue;
extern queue_t device_queue;
extern uint32_t top_ready_priority_bitmap;

/* =================================================
   4. FUNCTION PROTOTYPES
   ================================================= */

/* --- Nhóm 1: Hàm trong task_manage.c --- */
void os_kernel_init(void);
void process_create(void (*func)(void), uint32_t pid, uint8_t priority, int *max_res);
void process_set_state(uint32_t pid, process_state_t new_state); 
void os_task_kill(int pid);
void os_task_exit(void);
void os_task_suspend(int pid);
void os_task_resume(int pid);
void prvIdleTask(void);

/* --- Nhóm 2: Hàm trong scheduler.c --- */
void process_schedule(void);
void add_task_to_ready_queue(PCB_t *p);
PCB_t* get_highest_priority_ready_task(void);
void scheduler_init_queues(void);

/* --- Nhóm 3: Hàm trong timer.c --- */
void os_delay(uint32_t tick);
void process_timer_tick(void);

/* --- Nhóm 4: Hàm trong utils.c --- */
const char* process_state_str(process_state_t state);
int my_strcmp(const char *s1, const char *s2);
int my_strncmp(const char *s1, const char *s2, int n);
 

#endif /* KERNEL_H */

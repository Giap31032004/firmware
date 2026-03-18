#include "task.h"
#include "kernel.h"
#include "sync.h"
#include "ipc.h"

/* ĐỊNH NGHĨA THỰC TẾ (Cấp phát RAM) */
volatile int current_temperature = 25; 
volatile int system_uptime = 0;

os_msg_queue_t temp_queue;
os_mutex_t app_mutex;
os_mutex_t mutex_A;
os_mutex_t mutex_B;
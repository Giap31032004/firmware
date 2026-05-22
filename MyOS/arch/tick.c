#include "kernel.h" // Nhúng kernel.h để lấy nguyên mẫu hàm process_timer_tick()

/* ========================================================================
 * NGẮT SYSTICK PHẦN CỨNG (HARDWARE TICK INTERRUPT)
 * Hàm này được kích hoạt tự động mỗi 1ms (Tùy cấu hình SYSTICK_RATE_HZ)
 * ======================================================================== */
void kernel_tick(void) {
    // KHÔNG CẦN TỰ ĐẾM os_tick_count Ở ĐÂY NỮA!
    
    // Giao toàn bộ quyền sinh sát, đếm giờ, và kiểm tra Round-Robin 
    // cho trái tim của hệ điều hành:
    process_timer_tick();
}
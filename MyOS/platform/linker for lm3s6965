/* Điểm vào của chương trình là Reset_Handler */
ENTRY(Reset_Handler)

/* Khai báo kích thước và vị trí vùng nhớ cho chip LM3S6965 */
MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 256K
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 64K
}

/* Định nghĩa đỉnh Stack (Cuối vùng RAM) */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    /* =======================================================
       1. VÙNG FLASH (Mã lệnh và Dữ liệu chỉ đọc)
       ======================================================= */
    .text :
    {
        . = ALIGN(4);
        
        /* BẮT BUỘC: Bảng Vector phải nằm ở đầu file binary (0x00000000) */
        KEEP(*(.isr_vector)) 
        
        /* Mã lệnh chương trình */
        *(.text*) 
        
        /* Dữ liệu chỉ đọc (Hằng số, chuỗi ký tự...) */
        *(.rodata*) 
        
        . = ALIGN(4);
        /* Đánh dấu điểm kết thúc của code trong FLASH */
        _etext = .; 
    } > FLASH

    /* Lấy địa chỉ của vùng data lưu trong Flash để file startup copy */
    _sidata = LOADADDR(.data);

    /* =======================================================
       2. VÙNG RAM CÓ KHỞI TẠO (.data)
       Lưu ở FLASH (AT) nhưng trỏ địa chỉ chạy ở RAM (VMA)
       ======================================================= */
    .data : AT(_etext)
    {
        . = ALIGN(4);
        _sdata = .;           /* Bắt đầu vùng data trong RAM */
        
        *(.data*)             /* Các biến toàn cục có giá trị khởi tạo khác 0 */
        
        . = ALIGN(4);
        _edata = .;           /* Kết thúc vùng data trong RAM */
    } > RAM

    /* =======================================================
       3. VÙNG RAM CHƯA KHỞI TẠO (.bss)
       Sẽ được hàm Reset_Handler xóa về 0
       ======================================================= */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;            /* Bắt đầu vùng bss trong RAM */
        
        *(.bss*)              /* Các biến toàn cục chưa khởi tạo */
        *(COMMON)
        
        . = ALIGN(4);
        _ebss = .;            /* Kết thúc vùng bss trong RAM */
    } > RAM
}
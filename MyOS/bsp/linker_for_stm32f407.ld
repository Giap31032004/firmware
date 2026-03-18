/* BẢN ĐỒ BỘ NHỚ CHUẨN CÔNG NGHIỆP CHO STM32F407 */
ENTRY(Reset_Handler)

/* Đặt kích thước tối thiểu cho Heap và Stack để kiểm tra tràn RAM lúc Link */
_Min_Heap_Size  = 0x200;  /* 512 bytes */
_Min_Stack_Size = 0x400;  /* 1024 bytes */

MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 512K
  RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 128K
  CCMRAM (xrw)    : ORIGIN = 0x10000000, LENGTH = 64K
}

/* Đỉnh của Stack được tính sau khi đã định nghĩa MEMORY */
PROVIDE ( _estack = ORIGIN(RAM) + LENGTH(RAM) );

SECTIONS
{
  /* 1. Bảng Vector Ngắt */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* 2. Mã lệnh C và dữ liệu chỉ đọc */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  /* 3. Các khối bắt buộc cho ARM và C-Runtime */
  .ARM.extab : 
  { 
    *(.ARM.extab* .gnu.linkonce.armextab.*) 
  } >FLASH

  .ARM : 
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  _sidata = LOADADDR(.data);

  /* 4. Dữ liệu RAM có giá trị ban đầu */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  /* 5. Vùng nhớ CCMRAM (64KB siêu tốc) */
  .ccmram :
  {
    . = ALIGN(4);
    _sccmram = .;
    *(.ccmram)
    *(.ccmram*)
    . = ALIGN(4);
    _eccmram = .;
  } >CCMRAM

  /* 6. Dữ liệu RAM bằng 0 */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } >RAM

  /* Cột mốc bắt đầu cấp phát Heap */
  . = ALIGN(4);
  PROVIDE ( _end = . );
  PROVIDE ( __end__ = . );

  /* 7. Trình giả lập (Dummy Section) kiểm tra va chạm Heap & Stack */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM
}
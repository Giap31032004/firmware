#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <stdint.h>

/* =========================================================================
 * BẢN ĐỒ BỘ NHỚ STM32F407 (VET6 / VGT6 / Discovery)
 * ========================================================================= */

/* 1. Các nhánh Bus chính (System Bus) */
#define PERIPH_BASE           (0x40000000UL) /* Base address của mọi ngoại vi */
#define APB1PERIPH_BASE       (PERIPH_BASE)
#define APB2PERIPH_BASE       (PERIPH_BASE + 0x00010000UL)
#define AHB1PERIPH_BASE       (PERIPH_BASE + 0x00020000UL)
#define AHB2PERIPH_BASE       (PERIPH_BASE + 0x00050000UL)

/* 2. Cầu dao tổng RCC (Reset and Clock Control) - Nằm trên AHB1 */
#define RCC_BASE              (AHB1PERIPH_BASE + 0x3800UL)

/* 3. Các cổng GPIO (Nằm trên AHB1) */
#define GPIOA_BASE            (AHB1PERIPH_BASE + 0x0000UL)
#define GPIOB_BASE            (AHB1PERIPH_BASE + 0x0400UL)
#define GPIOC_BASE            (AHB1PERIPH_BASE + 0x0800UL)
#define GPIOD_BASE            (AHB1PERIPH_BASE + 0x0C00UL)

/* 4. UART / USART */
/* USART1 & USART6 nằm trên Bus tốc độ cao APB2 */
#define USART1_BASE           (APB2PERIPH_BASE + 0x1000UL)
#define USART6_BASE           (APB2PERIPH_BASE + 0x1400UL)

/* USART2 -> UART5 nằm trên Bus APB1 */
#define USART2_BASE           (APB1PERIPH_BASE + 0x4400UL)
#define USART3_BASE           (APB1PERIPH_BASE + 0x4800UL)
#define UART4_BASE            (APB1PERIPH_BASE + 0x4C00UL)
#define UART5_BASE            (APB1PERIPH_BASE + 0x5000UL)

/* 5. Cấu trúc Thanh ghi RCC (Phục vụ việc bật Clock) */
typedef struct {
  volatile uint32_t CR;            /* 0x00 */
  volatile uint32_t PLLCFGR;       /* 0x04 */
  volatile uint32_t CFGR;          /* 0x08 */
  volatile uint32_t CIR;           /* 0x0C */
  volatile uint32_t AHB1RSTR;      /* 0x10 */
  volatile uint32_t AHB2RSTR;      /* 0x14 */
  volatile uint32_t AHB3RSTR;      /* 0x18 */
  uint32_t RESERVED_0;             /* 0x1C */
  volatile uint32_t APB1RSTR;      /* 0x20 */
  volatile uint32_t APB2RSTR;      /* 0x24 */
  uint32_t RESERVED_1[2];          /* 0x28, 0x2C */
  volatile uint32_t AHB1ENR;       /* 0x30: Bật Clock cho GPIO, DMA */
  volatile uint32_t AHB2ENR;       /* 0x34 */
  volatile uint32_t AHB3ENR;       /* 0x38 */
  uint32_t RESERVED_2;             /* 0x3C */
  volatile uint32_t APB1ENR;       /* 0x40: Bật Clock cho USART2..5, Timer2..7 */
  volatile uint32_t APB2ENR;       /* 0x44: Bật Clock cho USART1, USART6, Timer1,8,9 */
} RCC_TypeDef;

#define RCC ((RCC_TypeDef *) RCC_BASE)

/* 6. Cấu trúc Thanh ghi GPIO (Điều khiển chân cắm) */
typedef struct {
  volatile uint32_t MODER;    /* Chế độ (Input/Output/Alternate) */
  volatile uint32_t OTYPER;   /* Loại Output */
  volatile uint32_t OSPEEDR;  /* Tốc độ */
  volatile uint32_t PUPDR;    /* Kéo lên/Kéo xuống */
  volatile uint32_t IDR;      /* Đọc giá trị */
  volatile uint32_t ODR;      /* Ghi giá trị */
  volatile uint32_t BSRR;     /* Set/Reset nhanh */
  volatile uint32_t LCKR;     /* Khóa cấu hình */
  volatile uint32_t AFR[2];   /* Chế độ chức năng phụ (Alternate Function) */
} GPIO_TypeDef;

/* 7. Cấu trúc Thanh ghi USART (Truyền nhận Serial) */
typedef struct {
  volatile uint32_t SR;       /* Thanh ghi Trạng thái (Cờ báo gửi xong/nhận xong) */
  volatile uint32_t DR;       /* Thanh ghi Dữ liệu (Chứa ký tự in ra) */
  volatile uint32_t BRR;      /* Thanh ghi Tốc độ Baud */
  volatile uint32_t CR1;      /* Cấu hình 1 */
  volatile uint32_t CR2;      /* Cấu hình 2 */
  volatile uint32_t CR3;      /* Cấu hình 3 */
  volatile uint32_t GTPR;
} USART_TypeDef;

/* Gắn cấu trúc vào địa chỉ thật */
#define GPIOA   ((GPIO_TypeDef *) GPIOA_BASE)
#define USART1  ((USART_TypeDef *) USART1_BASE)

#endif /* MEMORY_MAP_H */
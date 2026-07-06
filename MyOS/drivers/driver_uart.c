#include "driver_uart.h"

#include "stm32f407xx.h"

#define NVIC_ISER1       (*(volatile uint32_t *)0xE000E104UL)
#define USART1_IRQ_NUM   37U

__attribute__((weak))
void uart_driver_rx_callback_from_isr(char c)
{
    (void)c;
}

void uart_driver_init(void)
{
    RCC->AHB1ENR |= (1U << 0);
    RCC->APB2ENR |= (1U << 4);

    GPIOA->MODER &= ~((3U << (9U * 2U)) | (3U << (10U * 2U)));
    GPIOA->MODER |=  ((2U << (9U * 2U)) | (2U << (10U * 2U)));

    GPIOA->AFR[1] &= ~((0xFU << (1U * 4U)) | (0xFU << (2U * 4U)));
    GPIOA->AFR[1] |=  ((7U << (1U * 4U)) | (7U << (2U * 4U)));

    USART1->CR1 = 0x00U;
    USART1->BRR = 0x008AU;
    USART1->CR1 |= (1U << 13) | (1U << 3) | (1U << 2);
    USART1->CR1 |= USART_CR1_RXNEIE;

    NVIC_ISER1 |= (1U << (USART1_IRQ_NUM - 32U));
}

void uart_driver_putc_raw(char c)
{
    while ((USART1->SR & USART_SR_TXE) == 0U) {
    }

    USART1->DR = (uint16_t)c;
}

void USART1_Handler(void)
{
    if ((USART1->SR & USART_SR_RXNE) != 0U) {
        char c = (char)(USART1->DR & 0xFFU);
        uart_driver_rx_callback_from_isr(c);
    }
}

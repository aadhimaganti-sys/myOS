#include "uart.h"

#define REG(offset) (*(volatile uint32_t *)(UART0_BASE + (offset)))

#define UART_DR    REG(0x00)
#define UART_RSR   REG(0x04)
#define UART_ECR   REG(0x04)
#define UART_FR    REG(0x18)
#define UART_IBRD  REG(0x24)
#define UART_FBRD  REG(0x28)
#define UART_LCR_H REG(0x2C)
#define UART_CR    REG(0x30)
#define UART_IFLS  REG(0x34)
#define UART_IMSC  REG(0x38)
#define UART_RIS   REG(0x3C)
#define UART_MIS   REG(0x40)
#define UART_ICR   REG(0x44)

/* Flag register bits */
#define FR_BUSY (1U << 3)
#define FR_RXFE (1U << 4)
#define FR_TXFF (1U << 5)
#define FR_RXFF (1U << 6)
#define FR_TXFE (1U << 7)

/* Line control register bits */
#define LCR_H_FEN  (1U << 4)
#define LCR_H_WLEN8 (3U << 5)

/* Control register bits */
#define CR_UARTEN (1U << 0)
#define CR_TXE    (1U << 8)
#define CR_RXE    (1U << 9)

void uart_init(void) {
    /* Disable UART before programming */
    UART_CR = 0;

    /* Clear pending interrupts */
    UART_ICR = 0x7FF;

    /*
     * For QEMU virt machine, UART clock is 24MHz.
     * Baud rate: 115200
     * Divider = 24000000 / (16 * 115200) = 13.0208333...
     * IBRD = 13
     * FBRD = integer((0.0208333 * 64) + 0.5) = 1
     */
    UART_IBRD = 13;
    UART_FBRD = 1;

    /* 8 bits, no parity, 1 stop bit, FIFOs enabled */
    UART_LCR_H = LCR_H_WLEN8 | LCR_H_FEN;

    /* Mask all interrupts initially */
    UART_IMSC = 0;

    /* Enable UART, Transmit, and Receive */
    UART_CR = CR_UARTEN | CR_TXE | CR_RXE;
}

void uart_putc(char c) {
    if (c == '\n') {
        /* Wait until transmit FIFO is not full */
        while (UART_FR & FR_TXFF) {
            __asm__ volatile("nop");
        }
        UART_DR = '\r';
    }
    while (UART_FR & FR_TXFF) {
        __asm__ volatile("nop");
    }
    UART_DR = (uint32_t)(unsigned char)c;
}

char uart_getc(void) {
    /* Wait until receive FIFO is not empty */
    while (UART_FR & FR_RXFE) {
        __asm__ volatile("wfe");
    }
    return (char)(UART_DR & 0xFF);
}

int uart_getc_nonblock(char *c) {
    if (UART_FR & FR_RXFE) {
        return 0;
    }
    *c = (char)(UART_DR & 0xFF);
    return 1;
}

void uart_puts(const char *str) {
    if (!str) return;
    while (*str) {
        uart_putc(*str++);
    }
}

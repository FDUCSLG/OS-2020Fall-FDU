#include <stdint.h>

#include "uart.h"
#include "arm.h"
#include "peripherals/mini_uart.h"
#include "peripherals/gpio.h"
#include "console.h"

void
uart_putchar(int c)
{
    while (!(get32(AUX_MU_LSR_REG) & 0x20))
        ;
    put32(AUX_MU_IO_REG, c & 0xFF);
}

int
uart_getchar()
{
    int stat = get32(AUX_MU_IIR_REG);
    if ((stat & 1) || (stat & 6) != 4)
        return -1;
    return get32(AUX_MU_IO_REG) & 0xFF;
}

void
uart_intr()
{
    console_intr(uart_getchar);
}

void
uart_init()
{
    uint32_t selector, enables;

    /* initialize UART */
    enables = get32(AUX_ENABLES);
    enables |= 1;
    put32(AUX_ENABLES, enables);    /* enable UART1, AUX mini uart */
    put32(AUX_MU_CNTL_REG, 0);
    put32(AUX_MU_LCR_REG, 3);       /* 8 bits */
    put32(AUX_MU_MCR_REG, 0);
    put32(AUX_MU_IER_REG, 3 << 2 | 1);
    put32(AUX_MU_IIR_REG, 0xc6);    /* disable interrupts */
    put32(AUX_MU_BAUD_REG, 270);    /* 115200 baud */
    /* map UART1 to GPIO pins */
    selector = get32(GPFSEL1);
    selector&=~((7<<12)|(7<<15)); /* gpio14, gpio15 */
    selector|=(2<<12)|(2<<15);    /* alt5 */
    put32(GPFSEL1, selector);

    put32(GPPUD, 0);            /* enable pins 14 and 15 */
    delay(150);
    put32(GPPUDCLK0, (1<<14)|(1<<15));
    delay(150);
    put32(GPPUDCLK0, 0);        /* flush GPIO setup */
    put32(AUX_MU_CNTL_REG, 3);      /* enable Tx, Rx */
}

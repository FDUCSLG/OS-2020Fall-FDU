#ifndef INC_UART_H
#define INC_UART_H

void uart_init();
void uart_intr();
void uart_putchar(int);
int  uart_getchar();

#endif

#ifndef INC_UART_H
#define INC_UART_H

void uart_init();
char uart_intr();
void uart_putchar(int);
char uart_getchar();

#endif

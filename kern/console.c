#include "console.h"

#include <stdarg.h>
#include <stdint.h>

#include "arm.h"
#include "uart.h"
#include "spinlock.h"

static struct spinlock conslock;
static int panicked = -1;

void
console_init()
{
    uart_init();
}

static void
printint(int64_t x, int base, int sign)
{
    static char digit[] = "0123456789abcdef";
    static char buf[64];

    if (sign && x < 0) {
        x = -x;
        uart_putchar('-');
    }

    int i = 0;
    uint64_t t = x;
    do {
        buf[i++] = digit[t % base];
    } while (t /= base);

    while (i--) uart_putchar(buf[i]);
}

void
vprintfmt(void (*putch)(int), const char *fmt, va_list ap)
{
    int i, c;
    char *s;
    for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
        if (c != '%') {
            putch(c);
            continue;
        }

        int l = 0;
        for (; fmt[i+1] == 'l'; i++)
            l++;

        if (!(c = fmt[++i] & 0xff))
            break;

        switch (c) {
        case 'u':
            if (l == 2) printint(va_arg(ap, int64_t), 10, 0);
            else printint(va_arg(ap, uint32_t), 10, 0);
            break;
        case 'd':
            if (l == 2) printint(va_arg(ap, int64_t), 10, 1);
            else printint(va_arg(ap, int), 10, 1);
            break;
        case 'x':
            if (l == 2) printint(va_arg(ap, int64_t), 16, 0);
            else printint(va_arg(ap, uint32_t), 16, 0);
            break;
        case 'p':
            printint((uint64_t)va_arg(ap, void *), 16, 0);
            break;
        case 'c':
            putch(va_arg(ap, int));
            break;
        case 's':
            if ((s = (char*)va_arg(ap, char *)) == 0)
                s = "(null)";
            for (; *s; s++)
                putch(*s);
            break;
        case '%':
            putch('%');
            break;
        default:
            /* Print unknown % sequence to draw attention. */
            putch('%');
            putch(c);
            break;
        }
    }
}

/* Print to the console. */
void
cprintf(const char *fmt, ...)
{
    va_list ap;

    acquire(&conslock);
    if (panicked >= 0 && panicked != cpuid()) {
        release(&conslock);
        while (1) ;
    }
    va_start(ap, fmt);
    vprintfmt(uart_putchar, fmt, ap);
    va_end(ap);
    release(&conslock);
}

void
cgetchar(int c)
{
    cprintf("%c", c);
}

void
panic(const char *fmt, ...)
{
    va_list ap;

    acquire(&conslock);
    if (panicked < 0) panicked = cpuid();
    else {
        release(&conslock);
        while (1) ;
    }
    va_start(ap, fmt);
    vprintfmt(uart_putchar, fmt, ap);
    va_end(ap);
    release(&conslock);

    cprintf("%s:%d: kernel panic at cpu %d.\n", __FILE__, __LINE__, cpuid());
    while (1) ;
}

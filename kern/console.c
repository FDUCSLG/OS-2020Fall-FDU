#include "console.h"

#include <stdarg.h>
#include <stdint.h>

#include "arm.h"
#include "uart.h"
#include "spinlock.h"
#include "file.h"

#define CONSOLE 1

static struct spinlock conslock;
static int panicked = -1;

#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  size_t r;  // Read index
  size_t w;  // Write index
  size_t e;  // Edit index
} input;
#define C(x)  ((x)-'@')  // Control-x
#define BACKSPACE 0x100

static void
consputc(int c)
{
    if(c == BACKSPACE) {
        uart_putchar('\b'); uart_putchar(' '); uart_putchar('\b');
    } else
        uart_putchar(c);
}

static ssize_t
console_write(struct inode *ip, char *buf, ssize_t n)
{
    iunlock(ip);
    acquire(&conslock);
    for (size_t i = 0; i < n; i++)
        consputc(buf[i] & 0xff);
    release(&conslock);
    ilock(ip);
    return n;
}

static ssize_t
console_read(struct inode *ip, char *dst, ssize_t n)
{
    iunlock(ip);
    size_t target = n;
    acquire(&conslock);
    while (n > 0) {
        while (input.r == input.w) {
            if (thisproc()->killed) {
                release(&conslock);
                ilock(ip);
                return -1;
            }
            sleep(&input.r, &conslock);
        }
        int c = input.buf[input.r++ % INPUT_BUF];
        if (c == C('D')) {  // EOF
            if (n < target) {
                // Save ^D for next time, to make sure
                // caller gets a 0-byte result.
                input.r--;
            }
            break;
        }
        *dst++ = c;
        --n;
        if (c == '\n')
            break;
    }
    release(&conslock);
    ilock(ip);

    return target - n;
}

void
console_init()
{
    uart_init();

    devsw[CONSOLE].read = console_read;
    devsw[CONSOLE].write = console_write;
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
console_intr(int (*getc)())
{
    int c, doprocdump = 0;

    acquire(&conslock);
    if (panicked >= 0) {
        release(&conslock);
        while (1) ;
    }

    while ((c = getc()) >= 0) {
        switch (c) {
        case C('P'):  // Process listing.
            // procdump() locks cons.lock indirectly; invoke later
            doprocdump = 1;
            break;
        case C('U'):  // Kill line.
            while (input.e != input.w && input.buf[(input.e-1) % INPUT_BUF] != '\n') {
                input.e--;
                consputc(BACKSPACE);
            }
        break;
        case C('H'): case '\x7f':  // Backspace
            if (input.e != input.w){
                input.e--;
                consputc(BACKSPACE);
            }
            break;
        default:
            if (c != 0 && input.e-input.r < INPUT_BUF) {
                c = (c == '\r') ? '\n' : c;
                input.buf[input.e++ % INPUT_BUF] = c;
                consputc(c);
                if (c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF) {
                    input.w = input.e;
                    wakeup(&input.r);
                }
            }
            break;
        }
    }
    release(&conslock);

    if (doprocdump) procdump();
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

#ifndef INC_CONSOLE_H
#define INC_CONSOLE_H

#include <stdarg.h>
#include <stdint.h>

void console_init();
void cgetchar(int c);
void cprintf(const char *fmt, ...);
void panic(const char *fmt, ...);

#define assert(x)                                                   \
({                                                                  \
    if (!(x)) {                                                     \
        panic("%s:%d: assertion failed.\n", __FILE__, __LINE__);    \
    }                                                               \
})

/* Assertion with reason. */
#define asserts(x, ...)                                             \
({                                                                  \
    if (!(x)) {                                                     \
        cprintf("%s:%d: assertion failed.\n", __FILE__, __LINE__);  \
        panic(__VA_ARGS__);                                         \
    }                                                               \
})

#endif

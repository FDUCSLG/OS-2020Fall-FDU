#ifndef INC_ARM_H
#define INC_ARM_H

#include <stdint.h>

static inline void
delay(int32_t count)
{
    asm volatile("__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n":
                 "=r"(count): [count]"0"(count) : "cc");
}

static inline void
put32(uint64_t p, uint32_t x)
{
    *(volatile uint32_t *)p = x;
}

static inline uint32_t
get32(uint64_t p)
{
    return *(volatile uint32_t *)p;
}

/*
 * Load vector base address register
 */
static inline void
load_vbar_el1(void *p)
{
    asm volatile("msr vbar_el1, %[x]": : [x]"r"(p));
}

static inline void
load_sp_el1(void *p)
{
    asm volatile("msr sp_el1, %[x]": : [x]"r"(p));
}

#endif

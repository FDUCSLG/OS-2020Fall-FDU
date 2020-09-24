#ifndef INC_STRING_H
#define INC_STRING_H

#include <stdint.h>
#include <stddef.h>

static inline void *
memset(void *str, int c, size_t n)
{
    char *l = (char *)str, *r = l + n;
    for (; l != r; l ++)
        *l = c & 0xff;
    return str;
}

static inline void *
memmove(void *dst, const void *src, size_t n)
{
    const char *s;
    char *d;

    s = (const char *)src;
    d = (char *)dst;
    if (s < d && s + n > d) {
        s += n;
        d += n;
        while (n-- > 0) *--d = *--s;
    } else {
        while (n-- > 0) *d++ = *s++;
    }

    return dst;
}

#endif

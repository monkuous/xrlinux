#include "compiler.h"
#include <stddef.h>
#include <stdint.h>

USED int memcmp(const void *s1, const void *s2, size_t count) {
    const unsigned char *b1 = s1;
    const unsigned char *b2 = s2;

    while (count--) {
        int c1 = *b1++;
        int c2 = *b2++;

        int diff = c1 - c2;
        if (diff != 0) return diff;
    }

    return 0;
}

USED void *memcpy(void *restrict dest, const void *restrict src, size_t count) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    while (count--) *d++ = *s++;

    return dest;
}

USED void *memmove(void *dest, const void *src, size_t count) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    if ((uintptr_t)d < (uintptr_t)s) {
        while (count--) *d++ = *s++;
    } else {
        d += count;
        s += count;

        while (count--) *--d = *--s;
    }

    return dest;
}

USED void *memset(void *dest, int value, size_t count) {
    unsigned char *d = dest;
    unsigned char f = value;

    while (count--) *d++ = f;

    return dest;
}

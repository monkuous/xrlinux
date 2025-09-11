#include "compiler.h"
#include <stddef.h>
#include <stdint.h>

BL_USED int memcmp(const void *s1, const void *s2, size_t count) {
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

BL_USED void *memcpy(void *restrict dest, const void *restrict src, size_t count) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    while (count--) *d++ = *s++;

    return dest;
}

BL_USED void *memmove(void *dest, const void *src, size_t count) {
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

BL_USED void *memset(void *dest, int value, size_t count) {
    unsigned char *d = dest;
    unsigned char f = value;

    while (count--) *d++ = f;

    return dest;
}

BL_USED int strcmp(const char *s1, const char *s2) {
    while (true) {
        int c1 = (unsigned char)*s1++;
        int c2 = (unsigned char)*s2++;

        int diff = c1 - c2;
        if (diff != 0) return diff;
        if (c1 == 0) return 0;
    }
}

BL_USED size_t strlen(const char *str) {
    size_t length = 0;
    while (*str++) length++;
    return length;
}

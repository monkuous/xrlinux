#pragma once

#include <stddef.h>
#include <stdint.h>

#define USED __attribute__((used))

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#define ALIGN_UP(x, y)               \
    ({                               \
        __typeof__(x) _x = (x);      \
        __typeof__(x) _y = (y);      \
        (_x + (_y - 1)) & ~(_y - 1); \
    })
#define ALIGN_DOWN(x, y)        \
    ({                          \
        __typeof__(x) _x = (x); \
        __typeof__(x) _y = (y); \
        _x & ~(_y - 1);         \
    })
#define CONTAINER(type, name, value)                            \
    ({                                                          \
        void *_ptr = (value);                                   \
        _ptr ? (type *)(_ptr - offsetof(type, name)) : nullptr; \
    })

typedef int32_t ssize_t;

static inline void BlCopyMemory(void *restrict dest, const void *restrict src, size_t count) {
    __builtin_memcpy(dest, src, count);
}

static inline void BlCopyMemoryOverlapping(void *dest, const void *src, size_t count) {
    __builtin_memmove(dest, src, count);
}

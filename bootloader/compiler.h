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

static inline uint16_t BlSwap16(uint16_t x) {
    return (x >> 8) | ((x & 0xff) << 8);
}

static inline uint32_t BlSwap32(uint32_t x) {
    return (x >> 24) | ((x & 0xff) << 24) | ((x & 0xff00) << 8) | ((x & 0xff'0000) >> 8);
}

static inline uint64_t BlSwap64(uint64_t x) {
    return (x >> 56) | ((x & 0xff) << 56) | ((x & 0xff00) << 40) | ((x & 0xff00'0000'0000) >> 40) |
           ((x & 0xff'0000) << 24) | ((x & 0xff'0000'0000'0000) >> 24) | ((x & 0xff00'0000) << 8) |
           ((x & 0xff'0000'0000) >> 8);
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define LE16(x) (x)
#define LE32(x) (x)
#define LE64(x) (x)

#define BE16(x) BlSwap16(x)
#define BE32(x) BlSwap32(x)
#define BE64(x) BlSwap64(x)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BE16(x) (x)
#define BE32(x) (x)
#define BE64(x) (x)

#define LE16(x) BlSwap16(x)
#define LE32(x) BlSwap32(x)
#define LE64(x) BlSwap64(x)
#else
#error "Unsupported endianness"
#endif

typedef int32_t ssize_t;

extern _Noreturn void BlAssertionFailed(const char *expr, const char *file, int line, const char *func);

#ifndef NDEBUG
#define ASSERT(x) (__builtin_expect(!!(x), 1) ? (void)0 : BlAssertionFailed(#x, __FILE__, __LINE__, __func__))
#else
#define ASSERT(x) (__builtin_expect(!!(x), 1) ? (void)0 : __builtin_unreachable())
#endif

static inline void BlCopyMemory(void *restrict dest, const void *restrict src, size_t count) {
    __builtin_memcpy(dest, src, count);
}

static inline void BlCopyMemoryOverlapping(void *dest, const void *src, size_t count) {
    __builtin_memmove(dest, src, count);
}

static inline int BlCompareStrings(const char *s1, const char *s2) {
    return __builtin_strcmp(s1, s2);
}

static inline void BlFillMemory(void *dest, unsigned char value, size_t count) {
    __builtin_memset(dest, value, count);
}

static inline size_t BlStringLength(const char *str) {
    return __builtin_strlen(str);
}

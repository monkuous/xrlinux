#pragma once

#include <stddef.h>
#include <stdint.h>

#define BL_USED __attribute__((used))

#define BL_ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#define BL_ALIGN_UP(x, y)            \
    ({                               \
        __typeof__(x) _x = (x);      \
        __typeof__(x) _y = (y);      \
        (_x + (_y - 1)) & ~(_y - 1); \
    })
#define BL_ALIGN_DOWN(x, y)     \
    ({                          \
        __typeof__(x) _x = (x); \
        __typeof__(x) _y = (y); \
        _x & ~(_y - 1);         \
    })
#define BL_MIN(x, y)               \
    ({                          \
        __typeof__(x) _x = (x); \
        __typeof__(y) _y = (y); \
        _x <= _y ? _x : _y;     \
    })
#define BL_CONTAINER(type, name, value)                         \
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
#define BL_LE16(x) (x)
#define BL_LE32(x) (x)
#define BL_LE64(x) (x)

#define BL_BE16(x) BlSwap16(x)
#define BL_BE32(x) BlSwap32(x)
#define BL_BE64(x) BlSwap64(x)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BL_BE16(x) (x)
#define BL_BE32(x) (x)
#define BL_BE64(x) (x)

#define BL_LE16(x) BlSwap16(x)
#define BL_LE32(x) BlSwap32(x)
#define BL_LE64(x) BlSwap64(x)
#else
#error "Unsupported endianness"
#endif

typedef int32_t ssize_t;

extern _Noreturn void BlAssertionFailed(const char *expr, const char *file, int line, const char *func);

#ifndef NDEBUG
#define BL_ASSERT(x) (__builtin_expect(!!(x), 1) ? (void)0 : BlAssertionFailed(#x, __FILE__, __LINE__, __func__))
#else
#define BL_ASSERT(x) (__builtin_expect(!!(x), 1) ? (void)0 : __builtin_unreachable())
#endif

static inline int BlCompareMemory(const void *s1, const void *s2, size_t count) {
    return __builtin_memcmp(s1, s2, count);
}

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

static inline int BlCountTrailingZeroes(unsigned value) {
    int count = 0;

    if ((value & 0xffff) == 0) {
        count += 16;
        value >>= 16;
    }

    if ((value & 0xff) == 0) {
        count += 8;
        value >>= 8;
    }

    if ((value & 0xf) == 0) {
        count += 4;
        value >>= 4;
    }

    if ((value & 3) == 0) {
        count += 2;
        value >>= 2;
    }

    if ((value & 1) == 0) {
        count += 1;
    }

    return count;
}

#include "logging.h"
#include "compiler.h"
#include "platform.h"
#include <stddef.h>

#define BI_MAX_DIGITS 32

_Noreturn void BlCrash(const char *format, ...) {
    va_list args;
    va_start(args);
    BlPrint("BlCrash: %f\n", format, &args);
    va_end(args);

    BxReturnToFirmware();
}

_Noreturn void BlAssertionFailed(const char *expr, const char *file, int line, const char *func) {
    BlCrash("assertion failed: `%s` in %s at %s:%d", expr, func, file, line);
}

void BlPrint(const char *format, ...) {
    va_list args;
    va_start(args);
    BlPrintArgs(format, args);
    va_end(args);
}

static void BiPrintString(const char *str, void (*fn)(unsigned char, void *), void *ctx) {
    char c;

    while ((c = *str++) != 0) {
        fn(c, ctx);
    }
}

static void BiPrintUnsignedDecimal(
    size_t value,
    unsigned minDigits,
    char prefix,
    void (*fn)(unsigned char, void *),
    void *ctx
) {
    unsigned char buffer[BI_MAX_DIGITS];
    size_t index = sizeof(buffer);
    size_t minIndex = prefix != 0 ? 1 : 0;

    do {
        buffer[--index] = '0' + (value % 10);
        value /= 10;
    } while (value != 0);

    while (index > minIndex && sizeof(buffer) - index - minIndex < minDigits) buffer[--index] = '0';

    if (prefix != 0) buffer[--index] = prefix;

    while (index < sizeof(buffer)) {
        fn(buffer[index++], ctx);
    }
}

static void BiPrintSignedDecimal(ssize_t value, unsigned minDigits, void (*fn)(unsigned char, void *), void *ctx) {
    if (value >= 0) {
        BiPrintUnsignedDecimal(value, minDigits, 0, fn, ctx);
    } else {
        BiPrintUnsignedDecimal(-(size_t)value, minDigits, '-', fn, ctx);
    }
}

static void BiPrintHexadecimal(uint64_t value, unsigned minDigits, void (*fn)(unsigned char, void *), void *ctx) {
    unsigned char buffer[BI_MAX_DIGITS];
    size_t index = sizeof(buffer);

    do {
        buffer[--index] = "0123456789abcdef"[value & 0xf];
        value >>= 4;
    } while (value != 0);

    while (index > 0 && sizeof(buffer) - index < minDigits) buffer[--index] = '0';

    while (index < sizeof(buffer)) {
        fn(buffer[index++], ctx);
    }
}

static void BlPrintArgsToCallback(const char *format, va_list args, void (*fn)(unsigned char, void *), void *ctx) {
    char c;

    while ((c = *format++) != 0) {
        if (c != '%') {
            fn(c, ctx);
            continue;
        }

        c = *format++;

        unsigned minDigits = 0;

        while (c >= '0' && c <= '9') {
            minDigits = (minDigits * 10) + (c - '0');
            c = *format++;
        }

        char typeCode = c;

        if (typeCode == 'l' || typeCode == 'z') c = *format++;
        else typeCode = 0;

        switch (c) {
        case '%': fn('%', ctx); break;
        case 'f': {
            const char *format2 = va_arg(args, const char *);
            va_list *args2 = va_arg(args, va_list *);
            BlPrintArgsToCallback(format2, *args2, fn, ctx);
            break;
        }
        case 'c': fn(va_arg(args, int), ctx); break;
        case 's': BiPrintString(va_arg(args, const char *), fn, ctx); break;
        case 'p':
            fn('0', ctx);
            fn('x', ctx);
            BiPrintHexadecimal((uintptr_t)va_arg(args, void *), 0, fn, ctx);
            break;
        case 'd':
            switch (typeCode) {
            case 0: BiPrintSignedDecimal(va_arg(args, int), minDigits, fn, ctx); break;
            case 'z': BiPrintSignedDecimal(va_arg(args, ssize_t), minDigits, fn, ctx); break;
            default: break;
            }
            break;
        case 'u':
            switch (typeCode) {
            case 0: BiPrintUnsignedDecimal(va_arg(args, unsigned), minDigits, 0, fn, ctx); break;
            case 'z': BiPrintUnsignedDecimal(va_arg(args, size_t), minDigits, 0, fn, ctx); break;
            default: break;
            }
            break;
        case 'x':
            switch (typeCode) {
            case 0: BiPrintHexadecimal(va_arg(args, unsigned), minDigits, fn, ctx); break;
            case 'z': BiPrintHexadecimal(va_arg(args, size_t), minDigits, fn, ctx); break;
            case 'l': BiPrintHexadecimal(va_arg(args, uint64_t), minDigits, fn, ctx); break;
            default: break;
            }
        default: break;
        }
    }
}

static void BlPrintArgsCallback(unsigned char c, void *) {
    BxPrintCharacter(c);
}

void BlPrintArgs(const char *format, va_list args) {
    BlPrintArgsToCallback(format, args, BlPrintArgsCallback, nullptr);
}

size_t BlPrintToBuffer(void *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    size_t count = BlPrintArgsToBuffer(buffer, size, format, args);
    va_end(args);
    return count;
}

struct BlPrintBufferData {
    unsigned char *buffer;
    unsigned char *end;
};

static void BlPrintBufferCallback(unsigned char c, void *ptr) {
    struct BlPrintBufferData *ctx = ptr;

    if (ctx->buffer < ctx->end) *ctx->buffer = c;
    ctx->buffer += 1;
}

size_t BlPrintArgsToBuffer(void *buffer, size_t size, const char *format, va_list args) {
    struct BlPrintBufferData data = {buffer, buffer + size};
    BlPrintArgsToCallback(format, args, BlPrintBufferCallback, &data);
    BlPrintBufferCallback(0, &data);
    return (void *)data.buffer - buffer;
}

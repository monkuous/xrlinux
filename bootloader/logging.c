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

void BlPrint(const char *format, ...) {
    va_list args;
    va_start(args);
    BlPrintArgs(format, args);
    va_end(args);
}

static void BiPrintString(const char *str) {
    char c;

    while ((c = *str++) != 0) {
        BxPrintCharacter(c);
    }
}

static void BiPrintUnsignedDecimal(size_t value, unsigned minDigits, char prefix) {
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
        BxPrintCharacter(buffer[index++]);
    }
}

static void BiPrintSignedDecimal(ssize_t value, unsigned minDigits) {
    if (value >= 0) {
        BiPrintUnsignedDecimal(value, minDigits, 0);
    } else {
        BiPrintUnsignedDecimal(-(size_t)value, minDigits, '-');
    }
}

static void BiPrintHexadecimal(uint64_t value, unsigned minDigits) {
    unsigned char buffer[BI_MAX_DIGITS];
    size_t index = sizeof(buffer);

    do {
        buffer[--index] = "0123456789abcdef"[value & 0xf];
        value >>= 4;
    } while (value != 0);

    while (index > 0 && sizeof(buffer) - index < minDigits) buffer[--index] = '0';

    while (index < sizeof(buffer)) {
        BxPrintCharacter(buffer[index++]);
    }
}

void BlPrintArgs(const char *format, va_list args) {
    char c;

    while ((c = *format++) != 0) {
        if (c != '%') {
            BxPrintCharacter(c);
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
        case '%': BxPrintCharacter('%'); break;
        case 'f': {
            const char *format2 = va_arg(args, const char *);
            va_list *args2 = va_arg(args, va_list *);
            BlPrintArgs(format2, *args2);
            break;
        }
        case 'c': BxPrintCharacter(va_arg(args, int)); break;
        case 's': BiPrintString(va_arg(args, const char *)); break;
        case 'p':
            BxPrintCharacter('0');
            BxPrintCharacter('x');
            BiPrintHexadecimal((uintptr_t)va_arg(args, void *), 0);
            break;
        case 'd':
            switch (typeCode) {
            case 0: BiPrintSignedDecimal(va_arg(args, int), minDigits); break;
            case 'z': BiPrintSignedDecimal(va_arg(args, ssize_t), minDigits); break;
            default: break;
            }
            break;
        case 'u':
            switch (typeCode) {
            case 0: BiPrintUnsignedDecimal(va_arg(args, unsigned), minDigits, 0); break;
            case 'z': BiPrintUnsignedDecimal(va_arg(args, size_t), minDigits, 0); break;
            default: break;
            }
            break;
        case 'x':
            switch (typeCode) {
            case 0: BiPrintHexadecimal(va_arg(args, unsigned), minDigits); break;
            case 'z': BiPrintHexadecimal(va_arg(args, size_t), minDigits); break;
            case 'l': BiPrintHexadecimal(va_arg(args, uint64_t), minDigits); break;
            default: break;
            }
        default: break;
        }
    }
}

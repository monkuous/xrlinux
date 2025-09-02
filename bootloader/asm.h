#pragma once

#include <stddef.h>

static inline size_t BlReadWhami(void) {
    size_t value;
    asm volatile("mfcr %0, whami" : "=r"(value));
    return value;
}

#pragma once

#include <stdarg.h>
#include <stddef.h>

_Noreturn void BlCrash(const char *format, ...);
void BlPrint(const char *format, ...);
void BlPrintArgs(const char *format, va_list args);
size_t BlPrintToBuffer(void *buffer, size_t size, const char *format, ...);
size_t BlPrintArgsToBuffer(void *buffer, size_t size, const char *format, va_list args);

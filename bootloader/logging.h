#pragma once

#include <stdarg.h>

_Noreturn void BlCrash(const char *format, ...);
void BlPrint(const char *format, ...);
void BlPrintArgs(const char *format, va_list args);

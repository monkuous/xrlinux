#pragma once

#include <stddef.h>
#include <stdint.h>

_Noreturn void BlTransition(uintptr_t entrypoint, void *deviceTree, size_t totalCpus, size_t protocolMinor);

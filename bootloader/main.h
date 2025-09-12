#pragma once

#include <stdint.h>

struct BiKernelHeader {
    uint32_t Magic;
    uint16_t MinorVersion;
    uint16_t MajorVersion;
    uint32_t VirtualAddr;
    uint32_t MSize;
    uint32_t Entry;
    uint32_t Flags;
    uint32_t DtbAddress;
    uint32_t MaxDtbEnd;
};

#define BL_FLAG_MAP_DTB (1U << 0)

extern struct BiKernelHeader BlKernelHeader;

_Noreturn void BlMain(void);

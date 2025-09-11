#pragma once

#include <stdint.h>

#define BL_PAGE_SHIFT 12
#define BL_PAGE_SIZE (1U << BL_PAGE_SHIFT)
#define BL_PAGE_MASK (BL_PAGE_SIZE - 1)

typedef uint32_t paddr_t;

void BlMapPage(uintptr_t virt, paddr_t phys);
paddr_t BlGetMapping(uintptr_t virt);

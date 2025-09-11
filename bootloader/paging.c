#include "paging.h"
#include "compiler.h"
#include "logging.h"
#include "memory.h"

#define BI_META_BITS 0x17 /* V=1, W=1, K=1, G=1 */
#define BI_PFN_SHIFT 5
#define BI_PFN_BITS 20

#define BI_LEVEL_SHIFT 10
#define BI_LEVEL_COUNT 2
#define BI_LEVEL_SIZE (1U << BI_LEVEL_SHIFT)
#define BI_LEVEL_MASK (BI_LEVEL_SIZE - 1)

typedef uint32_t pte_t;

#define BI_PFN_MASK (((pte_t)1 << BI_PFN_BITS) - 1)

USED _Alignas(BL_PAGE_SIZE) pte_t BlPageTable[BI_LEVEL_SIZE];

_Static_assert(sizeof(BlPageTable) == BL_PAGE_SIZE, "Page table size incorrect");

static inline pte_t BiCreatePte(paddr_t phys) {
    return ((phys >> BL_PAGE_SHIFT) << BI_PFN_SHIFT) | BI_META_BITS;
}

static inline paddr_t BiDecodePte(pte_t pte) {
    return ((pte >> BI_PFN_SHIFT) & BI_PFN_MASK) << BL_PAGE_SHIFT;
}

static inline size_t BiPteIndex(uintptr_t virt, size_t level) {
    return (virt >> (BL_PAGE_SHIFT + BI_LEVEL_SHIFT * level)) & BI_LEVEL_MASK;
}

static inline pte_t *BiGetTable(uintptr_t virt, bool allowCreation) {
    pte_t *table = BlPageTable;

    for (size_t i = BI_LEVEL_COUNT; i > 1; i--) {
        size_t index = BiPteIndex(virt, i - 1);
        pte_t entry = table[index];

        if (entry) {
            table = (pte_t *)(uintptr_t)BiDecodePte(entry);
        } else if (allowCreation) {
            pte_t *newTable = BlAllocateHeap(BL_PAGE_SIZE, BL_PAGE_SIZE, true);
            BlFillMemory(newTable, 0, BL_PAGE_SIZE);
            table[index] = BiCreatePte((uintptr_t)newTable);
            table = newTable;
        } else {
            BlCrash("BiGetTable: not found");
        }
    }

    return table;
}

void BlMapPage(uintptr_t virt, paddr_t phys) {
    BiGetTable(virt, true)[BiPteIndex(virt, 0)] = BiCreatePte(phys);
}

paddr_t BlGetMapping(uintptr_t virt) {
    pte_t pte = BiGetTable(virt, false)[BiPteIndex(virt, 0)];
    if (!pte) BlCrash("BiGetMapping: not found");
    return BiDecodePte(pte) | (virt & BL_PAGE_MASK);
}

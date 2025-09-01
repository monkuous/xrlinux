#include "a4x.h"
#include "compiler.h"
#include "logging.h"
#include "memory.h"

extern const char BxImageEnd[];

static void BxAddMemoryRanges(struct FwDeviceDatabase *deviceDatabase) {
    uintptr_t minHeapAddr = (uintptr_t)&BxImageEnd;

    for (size_t i = 0; i < ARRAY_SIZE(deviceDatabase->RamBanks); i++) {
        size_t pages = deviceDatabase->RamBanks[i].PageFrameCount;
        if (!pages) continue;

        uintptr_t base = i * 0x2'000'000; // RAM banks are placed at 32M intervals
        uintptr_t end = base + pages * 0x1'000;

        if (base < minHeapAddr) base = minHeapAddr;
        if (base >= end) continue;

        BlAddHeapRange(base, end - base);
    }
}

USED _Noreturn void BxMain(
    struct FwDeviceDatabase *deviceDatabase,
    struct FwApiTable *apiTable,
    struct FwPartition *bootPartition,
    char *args
) {
    BxApiTable = apiTable;

    BxAddMemoryRanges(deviceDatabase);

    BlCrash("TODO (%p, %p, %p, %s)", deviceDatabase, apiTable, bootPartition, args);
}

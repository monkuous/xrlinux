#include "a4x.h"
#include "compiler.h"
#include "dt.h"
#include "logging.h"
#include "memory.h"

#define BX_RAM_BANK_INTERVAL 0x200'0000 // RAM banks are placed at 32M intervals

extern const char BxImageEnd[];

static void BxAddMemoryRanges(struct FwDeviceDatabase *deviceDatabase) {
    uintptr_t minHeapAddr = (uintptr_t)&BxImageEnd;

    for (size_t i = 0; i < ARRAY_SIZE(deviceDatabase->RamBanks); i++) {
        size_t pages = deviceDatabase->RamBanks[i].PageFrameCount;
        if (!pages) continue;

        uintptr_t base = i * BX_RAM_BANK_INTERVAL;
        uintptr_t end = base + pages * 0x1000;

        if (base < minHeapAddr) base = minHeapAddr;
        if (base >= end) continue;

        BlAddHeapRange(base, end - base);
    }
}

static void BxEmitMemoryNode(size_t start, size_t end) {
    char buffer[32];
    BlPrintToBuffer(buffer, ARRAY_SIZE(buffer), "memory@%zx", start);

    auto node = BlDtCreateNode(nullptr, buffer);
    uint32_t reg[] = {start, end - start};

    BlDtAddPropertyString(node, "device_type", "memory");
    BlDtAddPropertyU32s(node, "reg", reg, ARRAY_SIZE(reg));
}

static void BxAddMemoryToDeviceTree(struct FwDeviceDatabase *deviceDatabase) {
    size_t start = 0;
    size_t end = 0;

    for (size_t i = 0; i < ARRAY_SIZE(deviceDatabase->RamBanks); i++) {
        size_t pages = deviceDatabase->RamBanks[i].PageFrameCount;
        if (!pages) continue;

        size_t base = i * BX_RAM_BANK_INTERVAL;

        if (base != end) {
            if (start != end) BxEmitMemoryNode(start, end);
            start = base;
        }

        end = base + pages * 0x1000;
    }

    if (start != end) BxEmitMemoryNode(start, end);
}

static void BxFillDeviceTree(struct FwDeviceDatabase *deviceDatabase) {
    BlDtAddPropertyU32(nullptr, "#address-cells", 1);
    BlDtAddPropertyU32(nullptr, "#size-cells", 1);
    BlDtAddPropertyString(nullptr, "compatible", "xrarch,xrcomputer");

    switch (deviceDatabase->MachineType) {
    case FW_XR_STATION: BlDtAddPropertyString(nullptr, "model", "XR/station"); break;
    case FW_XR_MP: BlDtAddPropertyString(nullptr, "model", "XR/MP"); break;
    case FW_XR_FRAME: BlDtAddPropertyString(nullptr, "model", "XR/frame"); break;
    default: BlCrash("unknown machine type"); break;
    }

    BxAddMemoryToDeviceTree(deviceDatabase);
}

USED _Noreturn void BxMain(
    struct FwDeviceDatabase *deviceDatabase,
    struct FwApiTable *apiTable,
    struct FwPartition *bootPartition,
    char *args
) {
    BxApiTable = apiTable;

    BxAddMemoryRanges(deviceDatabase);
    BxFillDeviceTree(deviceDatabase);

    BlPrint("Built FDT at %p\n", BlDtBuildBlob());
    BlCrash("TODO (%p, %p, %p, %s)", deviceDatabase, apiTable, bootPartition, args);
}

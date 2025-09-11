#include "main.h"
#include "a4x.h"
#include "compiler.h"
#include "dt.h"
#include "logging.h"
#include "memory.h"
#include "platform.h"

#define BX_RAM_BANK_INTERVAL 0x200'0000 // RAM banks are placed at 32M intervals

#define BX_CPU_IRQ_INT 1

#define BX_LSIC_BASE 0xf803'0000
#define BX_LSIC_SIZE 0x100

#define BX_RTC_BASE 0xf800'0080
#define BX_RTC_SIZE 8
#define BX_RTC_IRQ 2

#define BX_SERIAL_COUNT 2
#define BX_SERIAL_SIZE 8
#define BX_SERIAL_BASE(n) (0xf800'0040 + (n) * BX_SERIAL_SIZE)
#define BX_SERIAL_IRQ(n) (4 + (n))
#define BX_SERIAL_BAUD 9600

#define BX_DISKS_BASE 0xf800'0064
#define BX_DISKS_SIZE 12
#define BX_DISKS_IRQ 3

#define BX_AMTSU_BASE 0xf800'00c0
#define BX_AMTSU_SIZE 20
#define BX_AMTSU_IRQ 0x30
#define BX_AMTSU_NIRQ 4

#define BX_BOARD_SIZE 0x800'0000
#define BX_BOARD_IRQ(n) (0x28 + (n))

extern const char BxImageEnd[];

static void BxAddMemoryRanges(void) {
    BlPrint("Initializing bootloader heap\n");

    uintptr_t minHeapAddr = (uintptr_t)&BxImageEnd;

    for (size_t i = 0; i < BL_ARRAY_SIZE(BxDeviceDatabase->RamBanks); i++) {
        size_t pages = BxDeviceDatabase->RamBanks[i].PageFrameCount;
        if (!pages) continue;

        uintptr_t base = i * BX_RAM_BANK_INTERVAL;
        uintptr_t end = base + pages * 0x1000;

        if (base < minHeapAddr) base = minHeapAddr;
        if (base >= end) continue;

        BlAddHeapRange(base, end - base);
    }
}

static void BxDtAddMemoryBank(size_t start, size_t end) {
    char buffer[32];
    BlPrintToBuffer(buffer, BL_ARRAY_SIZE(buffer), "memory@%zx", start);

    auto node = BlDtCreateNode(nullptr, buffer);
    uint32_t reg[] = {start, end - start};

    BlDtAddPropertyString(node, "device_type", "memory");
    BlDtAddPropertyU32s(node, "reg", reg, BL_ARRAY_SIZE(reg));
}

static void BxDtAddMemory(void) {
    size_t start = 0;
    size_t end = 0;

    for (size_t i = 0; i < BL_ARRAY_SIZE(BxDeviceDatabase->RamBanks); i++) {
        size_t pages = BxDeviceDatabase->RamBanks[i].PageFrameCount;
        if (!pages) continue;

        size_t base = i * BX_RAM_BANK_INTERVAL;

        if (base != end) {
            if (start != end) BxDtAddMemoryBank(start, end);
            start = base;
        }

        end = base + pages * 0x1000;
    }

    if (start != end) BxDtAddMemoryBank(start, end);
}

static void BxDtAddChosen(char *args) {
    auto node = BlDtCreateNode(nullptr, "chosen");
    BlDtAddPropertyString(node, "bootargs", args);
}

static uint32_t BxCpuPhandles[BL_ARRAY_SIZE(((struct FwDeviceDatabase *)0)->Processors)];
static uint32_t BxLsicPhandle;

static void BxDtAddCpus(void) {
    auto cpus = BlDtCreateNode(nullptr, "cpus");
    BlDtAddPropertyU32(cpus, "#address-cells", 1);
    BlDtAddPropertyU32(cpus, "#size-cells", 0);

    for (size_t i = 0; i < BL_ARRAY_SIZE(BxDeviceDatabase->Processors); i++) {
        if (!BxDeviceDatabase->Processors[i].Present) continue;

        char buffer[32];
        BlPrintToBuffer(buffer, sizeof(buffer), "cpu@%u", i);

        auto cpu = BlDtCreateNode(cpus, buffer);
        auto phandle = BlDtAllocPhandle();
        BlDtAddPropertyU32(cpu, "phandle", phandle);
        BlDtAddPropertyString(cpu, "device_type", "cpu");
        BlDtAddPropertyU32(cpu, "reg", i);
        BlDtAddPropertyString(cpu, "status", "okay");
        BlDtAddPropertyString(cpu, "compatible", "xrarch,xr17032");
        BlDtAddProperty(cpu, "interrupt-controller", nullptr, 0);
        BlDtAddPropertyU32(cpu, "#interrupt-cells", 1);

        BxCpuPhandles[BxNumCpus++] = phandle;
    }
}

static void BxDtAddInterrupts(struct BlDtNode *node, uint32_t base, uint32_t count) {
    BlDtAddPropertyU32(node, "interrupt-parent", BxLsicPhandle);

    if (count == 1) {
        BlDtAddPropertyU32(node, "interrupts", base);
        return;
    }

    auto data = BL_ALLOCATE(uint32_t, count);

    for (size_t i = 0; i < count; i++) {
        data[i] = base + i;
    }

    BlDtAddPropertyU32s(node, "interrupts", data, count);

    BlFreeHeap(data);
}

static struct BlDtNode *BxDtAddDevice(
    const char *name,
    const char *cid,
    size_t address,
    size_t size,
    uint32_t irq,
    uint32_t nirq
) {
    char buffer[32];
    BlPrintToBuffer(buffer, sizeof(buffer), "%s@%zx", name, address);

    auto node = BlDtCreateNode(nullptr, buffer);
    uint32_t reg[] = {address, size};

    BlDtAddPropertyU32s(node, "reg", reg, BL_ARRAY_SIZE(reg));
    BlDtAddPropertyString(node, "compatible", cid);
    BxDtAddInterrupts(node, irq, nirq);

    return node;
}

static void BxDtAddRtc(void) {
    BxDtAddDevice("rtc", "xrarch,rtc", BX_RTC_BASE, BX_RTC_SIZE, BX_RTC_IRQ, 1);
}

static void BxDtAddSerial(void) {
    for (size_t i = 0; i < BX_SERIAL_COUNT; i++) {
        auto node = BxDtAddDevice("serial", "xrarch,serial", BX_SERIAL_BASE(i), BX_SERIAL_SIZE, BX_SERIAL_IRQ(i), 1);
        BlDtAddPropertyU32(node, "clock-frequency", BX_SERIAL_BAUD);
        BlDtAddPropertyU32(node, "current-speed", BX_SERIAL_BAUD);
    }
}

static void BxDtAddDisks(void) {
    BxDtAddDevice("disk-controller", "xrarch,disk-controller", BX_DISKS_BASE, BX_DISKS_SIZE, BX_DISKS_IRQ, 1);
}

static void BxDtAddAmtsu(void) {
    BxDtAddDevice("amtsu", "xrarch,amtsu", BX_AMTSU_BASE, BX_AMTSU_SIZE, BX_AMTSU_IRQ, BX_AMTSU_NIRQ);
}

static void BxDtAddLsic(void) {
    BxLsicPhandle = BlDtAllocPhandle();

    auto data = BL_ALLOCATE(uint32_t, BxNumCpus * 2);

    for (size_t i = 0; i < BxNumCpus; i++) {
        data[i * 2] = BxCpuPhandles[i];
        data[i * 2 + 1] = BX_CPU_IRQ_INT;
    }

    char buffer[32];
    BlPrintToBuffer(buffer, sizeof(buffer), "lsic@%zx", (size_t)BX_LSIC_BASE);

    auto node = BlDtCreateNode(nullptr, buffer);
    uint32_t reg[] = {BX_LSIC_BASE, BX_LSIC_SIZE};

    BlDtAddPropertyU32(node, "phandle", BxLsicPhandle);
    BlDtAddPropertyU32s(node, "reg", reg, BL_ARRAY_SIZE(reg));
    BlDtAddPropertyString(node, "compatible", "xrarch,lsic");
    BlDtAddPropertyU32s(node, "interrupts-extended", data, BxNumCpus * 2);
    BlDtAddProperty(node, "interrupt-controller", nullptr, 0);
    BlDtAddPropertyU32(node, "#interrupt-cells", 1);

    BlFreeHeap(data);
}

static void BxDtAddBoards(void) {
    for (size_t i = 0; i < BL_ARRAY_SIZE(BxDeviceDatabase->Boards); i++) {
        struct FwBoard *board = &BxDeviceDatabase->Boards[i];
        if (!board->BoardId) continue;

        char buffer[32];
        BlPrintToBuffer(buffer, sizeof(buffer), "xrarch,expansion-%zx", board->BoardId);

        auto node = BxDtAddDevice(
            "expansion-board",
            buffer,
            (uintptr_t)board->Address,
            BX_BOARD_SIZE,
            BX_BOARD_IRQ(i),
            1
        );

        if (board->Name[0]) {
            BlDtAddPropertyString(node, "model", board->Name);
        }
    }
}

static void BxDtPopulate(char *args) {
    BlPrint("Populating device tree\n");

    BlDtAddPropertyU32(nullptr, "#address-cells", 1);
    BlDtAddPropertyU32(nullptr, "#size-cells", 1);
    BlDtAddPropertyString(nullptr, "compatible", "xrarch,xrcomputer");

    switch (BxDeviceDatabase->MachineType) {
    case FW_XR_STATION: BlDtAddPropertyString(nullptr, "model", "XR/station"); break;
    case FW_XR_MP: BlDtAddPropertyString(nullptr, "model", "XR/MP"); break;
    case FW_XR_FRAME: BlDtAddPropertyString(nullptr, "model", "XR/frame"); break;
    default: BlCrash("unknown machine type"); break;
    }

    BxDtAddMemory();
    BxDtAddChosen(args);
    BxDtAddCpus();
    BxDtAddLsic();
    BxDtAddRtc();
    BxDtAddSerial();
    BxDtAddDisks();
    BxDtAddAmtsu();
    BxDtAddBoards();
}

BL_USED _Noreturn void BxMain(
    struct FwDeviceDatabase *deviceDatabase,
    struct FwApiTable *apiTable,
    struct FwPartition *bootPartition,
    char *args
) {
    BxDeviceDatabase = deviceDatabase;
    BxApiTable = apiTable;

    struct FwDisk *disk = &deviceDatabase->Disks[bootPartition->Id];
    BxBootDisk = &disk->Partitions[BL_ARRAY_SIZE(disk->Partitions) - 1];
    BL_ASSERT(BxBootDisk->BaseSector == 0);

    BxAddMemoryRanges();
    BxDtPopulate(args);

    BlMain();
}

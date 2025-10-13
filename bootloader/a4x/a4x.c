#include "a4x.h"
#include "asm.h"
#include "compiler.h"
#include "logging.h"
#include "platform.h"
#include "platformdefs.h"

struct FwDeviceDatabase *BxDeviceDatabase;
struct FwApiTable *BxApiTable;
struct FwPartition *BxBootDisk;
size_t BxNumCpus;

void BxPrintCharacter(unsigned char c) {
    BxApiTable->PutCharacter(c);
}

bool BxReadFromDisk(void *buffer, uint64_t sector, size_t count) {
    if ((uintptr_t)buffer & BL_SECTOR_MASK) BlCrash("BxReadFromDisk: unaligned buffer");

    uint64_t end = sector + count;
    if (end < sector) return false;
    if (end > BxBootDisk->SectorCount) end = BxBootDisk->SectorCount;

    return BxApiTable->ReadDisk(BxBootDisk, buffer, sector, end - sector);
}

struct BxKickProcessorData {
    void (*func)(void *);
    void *ctx;
    size_t numFinished;
};

static void BxKickProcessorHandler(size_t, void *ptr) {
    struct BxKickProcessorData *data = ptr;

    void (*func)(void *) = data->func;
    void *ctx = data->ctx;

    // This is just an atomic add with release semantics, but that isn't implemented in GCC currently,
    // so do it via ASM.

    size_t scratch;
    asm volatile("wmb\n"
                 "1:\n"
                 "ll\t%0,%1\n"
                 "addi\t%0,%0,1\n"
                 "sc\t%0,%1,%0\n"
                 "beq\t%0,1b"
                 : "=&r"(scratch)
                 : "r"(&data->numFinished)
                 : "memory");

    func(ctx);
}

void BxRunOnOtherCpus(void (*func)(void *), void *ctx) {
    size_t respondCount = BxNumCpus - 1;

    if (respondCount == 0) return;

    struct BxKickProcessorData data = {func, ctx, 0};
    size_t self = BlReadWhami();

    for (size_t i = 0; i < BL_ARRAY_SIZE(BxDeviceDatabase->Processors); i++) {
        if (i == self) continue;
        if (!BxDeviceDatabase->Processors[i].Present) continue;

        BxApiTable->KickProcessor(i, &data, BxKickProcessorHandler);
    }

    while (__atomic_load_n(&data.numFinished, __ATOMIC_ACQUIRE) < respondCount) {
        asm("pause");
    }
}

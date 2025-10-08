#include "main.h"
#include "compiler.h"
#include "config.h"
#include "dt.h"
#include "filesystem.h"
#include "logging.h"
#include "memory.h"
#include "paging.h"
#include "partition.h"
#include "platform.h"
#include "transition.h"

#define BI_PROTOCOL_MAGIC 0x584c5258
#define BI_PROTOCOL_MAJOR 2
#define BI_PROTOCOL_MINOR 0

struct BiKernelHeader BlKernelHeader;

static bool BiRangesOverlap(uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1) {
    return a0 <= b1 && b0 <= a1;
}

static void BiLoadKernel(void) {
    BlPrint("Loading kernel from %s\n", BlKernelPath);

    struct BlFsFile *file = BlFsFind(BlKernelPath);
    if (!file) BlCrash("failed to open kernel file");

    BlFsFileRead(file, &BlKernelHeader, sizeof(BlKernelHeader), 0);
    if (BL_LE32(BlKernelHeader.Magic) != BI_PROTOCOL_MAGIC) BlCrash("invalid magic number");

    BlKernelHeader.MinorVersion = BL_LE16(BlKernelHeader.MinorVersion);
    BlKernelHeader.MajorVersion = BL_LE16(BlKernelHeader.MajorVersion);

    if (BlKernelHeader.MajorVersion != BI_PROTOCOL_MAJOR) BlCrash("unsupported major version");

    BlKernelHeader.VirtualAddr = BL_LE32(BlKernelHeader.VirtualAddr);
    BlKernelHeader.MSize = BL_LE32(BlKernelHeader.MSize);
    BlKernelHeader.Entry = BL_LE32(BlKernelHeader.Entry);
    BlKernelHeader.Flags = BL_LE32(BlKernelHeader.Flags);
    BlKernelHeader.DtbAddress = BL_LE32(BlKernelHeader.DtbAddress);
    BlKernelHeader.MaxDtbEnd = BL_LE32(BlKernelHeader.MaxDtbEnd);

    if (BlKernelHeader.Entry < BlKernelHeader.VirtualAddr ||
        BlKernelHeader.Entry - BlKernelHeader.VirtualAddr >= BlKernelHeader.MSize) {
        BlCrash("kernel entry point outside kernel image");
    }

    if (BlKernelHeader.Flags & BL_FLAG_MAP_DTB) {
        BlKernelHeader.DtbAddress = BL_ALIGN_UP(BlKernelHeader.DtbAddress, BL_PAGE_SIZE);

        if (BlKernelHeader.MaxDtbEnd <= BlKernelHeader.DtbAddress) {
            BlCrash("device tree mapping area has negative size");
        }

        if (BiRangesOverlap(
                BlKernelHeader.VirtualAddr,
                BlKernelHeader.VirtualAddr + BlKernelHeader.MSize - 1,
                BlKernelHeader.DtbAddress,
                BlKernelHeader.MaxDtbEnd
            )) {
            BlCrash("device tree mapping area overlaps kernel image");
        }
    }

    uint64_t fileSize = BlFsFileSize(file);
    if (fileSize > BlKernelHeader.MSize) BlCrash("kernel file too large (0x%lx bytes)", fileSize);

    uintptr_t current = BL_ALIGN_DOWN(BlKernelHeader.VirtualAddr, BL_PAGE_SIZE);
    uintptr_t fileEnd = BlKernelHeader.VirtualAddr + fileSize;
    uintptr_t alignedFileEnd = BL_ALIGN_DOWN(fileEnd, BL_PAGE_SIZE);
    uintptr_t end = BL_ALIGN_UP(BlKernelHeader.VirtualAddr + BlKernelHeader.MSize, BL_PAGE_SIZE);

    if (current < BlKernelHeader.VirtualAddr) {
        void *buffer = BlAllocateHeap(BL_PAGE_SIZE, BL_PAGE_SIZE, true);
        size_t headCount = BlKernelHeader.VirtualAddr - current;
        size_t tailCount = BL_PAGE_SIZE - headCount;
        size_t readCount = BL_MIN(tailCount, fileSize);

        BlFillMemory(buffer, 0, headCount);
        BlFsFileRead(file, buffer + headCount, readCount, 0);

        if (readCount != tailCount) {
            BlFillMemory(buffer + headCount + readCount, 0, tailCount - readCount);
        }

        BlMapPage(current, (uintptr_t)buffer);
        current += BL_PAGE_SIZE;
    }

    while (current < alignedFileEnd) {
        void *buffer = BlAllocateHeap(BL_PAGE_SIZE, BL_PAGE_SIZE, true);
        BlFsFileRead(file, buffer, BL_PAGE_SIZE, current - BlKernelHeader.VirtualAddr);
        BlMapPage(current, (uintptr_t)buffer);
        current += BL_PAGE_SIZE;
    }

    if (current < fileEnd) {
        void *buffer = BlAllocateHeap(BL_PAGE_SIZE, BL_PAGE_SIZE, true);
        size_t headCount = fileEnd - current;
        size_t tailCount = BL_PAGE_SIZE - headCount;

        BlFsFileRead(file, buffer, headCount, current - BlKernelHeader.VirtualAddr);
        BlFillMemory(buffer + headCount, 0, tailCount);

        BlMapPage(current, (uintptr_t)buffer);
        current += BL_PAGE_SIZE;
    }

    while (current < end) {
        void *buffer = BlAllocateHeap(BL_PAGE_SIZE, BL_PAGE_SIZE, true);
        BlFillMemory(buffer, 0, BL_PAGE_SIZE);
        BlMapPage(current, (uintptr_t)buffer);
        current += BL_PAGE_SIZE;
    }
}

struct BiTransitionData {
    uintptr_t entrypoint;
    void *deviceTree;
};

_Noreturn static void BiDoTransition(void *ptr) {
    struct BiTransitionData *data = ptr;

    BlTransition(data->entrypoint, data->deviceTree, BxNumCpus, BI_PROTOCOL_MINOR);
}

static void BiProcessConfig(void) {
    if (BlStdoutPath) {
        auto chosen = BlDtFindOrCreateNode(nullptr, "chosen");
        BlDtAddPropertyString(chosen, "stdout-path", BlStdoutPath);
    }
}

_Noreturn void BlMain(void) {
    BlFindRootPartition();
    BiProcessConfig();

    BiLoadKernel();

    struct BiTransitionData transitionData = {
        .entrypoint = BlGetMapping(BlKernelHeader.Entry),
        .deviceTree = BlDtBuildBlob(),
    };

    BlPrint("Starting kernel\n");
    BxRunOnOtherCpus(BiDoTransition, &transitionData);
    BiDoTransition(&transitionData);
}

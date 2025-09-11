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
#define BI_PROTOCOL_MAJOR 1
#define BI_PROTOCOL_MINOR 0

struct BiKernelHeader {
    uint32_t Magic;
    uint16_t MinorVersion;
    uint16_t MajorVersion;
    uint32_t VirtualAddr;
    uint32_t Entry;
    uint32_t MSize;
};

static uintptr_t BiLoadKernel(void) {
    BlPrint("Loading kernel from %s\n", BlKernelPath);

    struct BlFsFile *file = BlFsFind(BlKernelPath);
    if (!file) BlCrash("failed to open kernel file");

    struct BiKernelHeader header;
    BlFsFileRead(file, &header, sizeof(header), 0);
    if (LE32(header.Magic) != BI_PROTOCOL_MAGIC) BlCrash("invalid magic number");

    header.MinorVersion = LE16(header.MinorVersion);
    header.MajorVersion = LE16(header.MajorVersion);

    if (header.MajorVersion != BI_PROTOCOL_MAJOR) BlCrash("unsupported major version");

    header.VirtualAddr = LE32(header.VirtualAddr);
    header.Entry = LE32(header.Entry);
    header.MSize = LE32(header.MSize);

    if (header.Entry < header.VirtualAddr || header.Entry - header.VirtualAddr >= header.MSize) {
        BlCrash("kernel entry point outside kernel image");
    }

    uint64_t fileSize = BlFsFileSize(file);
    if (fileSize > header.MSize) BlCrash("kernel file too large (0x%lx bytes)", fileSize);

    uintptr_t current = ALIGN_DOWN(header.VirtualAddr, BL_PAGE_SIZE);
    uintptr_t fileEnd = header.VirtualAddr + fileSize;
    uintptr_t alignedFileEnd = ALIGN_DOWN(fileEnd, BL_PAGE_SIZE);
    uintptr_t end = ALIGN_UP(header.VirtualAddr + header.MSize, BL_PAGE_SIZE);

    if (current < header.VirtualAddr) {
        void *buffer = BlAllocateHeap(BL_PAGE_SIZE, BL_PAGE_SIZE, true);
        size_t headCount = header.VirtualAddr - current;
        size_t tailCount = BL_PAGE_SIZE - headCount;
        size_t readCount = MIN(tailCount, fileSize);

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
        BlFsFileRead(file, buffer, BL_PAGE_SIZE, current - header.VirtualAddr);
        BlMapPage(current, (uintptr_t)buffer);
        current += BL_PAGE_SIZE;
    }

    if (current < fileEnd) {
        void *buffer = BlAllocateHeap(BL_PAGE_SIZE, BL_PAGE_SIZE, true);
        size_t headCount = fileEnd - current;
        size_t tailCount = BL_PAGE_SIZE - headCount;

        BlFsFileRead(file, buffer, headCount, current - header.VirtualAddr);
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

    return BlGetMapping(header.Entry);
}

struct BiTransitionData {
    uintptr_t entrypoint;
    void *deviceTree;
};

_Noreturn static void BiDoTransition(void *ptr) {
    struct BiTransitionData *data = ptr;

    BlTransition(data->entrypoint, data->deviceTree, BxNumCpus, BI_PROTOCOL_MINOR);
}

_Noreturn void BlMain(void) {
    BlFindRootPartition();

    struct BiTransitionData transitionData = {
        .entrypoint = BiLoadKernel(),
        .deviceTree = BlDtBuildBlob(),
    };

    BlPrint("Starting kernel\n");
    BxRunOnOtherCpus(BiDoTransition, &transitionData);
    BiDoTransition(&transitionData);
}

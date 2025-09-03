#include "main.h"

#include "compiler.h"
#include "config.h"
#include "dt.h"
#include "filesystem.h"
#include "logging.h"
#include "memory.h"
#include "partition.h"
#include "platform.h"
#include "transition.h"

#define BI_PROTOCOL_MAGIC 0x584c5258
#define BI_PROTOCOL_MAJOR 0
#define BI_PROTOCOL_MINOR 0

struct BiKernelHeader {
    uint32_t Magic;
    uint16_t MinorVersion;
    uint16_t MajorVersion;
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

    header.Entry = LE32(header.Entry);
    header.MSize = LE32(header.MSize);

    if (header.Entry >= header.MSize) BlCrash("kernel entry point outside kernel image");

    uint64_t size = BlFsFileSize(file);
    if (size > header.MSize) BlCrash("kernel file too large (0x%lx bytes)", size);

    void *buffer = BlAllocateHeap(header.MSize, 0x1000);
    BlFsFileRead(file, buffer, size, 0);
    BlFsFree(file);
    BlFillMemory(buffer + size, 0, header.MSize - size);

    return (uintptr_t)buffer + header.Entry;
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

#include "partition.h"
#include "compiler.h"
#include "config.h"
#include "filesystem.h"
#include "list.h"
#include "logging.h"
#include "platform.h"
#include "platformdefs.h"

#define BI_BCACHE_SHIFT 12
#define BI_BCACHE_SIZE (1u << BI_BCACHE_SHIFT)
#define BI_BCACHE_MASK (BI_BCACHE_SIZE - 1)

_Static_assert(BI_BCACHE_SHIFT >= BL_SECTOR_SHIFT, "BI_BCACHE_SHIFT must be larger than BL_SECTOR_SHIFT");

#define BL_BCACHE_COUNT 16

struct BiBCacheMeta {
    struct BlListNode Node;
    uint64_t Block;
    void *Data;
};

_Alignas(BL_BCACHE_ALIGN) static unsigned char BiBCacheData[BL_BCACHE_COUNT][BI_BCACHE_SIZE];
static struct BiBCacheMeta BiBCacheMeta[BL_BCACHE_COUNT];
static size_t BiBCacheUsed;

static struct BlList BiBCache;

static void *BiGetBCacheEntry(uint64_t block) {
    BL_LIST_FOREACH(BiBCache, struct BiBCacheMeta, Node, entry) {
        if (entry->Block == block) {
            BlListRemove(&BiBCache, &entry->Node);
            BlListInsertAfter(&BiBCache, nullptr, &entry->Node);
            return entry->Data;
        }
    }

    struct BiBCacheMeta *entry;

    if (BiBCacheUsed < BL_BCACHE_COUNT) {
        entry = &BiBCacheMeta[BiBCacheUsed];
        entry->Data = &BiBCacheData[BiBCacheUsed];
        BiBCacheUsed += 1;
    } else {
        entry = BL_CONTAINER(struct BiBCacheMeta, Node, BiBCache.Tail);
        BlListRemove(&BiBCache, &entry->Node);
    }

    if (!BxReadFromDisk(
            entry->Data,
            block << (BI_BCACHE_SHIFT - BL_SECTOR_SHIFT),
            1ull << (BI_BCACHE_SHIFT - BL_SECTOR_SHIFT)
        )) {
        BlCrash("failed to read from disk");
    }

    entry->Block = block;
    BlListInsertAfter(&BiBCache, nullptr, &entry->Node);

    return entry->Data;
}

static void BiReadFromDisk(void *buffer, uint64_t position, size_t count, bool bypassCache) {
    if (bypassCache) {
        if (position & BL_SECTOR_MASK) BlCrash("BiReadFromDisk: unaligned position");
        if (count & BL_SECTOR_MASK) BlCrash("BiReadFromDisk: unaligned size");

        BxReadFromDisk(buffer, position >> BL_SECTOR_SHIFT, count >> BL_SECTOR_SHIFT);
    } else {
        while (count != 0) {
            uint64_t block = position >> BI_BCACHE_SHIFT;
            size_t offset = position & BI_BCACHE_MASK;
            size_t current = BL_MIN(BI_BCACHE_SIZE - offset, count);

            BlCopyMemory(buffer, BiGetBCacheEntry(block) + offset, current);

            buffer += current;
            position += current;
            count -= current;
        }
    }
}

#define BI_MBR_OFFSET 440
#define BI_MBR_SIGNATURE 0xaa55

struct MbrPartition {
    uint8_t BootIndicator;
    uint8_t StartingChs[3];
    uint8_t Type;
    uint8_t EndingChs[3];
    uint32_t StartingLba;
    uint32_t SizeInLba;
} __attribute__((packed));

struct Mbr {
    uint32_t DiskId;
    uint16_t Unknown;
    struct MbrPartition Partitions[4];
    uint16_t Signature;
} __attribute__((packed, aligned(4)));

struct BlPartition {
    uint64_t Start;
    uint64_t Size;
};

static struct BlPartition BlRootPartition;

void BlFindRootPartition(void) {
    BlPrint("Searching for root partition\n");

    struct Mbr mbr;
    BiReadFromDisk(&mbr, BI_MBR_OFFSET, sizeof(mbr), false);

    if (BL_LE32(mbr.Signature) != BI_MBR_SIGNATURE) BlCrash("invalid mbr");

    for (size_t i = 0; i < BL_ARRAY_SIZE(mbr.Partitions); i++) {
        struct MbrPartition *mbrPart = &mbr.Partitions[i];
        if (mbrPart->BootIndicator != 0 && mbrPart->BootIndicator != 0x80) BlCrash("invalid mbr");
    }

    for (size_t i = 0; i < BL_ARRAY_SIZE(mbr.Partitions); i++) {
        struct MbrPartition *mbrPart = &mbr.Partitions[i];
        if (mbrPart->Type == 0 || BL_LE32(mbrPart->SizeInLba) == 0) continue;

        BlRootPartition.Start = (uint64_t)BL_LE32(mbrPart->StartingLba) << BL_SECTOR_SHIFT;
        BlRootPartition.Size = (uint64_t)BL_LE32(mbrPart->SizeInLba) << BL_SECTOR_SHIFT;

        if (!BlFsInitialize()) continue;

        auto file = BlFsFind("xrlinux.cfg");
        if (!file) continue;
        BlLoadConfigurationFromFile(file);
        BlFsFree(file);
        return;
    }

    BlCrash("failed to find root partition");
}

void BlReadFromPartition(void *buffer, uint64_t position, size_t count, bool bypassCache) {
    uint64_t end = position + count;
    if (end < position || end > BlRootPartition.Size) BlCrash("tried to read beyond partition bounds");

    BiReadFromDisk(buffer, BlRootPartition.Start + position, count, bypassCache);
}

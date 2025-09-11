#include "filesystem.h"
#include "compiler.h"
#include "logging.h"
#include "memory.h"
#include "partition.h"
#include <stdint.h>

#define BI_SUPERBLOCK_OFFSET 1024
#define BI_SIGNATURE 0xef53
#define BI_ROOT_INODE 2

#define BI_DIR_TYPES (1u << 1)
#define BI_RO_FEATURES BI_DIR_TYPES

#define BI_SIZE_64 (1u << 1)

#define BI_TYPE(mode) ((mode) & 0xf000)
#define BI_TYPE_DIR 0x4000
#define BI_TYPE_REG 0x8000
#define BI_TYPE_SYM 0xa000

#define BI_MAX_NAME_LEN 0xff
#define BI_MAX_SYMLINKS 5

#define BI_INDIRECT_LEVELS 3

struct BiSuperblock {
    uint32_t _Inodes;
    uint32_t _Blocks;
    uint32_t _RootBlocks;
    uint32_t _UnallocatedBlocks;
    uint32_t _UnallocatedInodes;
    uint32_t _SuperblockBlock;
    uint32_t BlockSizeShift;
    uint32_t _FragmentSizeShift;
    uint32_t BlockGroupBlocks;
    uint32_t _BlockGroupFragments;
    uint32_t BlockGroupInodes;
    uint32_t _MountTime;
    uint32_t _WriteTime;
    uint16_t _MountsSinceLastCheck;
    uint16_t _MountsPerCheck;
    uint16_t Signature;
    uint16_t _State;
    uint16_t _ErrorBehavior;
    uint16_t _VersionMinor;
    uint32_t _CheckTime;
    uint32_t _CheckInterval;
    uint32_t _CreatorOs;
    uint32_t VersionMajor;
    uint16_t _RootUid;
    uint16_t _RootGid;
    // following fields only valid pre-canonicalization when VersionMajor >= 1
    uint32_t _FirstUnreservedInode;
    uint16_t InodeSize;
    uint16_t _SuperblockBlockGroup;
    uint32_t OptionalFeatures;
    uint32_t RequiredFeatures;
    uint32_t WriteRequiredFeatures;
    uint8_t FilesystemId[16];
    char VolumeName[16];
    char LastMountpoint[64];
    uint32_t _Compression;
    uint8_t FilePreallocateCount;
    uint8_t DirectoryPreallocateCount;
    uint16_t _Unused;
    uint8_t JournalId[16];
    uint32_t _JournalInode;
    uint32_t _JournalDevice;
    uint32_t _OrphanedInodesHead;
} __attribute__((packed, aligned(4)));

struct BiBlockGroupDescriptor {
    uint32_t _BlockBitmap;
    uint32_t _InodeBitmap;
    uint32_t InodeTableBlock;
    uint16_t _UnallocatedBlocks;
    uint16_t _UnallocatedInodes;
    uint16_t _Directories;
} __attribute__((packed, aligned(4)));

struct BiInode {
    uint16_t Mode;
    uint16_t _Uid;
    uint32_t Size;
    uint32_t _Atime;
    uint32_t _Ctime;
    uint32_t _Mtime;
    uint32_t _Dtime;
    uint16_t _Gid;
    uint16_t _Links;
    uint32_t _Sectors;
    uint32_t _Flags;
    uint32_t _Osv1;
    uint32_t DirectBlocks[12];
    uint32_t IndirectBlocks[BI_INDIRECT_LEVELS];
    /*uint32_t DoubleIndirectBlock;
    uint32_t TripleIndirectBlock;*/
    uint32_t _Generation;
    uint32_t _ExtendedAttributesBlock;
    uint32_t SizeUpper;
    uint32_t _FragmentBlock;
    uint8_t Osv2[12];
} __attribute__((packed, aligned(4)));

struct BiEntry {
    uint32_t Inode;
    uint16_t Size;
    uint8_t NameLength;
    uint8_t Type;
} __attribute__((packed, aligned(4)));

struct BlFsFile {
    struct BiInode Inode;
};

static struct BiSuperblock BiSuperblock;
static size_t BiBlockSize;
static uint64_t BiBgdtLocation;
static uint32_t BiInodeShift;
static uint32_t BiIndirectionShift;
static uint32_t BiIndirectionCount;
static uint32_t BiIndirectionMask;
static struct BiInode BiRoot;

static void BiReadBlockGroupDescriptor(struct BiBlockGroupDescriptor *out, uint32_t group) {
    BlReadFromPartition(out, BiBgdtLocation + (uint64_t)group * 32, sizeof(*out));
    out->InodeTableBlock = BL_LE32(out->InodeTableBlock);
}

static void BiReadInode(struct BiInode *out, uint32_t inode) {
    uint32_t group = (inode - 1) / BiSuperblock.BlockGroupInodes;
    uint32_t index = (inode - 1) % BiSuperblock.BlockGroupInodes;
    uint64_t offset = (uint64_t)index << BiInodeShift;

    struct BiBlockGroupDescriptor groupDescriptor;
    BiReadBlockGroupDescriptor(&groupDescriptor, group);

    uint64_t location = ((uint64_t)groupDescriptor.InodeTableBlock << BiSuperblock.BlockSizeShift) + offset;
    BlReadFromPartition(out, location, sizeof(*out));

    for (size_t i = 0; i < BL_ARRAY_SIZE(out->DirectBlocks); i++) {
        out->DirectBlocks[i] = BL_LE32(out->DirectBlocks[i]);
    }

    for (size_t i = 0; i < BI_INDIRECT_LEVELS; i++) {
        out->IndirectBlocks[i] = BL_LE32(out->IndirectBlocks[i]);
    }

    out->Mode = BL_LE16(out->Mode);
    out->Size = BL_LE32(out->Size);

    if (BiSuperblock.WriteRequiredFeatures & BI_SIZE_64) out->SizeUpper = BL_LE32(out->SizeUpper);
}

static uint32_t BiReadFromPointerBlock(uint32_t block, uint32_t index) {
    uint32_t value;
    BlReadFromPartition(&value, (block << BiSuperblock.BlockSizeShift) + index * sizeof(value), sizeof(value));
    return BL_LE32(value);
}

static uint64_t BiGetInodeBlockBase(struct BiInode *inode, uint64_t block) {
    if (block < BL_ARRAY_SIZE(inode->DirectBlocks)) return inode->DirectBlocks[block];
    block -= BL_ARRAY_SIZE(inode->DirectBlocks);

    uint32_t blocks = BiIndirectionCount;
    size_t level;

    for (level = 0; level < BI_INDIRECT_LEVELS; level++) {
        if (block < blocks) break;
        block -= blocks;
        blocks <<= BiIndirectionShift;
    }

    if (level >= BI_INDIRECT_LEVELS) BlCrash("tried to read beyond inode maximum bounds");

    uint32_t volumeBlock = inode->IndirectBlocks[level++];

    while (level > 0) {
        if (!volumeBlock) break;

        uint32_t index = (block >> (BiIndirectionShift * --level)) & BiIndirectionMask;
        volumeBlock = BiReadFromPointerBlock(volumeBlock, index);
    }

    return volumeBlock;
}

static void BiReadFromInode(struct BiInode *inode, void *buffer, size_t size, uint64_t position) {
    while (size) {
        uint64_t block = position >> BiSuperblock.BlockSizeShift;
        size_t offset = position & (BiBlockSize - 1);
        size_t current = BL_MIN(size, BiBlockSize - offset);

        uint64_t blockBase = BiGetInodeBlockBase(inode, block) << BiSuperblock.BlockSizeShift;

        if (blockBase) {
            BlReadFromPartition(buffer, blockBase + offset, current);
        } else {
            BlFillMemory(buffer, 0, current);
        }

        buffer += current;
        size -= current;
        position += current;
    }
}

static uint64_t BiInodeSize(struct BiInode *inode) {
    uint64_t size = inode->Size;

    if ((BiSuperblock.WriteRequiredFeatures & BI_SIZE_64) != 0 && BI_TYPE(inode->Mode) != BI_TYPE_DIR) {
        size |= (uint64_t)inode->SizeUpper << 32;
    }

    return size;
}

static bool BiFindEntryInDirectory(struct BiEntry *out, struct BiInode *dir, const void *name, size_t nameLength) {
    if (nameLength > BI_MAX_NAME_LEN) return false;

    uint64_t offset = 0;
    auto buffer = BL_ALLOCATE(char, nameLength);

    while (BiInodeSize(dir) - offset >= sizeof(*out)) {
        BiReadFromInode(dir, out, sizeof(*out), offset);

        out->Inode = BL_LE32(out->Inode);
        out->Size = BL_LE16(out->Size);

        if ((BiSuperblock.RequiredFeatures & BI_DIR_TYPES) != 0 || out->Type == 0) {
            if (out->Inode != 0 && out->NameLength == nameLength) {
                BiReadFromInode(dir, buffer, nameLength, offset + sizeof(*out));

                if (BlCompareMemory(buffer, name, nameLength) == 0) {
                    BlFreeHeap(buffer);
                    return true;
                }
            }
        }

        offset += out->Size;
    }

    BlFreeHeap(buffer);
    return false;
}

bool BlFsInitialize(void) {
    BlReadFromPartition(&BiSuperblock, BI_SUPERBLOCK_OFFSET, sizeof(BiSuperblock));
    if (BL_LE16(BiSuperblock.Signature) != BI_SIGNATURE) return false;

    BiSuperblock.BlockSizeShift = BL_LE32(BiSuperblock.BlockSizeShift) + 10;
    BiSuperblock.BlockGroupBlocks = BL_LE32(BiSuperblock.BlockGroupBlocks);
    BiSuperblock.BlockGroupInodes = BL_LE32(BiSuperblock.BlockGroupInodes);
    BiSuperblock.VersionMajor = BL_LE32(BiSuperblock.VersionMajor);

    if (BiSuperblock.VersionMajor < 1) {
        BiSuperblock.InodeSize = 128;
        BiSuperblock.OptionalFeatures = 0;
        BiSuperblock.RequiredFeatures = 0;
        BiSuperblock.WriteRequiredFeatures = 0;
    } else {
        BiSuperblock.InodeSize = BL_LE16(BiSuperblock.InodeSize);
        BiSuperblock.OptionalFeatures = BL_LE32(BiSuperblock.OptionalFeatures);
        BiSuperblock.RequiredFeatures = BL_LE32(BiSuperblock.RequiredFeatures);
        BiSuperblock.WriteRequiredFeatures = BL_LE32(BiSuperblock.WriteRequiredFeatures);
    }

    uint32_t missingFeatures = BiSuperblock.RequiredFeatures & ~BI_RO_FEATURES;

    if (missingFeatures != 0) {
        BlPrint("BlFsInitialize: missing required filesystem features 0x%x\n", missingFeatures);
        return false;
    }

    if (BiSuperblock.InodeSize & (BiSuperblock.InodeSize - 1)) {
        BlPrint("BlFsInitialize: inode size %u is not a power of two\n", BiSuperblock.InodeSize);
        return false;
    }

    BiBlockSize = 1u << BiSuperblock.BlockSizeShift;
    BiBgdtLocation = ((BI_SUPERBLOCK_OFFSET >> BiSuperblock.BlockSizeShift) + 1) << BiSuperblock.BlockSizeShift;
    BiInodeShift = BlCountTrailingZeroes(BiSuperblock.InodeSize);
    BiIndirectionShift = BiSuperblock.BlockSizeShift - 2;
    BiIndirectionCount = 1u << BiIndirectionShift;
    BiIndirectionMask = BiIndirectionCount - 1;

    BiReadInode(&BiRoot, BI_ROOT_INODE);
    return true;
}

static void BiMaybeFreeInode(struct BiInode *inode) {
    if (inode != &BiRoot) BlFreeHeap(inode);
}

static struct BiInode *BiFindInode(struct BiInode *inode, const char *path, size_t length, uint32_t symlinks) {
    if (symlinks == BI_MAX_SYMLINKS) return nullptr;
    if (length == 0) return nullptr;
    if (path[0] == '/') inode = &BiRoot;

    struct BiEntry entry;

    do {
        if (BI_TYPE(inode->Mode) != BI_TYPE_DIR) {
            BiMaybeFreeInode(inode);
            return nullptr;
        }

        while (length > 0 && path[0] == '/') {
            path++;
            length--;
        }

        if (length == 0) break;

        size_t componentLength = 0;
        while (componentLength < length && path[componentLength] != '/') componentLength++;

        if (!BiFindEntryInDirectory(&entry, inode, path, componentLength)) {
            BiMaybeFreeInode(inode);
            return nullptr;
        }

        auto newInode = BL_ALLOCATE(struct BiInode, 1);
        BiReadInode(newInode, entry.Inode);

        if (BI_TYPE(newInode->Mode) == BI_TYPE_SYM) {
            auto size = BiInodeSize(newInode);
            auto linkPath = BL_ALLOCATE(char, size);
            BiReadFromInode(newInode, linkPath, size, 0);
            BiMaybeFreeInode(newInode);
            newInode = BiFindInode(inode, linkPath, size, symlinks + 1);
            BlFreeHeap(linkPath);
        }

        BiMaybeFreeInode(inode);
        inode = newInode;

        if (!inode) return nullptr;

        path += componentLength;
        length -= componentLength;
    } while (length > 0);

    return inode;
}

struct BlFsFile *BlFsFind(const char *path) {
    struct BiInode *inode = BiFindInode(&BiRoot, path, BlStringLength(path), 0);
    if (!inode) return nullptr;

    if (BI_TYPE(inode->Mode) != BI_TYPE_REG) {
        if (inode != &BiRoot) BlFreeHeap(inode);
        return nullptr;
    }

    return (struct BlFsFile *)inode;
}

uint64_t BlFsFileSize(struct BlFsFile *file) {
    return BiInodeSize(&file->Inode);
}

void BlFsFileRead(struct BlFsFile *file, void *buffer, size_t count, uint64_t position) {
    if (!count) return;

    uint64_t fileSize = BiInodeSize(&file->Inode);
    if (position > fileSize) return;

    uint64_t avail = position - fileSize;
    if (count > avail) return;

    BiReadFromInode(&file->Inode, buffer, count, position);
}

void BlFsFree(struct BlFsFile *file) {
    BiMaybeFreeInode(&file->Inode);
}

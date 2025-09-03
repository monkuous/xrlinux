#include "dt.h"
#include "asm.h"
#include "list.h"
#include "logging.h"
#include "memory.h"
#include <stddef.h>
#include <stdint.h>

struct BlDtRsvMem {
    uint64_t Address;
    uint64_t Size;
};

struct BlDtString {
    struct BlListNode Node;
    char *Data;
    uint32_t Identifier;
};

struct BlDtProperty {
    struct BlDtString *Name;
    void *Data;
    uint32_t Size;
};

struct BlDtNode {
    struct BlDtNode *Parent;
    struct BlListNode Node;
    char *Name;
    struct BlList Children;
    struct BlDtProperty *Properties;
    size_t NumberOfProperties;
};

#define BL_DT_NODE_SIZE(nameLength) (8 + ALIGN_UP((nameLength) + 1, 4))
#define BL_DT_PROP_SIZE(dataSize) (12 + ALIGN_UP(dataSize, 4))

static struct BlDtRsvMem *BlDtRsvMem;
static size_t BlDtRsvMemCount;

static struct BlList *BlDtStrings;
static size_t BlDtStringsCount;
static size_t BlDtStringsCapacity;
static size_t BlDtStringsSize;

static struct BlDtNode BlDtRootNode = {.Name = ""};
static size_t BlDtStructureSize = 16; // FDT_BEGIN_NODE("") [8], FDT_END_NODE [4], FDT_END [4]

static uint32_t BlDtHash(const char *str) {
    uint32_t hash = 0x811c9dc5;
    unsigned char c;

    while ((c = *str++) != 0) {
        hash ^= c;
        hash *= 0x01000193;
    }

    return hash;
}

static char *BlDuplicateString(const char *str) {
    size_t len = BlStringLength(str);
    auto mem = BL_ALLOCATE(char, len + 1);
    BlCopyMemory(mem, str, len + 1);
    return mem;
}

static struct BlDtString *BlDtGetString(const char *str) {
    uint32_t hash = BlDtHash(str);

    if (BlDtStringsCount) {
        size_t bucket = hash & (BlDtStringsCapacity - 1);

        BL_LIST_FOREACH(BlDtStrings[bucket], struct BlDtString, Node, entry) {
            if (BlCompareStrings(entry->Data, str) == 0) {
                return entry;
            }
        }
    }

    if (BlDtStringsCount >= (BlDtStringsCapacity - (BlDtStringsCapacity / 4))) {
        size_t newCapacity = BlDtStringsCapacity ? BlDtStringsCapacity * 2 : 8;
        auto newTable = BL_ALLOCATE(struct BlList, newCapacity);
        BlFillMemory(newTable, 0, newCapacity * sizeof(*newTable));

        for (size_t i = 0; i < BlDtStringsCapacity; i++) {
            struct BlDtString *cur = BL_LIST_HEAD(struct BlDtString, Node, BlDtStrings[i]);

            while (cur) {
                struct BlDtString *next = BL_LIST_NEXT(struct BlDtString, Node, *cur);
                size_t newBucket = BlDtHash(cur->Data) & (newCapacity - 1);
                BlListInsertAfter(&newTable[newBucket], nullptr, &cur->Node);
                cur = next;
            }
        }

        BlFreeHeap(BlDtStrings);
        BlDtStrings = newTable;
        BlDtStringsCapacity = newCapacity;
    }

    auto entry = BL_ALLOCATE(struct BlDtString, 1);
    entry->Data = BlDuplicateString(str);

    size_t bucket = hash & (BlDtStringsCapacity - 1);
    BlListInsertAfter(&BlDtStrings[bucket], nullptr, &entry->Node);
    BlDtStringsCount += 1;
    BlDtStringsSize += BlStringLength(str) + 1;

    return entry;
}

void BlDtAddReservedMemory(uint64_t base, uint64_t size) {
    size_t index = BlDtRsvMemCount++;

    BlDtRsvMem = BL_RESIZE(typeof(*BlDtRsvMem), BlDtRsvMem, BlDtRsvMemCount);
    BlDtRsvMem[index].Address = base;
    BlDtRsvMem[index].Size = size;
}

struct BlDtNode *BlDtCreateNode(struct BlDtNode *parent, const char *name) {
    if (!parent) parent = &BlDtRootNode;

    auto node = BL_ALLOCATE(struct BlDtNode, 1);
    node->Parent = parent;
    node->Name = BlDuplicateString(name);
    BlListInit(&node->Children);
    node->Properties = nullptr;
    node->NumberOfProperties = 0;

    BlListInsertBefore(&parent->Children, nullptr, &node->Node);
    BlDtStructureSize += BL_DT_NODE_SIZE(BlStringLength(node->Name));

    return node;
}

static void BlDtDoAddProperty(struct BlDtNode *parent, const char *name, void *data, uint32_t size) {
    if (!parent) parent = &BlDtRootNode;

    size_t index = parent->NumberOfProperties++;
    parent->Properties = BL_RESIZE(struct BlDtProperty, parent->Properties, parent->NumberOfProperties);
    parent->Properties[index].Name = BlDtGetString(name);
    parent->Properties[index].Data = data;
    parent->Properties[index].Size = size;

    BlDtStructureSize += BL_DT_PROP_SIZE(size);
}

void BlDtAddProperty(struct BlDtNode *parent, const char *name, const void *data, uint32_t size) {
    void *buffer = BL_ALLOCATE(unsigned char, size);
    BlCopyMemory(buffer, data, size);
    BlDtDoAddProperty(parent, name, buffer, size);
}

void BlDtAddPropertyU32s(struct BlDtNode *parent, const char *name, const uint32_t *data, uint32_t count) {
    auto buffer = BL_ALLOCATE(uint32_t, count);

    for (uint32_t i = 0; i < count; i++) {
        buffer[i] = BE32(data[i]);
    }

    BlDtDoAddProperty(parent, name, buffer, count * 4);
}

void BlDtAddPropertyStrings(struct BlDtNode *parent, const char *name, const char **data, size_t count) {
    size_t size = 0;
    for (size_t i = 0; i < count; i++) size += BlStringLength(data[i]) + 1;

    void *buffer = BL_ALLOCATE(unsigned char, size);
    size = 0;

    for (size_t i = 0; i < count; i++) {
        const char *str = data[i];
        size_t length = BlStringLength(str) + 1;
        BlCopyMemory(buffer + size, str, length);
        size += length;
    }

    BlDtDoAddProperty(parent, name, buffer, size);
}

uint32_t BlDtAllocPhandle(void) {
    static uint32_t next;
    return ++next;
}

struct BlFdtHeader {
    uint32_t Magic;
    uint32_t TotalSize;
    uint32_t OffDtStruct;
    uint32_t OffDtStrings;
    uint32_t OffMemRsvmap;
    uint32_t Version;
    uint32_t LastCompVersion;
    uint32_t BootCpuidPhys;
    uint32_t SizeDtStrings;
    uint32_t SizeDtStruct;
} __attribute__((packed, aligned(8)));

struct BlFdtRsvmapEntry {
    uint64_t Address;
    uint64_t Size;
} __attribute__((packed, aligned(8)));

#define BL_FDT_MAGIC 0xd00dfeed
#define BL_FDT_VERSION 17
#define BL_FDT_COMP_VERSION 16

#define BL_FDT_BEGIN_NODE 1
#define BL_FDT_END_NODE 2
#define BL_FDT_PROP 3
#define BL_FDT_END 9

static void BlDtAddDataAndAlign(void *structure, size_t *offset, const void *data, size_t size) {
    size_t aligned = ALIGN_UP(size, 4);
    size_t curOff = *offset;
    size_t newOff = curOff + aligned;
    ASSERT(newOff <= BlDtStructureSize);

    BlCopyMemory(structure + curOff, data, size);
    BlFillMemory(structure + curOff + size, 0, aligned - size);

    *offset = newOff;
}

static void BlDtAddToken(void *structure, size_t *offset, uint32_t token) {
    size_t curOff = *offset;
    size_t newOff = curOff + 4;
    ASSERT(newOff <= BlDtStructureSize);

    *(uint32_t *)(structure + curOff) = BE32(token);

    *offset = newOff;
}

void *BlDtBuildBlob(void) {
    BlPrint("Creating device tree blob\n");

    size_t rsvMapSize = (BlDtRsvMemCount + 1) * sizeof(struct BlFdtRsvmapEntry);
    size_t totalSize = sizeof(struct BlFdtHeader) + rsvMapSize + BlDtStructureSize + BlDtStringsSize;

    struct BlFdtHeader *header = BlAllocateHeap(totalSize, _Alignof(struct BlFdtHeader));
    struct BlFdtRsvmapEntry *rsvmap = (void *)header + sizeof(*header);
    void *structure = (void *)rsvmap + rsvMapSize;
    void *strings = structure + BlDtStructureSize;

    header->Magic = BE32(BL_FDT_MAGIC);
    header->TotalSize = BE32(totalSize);
    header->OffDtStruct = BE32(sizeof(*header) + rsvMapSize);
    header->OffDtStrings = BE32(sizeof(*header) + rsvMapSize + BlDtStructureSize);
    header->OffMemRsvmap = BE32(sizeof(*header));
    header->Version = BE32(BL_FDT_VERSION);
    header->LastCompVersion = BE32(BL_FDT_COMP_VERSION);
    header->BootCpuidPhys = BE32(BlReadWhami());
    header->SizeDtStrings = BE32(BlDtStringsSize);
    header->SizeDtStruct = BE32(BlDtStructureSize);

    // Build rsvmap
    size_t i;
    for (i = 0; i < BlDtRsvMemCount; i++) {
        rsvmap[i].Address = BlDtRsvMem[i].Address;
        rsvmap[i].Size = BlDtRsvMem[i].Size;
    }
    rsvmap[i].Address = 0;
    rsvmap[i].Size = 0;

    // Build strings (this has to be done before structure to ensure BlDtString.Identifier is correct)
    size_t offset = 0;
    for (i = 0; i < BlDtStringsCapacity; i++) {
        BL_LIST_FOREACH(BlDtStrings[i], struct BlDtString, Node, str) {
            size_t size = BlStringLength(str->Data) + 1;
            size_t newOffset = offset + size;
            ASSERT(newOffset <= BlDtStringsSize);

            str->Identifier = offset;
            BlCopyMemory(strings + offset, str->Data, size);

            offset = newOffset;
        }
    }
    ASSERT(offset == BlDtStringsSize);

    // Build structure
    struct BlDtNode *node = &BlDtRootNode;
    struct BlDtNode *previous = nullptr;
    offset = 0;

    while (node) {
        if (previous == node->Parent) {
            BlDtAddToken(structure, &offset, BL_FDT_BEGIN_NODE);
            BlDtAddDataAndAlign(structure, &offset, node->Name, BlStringLength(node->Name) + 1);

            for (i = 0; i < node->NumberOfProperties; i++) {
                struct BlDtProperty *property = &node->Properties[i];
                BlDtAddToken(structure, &offset, BL_FDT_PROP);
                BlDtAddToken(structure, &offset, property->Size);
                BlDtAddToken(structure, &offset, property->Name->Identifier);
                BlDtAddDataAndAlign(structure, &offset, property->Data, property->Size);
            }

            previous = BL_LIST_HEAD(struct BlDtNode, Node, node->Children);
        } else {
            previous = BL_LIST_NEXT(struct BlDtNode, Node, *previous);
        }

        if (previous) {
            struct BlDtNode *next = previous;
            previous = node;
            node = next;
        } else {
            BlDtAddToken(structure, &offset, BL_FDT_END_NODE);
            previous = node;
            node = node->Parent;
        }
    }

    BlDtAddToken(structure, &offset, BL_FDT_END);
    ASSERT(offset == BlDtStructureSize);

    return header;
}

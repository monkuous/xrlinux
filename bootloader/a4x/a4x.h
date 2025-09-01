#pragma once

#include <stddef.h>
#include <stdint.h>

struct FwRamBank {
    uint32_t PageFrameCount;
};

struct FwBootInfo {
    char OsName[16];
    uint32_t BootstrapSector;
    uint32_t BootstrapCount;
    char Badge[512];
    uint8_t Legacy;
};

struct FwPartition {
    char Label[8];
    uint32_t BaseSector;
    uint32_t SectorCount;
    struct FwBootInfo *BootInfo;
    uint8_t Id;
    uint8_t PartitionId;
};

struct FwDisk {
    char Label[16];
    struct FwPartition Partitions[9];
};

struct FwAmtsu {
    uint32_t Mid;
};

struct FwBoard {
    void *Address;
    char Name[16];
    uint32_t BoardId;
};

struct FwProcessor {
    uint8_t Present;
};

enum FwFramebufferType : uint8_t {
    FW_FRAMEBUFFER_ABSENT = 0,
    FW_FRAMEBUFFER_KINNOWFB,
};

struct FwFramebuffer {
    volatile void *Address;
    uint16_t Width;
    uint16_t Height;
    enum FwFramebufferType Type;
};

struct FwKeyboard {
    uint8_t Id;
};

enum FwMachineType : uint8_t {
    FW_XR_STATION,
    FW_XR_MP,
    FW_XR_FRAME,
    FW_XR_TYPE_MAX,
};

struct FwDeviceDatabase {
    uint32_t TotalRamBytes;
    uint8_t ProcessorCount;
    uint8_t BootableCount;
    uint8_t Padding[2];
    struct FwRamBank RamBanks[8];
    struct FwDisk Disks[8];
    struct FwAmtsu Amtsu[16];
    struct FwBoard Boards[7];
    struct FwProcessor Processors[8];
    struct FwFramebuffer Framebuffer;
    struct FwKeyboard Keyboard;
    enum FwMachineType MachineType;
};

struct FwApiTable {
    void (*PutCharacter)(size_t character);
    size_t (*GetCharacter)(void);
    size_t (*ReadDisk)(struct FwPartition *partition, void *buffer, uint32_t sector, uint32_t count);
    void (*PutString)(const char *str);
    void (*KickProcessor)(size_t number, void *context, void (*callback)(size_t number, void *context));
};

extern struct FwApiTable *BxApiTable;

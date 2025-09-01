#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define APT_MAGIC 0x4e4d494d
#define APT_BOOT_MAGIC 0x796d6173
#define APT_START_SECTOR 4
#define APT_BOOT_SECTORS 3
#define MBR_SIGNATURE 0xaa55
#define SECTOR_SIZE 512
#define DEFAULT_AVAIL_SIZE 0x100000
#define BUFFER_SIZE 0x10000

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define LE16(x) (x)
#define LE32(x) (x)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define LE16(x) __builtin_bswap16(x)
#define LE32(x) __builtin_bswap32(x)
#else
#error "Unsupported byte order"
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

struct AptEntry {
    uint8_t Label[8];
    uint32_t SectorCount;
    uint32_t Status;
} __attribute__((packed));

struct AptBootBlock {
    uint8_t BootCode[15];
    uint8_t FfIfVariant;
    struct AptEntry Partitions[8];
    uint32_t Magic;
    uint8_t Label[16];
} __attribute__((packed));

struct AptOsRecord {
    uint32_t Magic;
    uint8_t OsName[16];
    uint32_t BootstrapSector;
    uint32_t BootstrapCount;
} __attribute__((packed));

struct MbrPartition {
    uint8_t BootIndicator;
    uint8_t StartingChs[3];
    uint8_t OsType;
    uint8_t EndingChs[3];
    uint32_t StartingLba;
    uint32_t SizeInLba;
} __attribute__((packed));

struct Mbr {
    union {
        uint8_t BootCode[424];
        struct AptBootBlock Apt;
    };
    uint8_t Reserved[16];
    uint32_t UniqueMbrDiskSignature;
    uint16_t Unknown;
    struct MbrPartition PartitionRecord[4];
    uint16_t Signature;
} __attribute__((packed));

_Static_assert(sizeof(struct Mbr) == 512, "Invalid size for struct Mbr");

static const char *AppName;
static const uint8_t OsLogo[512];

_Noreturn static void PerrorDie(const char *message) {
    fprintf(stderr, "%s: %s: %s\n", AppName, message, strerror(errno));
    exit(1);
}

_Noreturn static void ErrorDie(const char *message) {
    fprintf(stderr, "%s: %s\n", AppName, message);
    exit(1);
}

static void ReadFully(int fd, void *buffer, size_t size, off_t offset) {
    while (size != 0) {
        ssize_t current = pread(fd, buffer, size, offset);
        if (current < 0) PerrorDie("read failed");

        buffer += current;
        size -= current;
        offset += current;
    }
}

static void WriteFully(int fd, const void *buffer, size_t size, off_t offset) {
    while (size != 0) {
        ssize_t current = pwrite(fd, buffer, size, offset);
        if (current < 0) PerrorDie("write failed");

        buffer += current;
        size -= current;
        offset += current;
    }
}

static void CopyData(int src, int dst, size_t size, off_t srcPos, off_t dstPos) {
    unsigned char buffer[BUFFER_SIZE];

    while (size != 0) {
        ssize_t current = pread(src, buffer, sizeof(buffer) <= size ? sizeof(buffer) : size, srcPos);
        if (current < 0) perror("read failed");
        WriteFully(dst, buffer, current, dstPos);

        size -= current;
        srcPos += current;
        dstPos += current;
    }
}

int main(int argc, char *argv[]) {
    AppName = argv[0];

    if (argc != 4) {
        fprintf(stderr, "usage: %s DISK IMAGE NAME\n", AppName);
        return 2;
    }

    const char *name = argv[3];
    if (strlen(name) > 15) ErrorDie("name too long (must be at most 7 characters)");

    int disk = open(argv[1], O_RDWR);
    if (disk < 0) PerrorDie("failed to open disk");

    struct Mbr mbr;
    ReadFully(disk, &mbr, sizeof(mbr), 0);
    if (LE16(mbr.Signature) != MBR_SIGNATURE) ErrorDie("invalid mbr");

    uint32_t aptPartitionStart = APT_START_SECTOR;
    uint32_t aptPartitionEnd = DEFAULT_AVAIL_SIZE / SECTOR_SIZE;

    for (int i = 0; i < ARRAY_SIZE(mbr.PartitionRecord); i++) {
        struct MbrPartition *part = &mbr.PartitionRecord[i];
        if (part->OsType == 0 || part->SizeInLba == 0) continue;
        if (part->StartingLba < aptPartitionEnd) aptPartitionEnd = part->StartingLba;
    }

    if (aptPartitionEnd <= aptPartitionStart) ErrorDie("no space on disk for apt");

    uint32_t aptPartitionAvailable = aptPartitionEnd - aptPartitionStart;

    int image = open(argv[2], O_RDONLY);
    if (image < 0) PerrorDie("failed to open image");

    struct stat st;
    if (fstat(image, &st)) PerrorDie("failed to stat image");

    uint32_t imageSectors = (st.st_size + (SECTOR_SIZE - 1)) / SECTOR_SIZE;
    uint32_t neededSectors = APT_BOOT_SECTORS + imageSectors;
    if (neededSectors > aptPartitionAvailable) ErrorDie("image too large for free area");

    // Copy image data to disk
    CopyData(image, disk, st.st_size, 0, (aptPartitionStart + APT_BOOT_SECTORS) * SECTOR_SIZE);

    // Write image header to disk
    struct AptOsRecord osRecord = {
        .Magic = APT_BOOT_MAGIC,
        .BootstrapSector = APT_BOOT_SECTORS,
        .BootstrapCount = imageSectors,
    };
    memcpy(osRecord.OsName, name, strlen(name));

    WriteFully(disk, &osRecord, sizeof(osRecord), (aptPartitionStart + 1) * SECTOR_SIZE);
    WriteFully(disk, OsLogo, sizeof(OsLogo), (aptPartitionStart + 2) * SECTOR_SIZE);

    // Finally, create the actual partition table and write it to disk
    // This is done last so that the effects of the previous steps are invisible unless everything is successful
    memset(&mbr.Apt, 0, sizeof(mbr.Apt));
    mbr.Apt.FfIfVariant = 0xff;
    mbr.Apt.Magic = APT_MAGIC;
    mbr.Apt.Partitions[0].SectorCount = aptPartitionAvailable;
    mbr.Apt.Partitions[0].Status = 1;
    WriteFully(disk, &mbr, sizeof(mbr), 0);

    return 0;
}

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

typedef struct {
    uint8_t label[8];
    uint32_t sectorCount;
    uint32_t status;
} __attribute__((packed)) apt_entry_t;

typedef struct {
    uint8_t bootCode[15];
    uint8_t ffIfVariant;
    apt_entry_t partitions[8];
    uint32_t magic;
    uint8_t label[16];
} __attribute__((packed)) apt_boot_block_t;

typedef struct {
    uint32_t magic;
    uint8_t osName[16];
    uint32_t bootstrapSector;
    uint32_t bootstrapCount;
} __attribute__((packed)) apt_os_record_t;

typedef struct {
    uint8_t bootIndicator;
    uint8_t startingChs[3];
    uint8_t osType;
    uint8_t endingChs[3];
    uint32_t startingLba;
    uint32_t sizeInLba;
} __attribute__((packed)) mbr_part_t;

typedef struct {
    union {
        uint8_t bootCode[424];
        apt_boot_block_t apt;
    };
    uint8_t reserved[16];
    uint32_t uniqueMbrDiskSignature;
    uint16_t unknown;
    mbr_part_t partitionRecord[4];
    uint16_t signature;
} __attribute__((packed)) mbr_t;

_Static_assert(sizeof(mbr_t) == 512, "Invalid size for mbr_t");

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

    mbr_t mbr;
    ReadFully(disk, &mbr, sizeof(mbr), 0);
    if (LE16(mbr.signature) != MBR_SIGNATURE) ErrorDie("invalid mbr");

    uint32_t aptPartitionStart = APT_START_SECTOR;
    uint32_t aptPartitionEnd = DEFAULT_AVAIL_SIZE / SECTOR_SIZE;

    for (int i = 0; i < ARRAY_SIZE(mbr.partitionRecord); i++) {
        mbr_part_t *part = &mbr.partitionRecord[i];
        if (part->osType == 0 || part->sizeInLba == 0) continue;
        if (part->startingLba < aptPartitionEnd) aptPartitionEnd = part->startingLba;
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
    apt_os_record_t osRecord = {
        .magic = APT_BOOT_MAGIC,
        .bootstrapSector = APT_BOOT_SECTORS,
        .bootstrapCount = imageSectors,
    };
    memcpy(osRecord.osName, name, strlen(name));

    WriteFully(disk, &osRecord, sizeof(osRecord), (aptPartitionStart + 1) * SECTOR_SIZE);
    WriteFully(disk, OsLogo, sizeof(OsLogo), (aptPartitionStart + 2) * SECTOR_SIZE);

    // Finally, create the actual partition table and write it to disk
    // This is done last so that the effects of the previous steps are invisible unless everything is successful
    memset(&mbr.apt, 0, sizeof(mbr.apt));
    mbr.apt.ffIfVariant = 0xff;
    mbr.apt.magic = APT_MAGIC;
    mbr.apt.partitions[0].sectorCount = aptPartitionAvailable;
    mbr.apt.partitions[0].status = 1;
    WriteFully(disk, &mbr, sizeof(mbr), 0);

    return 0;
}

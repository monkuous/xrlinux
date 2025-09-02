#include "a4x.h"

#include "logging.h"
#include "platform.h"

struct FwApiTable *BxApiTable;
struct FwPartition *BxBootDisk;

void BxPrintCharacter(unsigned char c) {
    BxApiTable->PutCharacter(c);
}

bool BxReadFromDisk(void *buffer, uint64_t sector, size_t count) {
    uint64_t end = sector + count;
    if (end < sector) return false;
    if (end > BxBootDisk->SectorCount) end = BxBootDisk->SectorCount;

    return BxApiTable->ReadDisk(BxBootDisk, buffer, sector, end - sector);
}

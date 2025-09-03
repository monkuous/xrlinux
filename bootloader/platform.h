#pragma once

extern size_t BxNumCpus;

_Noreturn void BxReturnToFirmware(void);
void BxPrintCharacter(unsigned char c);

// read [sector,Min(sector+count,NumberOfSectors)) from the boot disk into buffer
bool BxReadFromDisk(void *buffer, uint64_t sector, size_t count);

void BxRunOnOtherCpus(void (*func)(void *), void *ctx);

#pragma once

#include <stddef.h>
#include <stdint.h>

struct BlPartition;

void BlFindRootPartition(void);

void BlReadFromPartition(void *buffer, uint64_t position, size_t count, bool bypassCache);

uint64_t BlRootPartitionSize(void);

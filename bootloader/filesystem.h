#pragma once

#include <stddef.h>
#include <stdint.h>

struct BlFsFile;

bool BlFsInitialize(void);

struct BlFsFile *BlFsFind(const char *path);
uint64_t BlFsFileSize(struct BlFsFile *file);
void BlFsFileRead(struct BlFsFile *file, void *buffer, size_t count, uint64_t position);
void BlFsFree(struct BlFsFile *file);

#pragma once

#include "filesystem.h"

extern const char *BlKernelPath;

void BlLoadConfigurationFromFile(struct BlFsFile *file);

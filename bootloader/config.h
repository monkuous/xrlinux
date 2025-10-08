#pragma once

#include "filesystem.h"

extern const char *BlKernelPath;
extern const char *BlStdoutPath;

void BlLoadConfigurationFromFile(struct BlFsFile *file);

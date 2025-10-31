#pragma once

#include "filesystem.h"

extern const char *BlKernelPath;
extern const char *BlStdoutPath;
extern const char *BlInitrdPath;

void BlLoadConfigurationFromFile(struct BlFsFile *file);

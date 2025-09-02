#include "main.h"
#include "logging.h"
#include "partition.h"

_Noreturn void BlMain(void) {
    BlFindRootPartition();

    BlCrash("BlMain: TODO");
}

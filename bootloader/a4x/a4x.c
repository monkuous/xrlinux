#include "a4x.h"
#include "platform.h"

struct FwApiTable *BxApiTable;

void BxPrintCharacter(unsigned char c) {
    BxApiTable->PutCharacter(c);
}

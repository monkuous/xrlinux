#include "config.h"
#include "compiler.h"
#include "logging.h"
#include "memory.h"

const char *BlKernelPath;
const char *BlStdoutPath;

enum BiOptionType {
    BI_OPTION_STRING,
};

#define BI_REQUIRED (1u << 0)
#define BI_PROVIDED (1u << 31)

struct BiOption {
    const char *Name;
    enum BiOptionType Type;
    unsigned Flags;
    void *Value;
};

static struct BiOption BiOptions[] = {
    {"KernelPath", BI_OPTION_STRING, BI_REQUIRED, &BlKernelPath},
    {"StdoutPath", BI_OPTION_STRING, 0, &BlStdoutPath},
};

static void BiValidateOptions(void) {
    for (size_t i = 0; i < BL_ARRAY_SIZE(BiOptions); i++) {
        struct BiOption *option = &BiOptions[i];

        if ((option->Flags & (BI_REQUIRED | BI_PROVIDED)) == BI_REQUIRED) {
            BlCrash("BiValidateOptions: missing required option `%s`", option->Name);
        }
    }
}

static bool BiIsWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static struct BiOption *BlFindOption(const char *name, size_t nameLength) {
    for (size_t i = 0; i < BL_ARRAY_SIZE(BiOptions); i++) {
        struct BiOption *option = &BiOptions[i];

        if (BlStringLength(option->Name) == nameLength && BlCompareMemory(option->Name, name, nameLength) == 0) {
            return option;
        }
    }

    return nullptr;
}

static void BiHandleOption(const char *name, size_t nameLength, const char *value, size_t valueLength) {
    struct BiOption *option = BlFindOption(name, nameLength);

    if (!option) {
        BlPrint("BlHandleOption: unknown option `%S`\n", name, nameLength);
        return;
    }

    switch (option->Type) {
    case BI_OPTION_STRING: {
        auto buffer = BL_ALLOCATE(char, valueLength + 1);
        BlCopyMemory(buffer, value, valueLength);
        buffer[valueLength] = 0;
        *(const char **)option->Value = buffer;
        break;
    }
    }

    option->Flags |= BI_PROVIDED;
}

void BlLoadConfigurationFromFile(struct BlFsFile *file) {
    BlPrint("Loading configuration\n");

    uint64_t size = BlFsFileSize(file);
    auto buffer = BL_ALLOCATE(char, size);
    BlFsFileRead(file, buffer, size, 0, false);

    char *start = buffer;
    size_t line = 1;

    while (size > 0) {
        while (size > 0 && BiIsWhitespace(buffer[0])) {
            buffer++;
            size--;
        }

        size_t nameEnd = SIZE_MAX;
        size_t valueEnd = SIZE_MAX;
        size_t lineEnd = 0;

        while (lineEnd < size && buffer[lineEnd] != '\n') {
            if (nameEnd == SIZE_MAX && buffer[lineEnd] == ':') nameEnd = lineEnd;
            if (valueEnd == SIZE_MAX && buffer[lineEnd] == '#') valueEnd = lineEnd;

            lineEnd += 1;
        }

        if (valueEnd == 0 || lineEnd == 0) goto nextLine;

        if (nameEnd == SIZE_MAX) BlCrash("invalid syntax in configuration (line %zu)", line);
        if (valueEnd == SIZE_MAX) valueEnd = lineEnd;

        size_t valueStart = nameEnd + 1;

        while (valueStart < valueEnd && BiIsWhitespace(buffer[valueStart])) valueStart++;
        while (valueStart < valueEnd && BiIsWhitespace(buffer[valueEnd - 1])) valueEnd--;

        BiHandleOption(buffer, nameEnd, &buffer[valueStart], valueEnd - valueStart);

    nextLine:
        if (lineEnd < size) lineEnd++;
        buffer += lineEnd;
        size -= lineEnd;
        line += 1;
    }

    BlFreeHeap(start);
    BiValidateOptions();
}

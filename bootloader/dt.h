#pragma once

#include <stddef.h>
#include <stdint.h>

struct BlDtNode;

void BlDtAddReservedMemory(uint64_t base, uint64_t size);

struct BlDtNode *BlDtCreateNode(struct BlDtNode *parent, const char *name);
void BlDtAddProperty(struct BlDtNode *parent, const char *name, const void *data, uint32_t size);
void BlDtAddPropertyU32s(struct BlDtNode *parent, const char *name, const uint32_t *data, uint32_t count);
void BlDtAddPropertyStrings(struct BlDtNode *parent, const char *name, const char **data, size_t count);
uint32_t BlDtAllocPhandle(void);

struct BlDtNode *BlDtFindNode(struct BlDtNode *parent, const char *name);

const char *BlDtNodeName(struct BlDtNode *node);
char *BlDtNodePath(struct BlDtNode *node);

void *BlDtBuildBlob(void);

static inline struct BlDtNode *BlDtFindOrCreateNode(struct BlDtNode *parent, const char *name) {
    auto node = BlDtFindNode(parent, name);
    if (!node) node = BlDtCreateNode(parent, name);
    return node;
}

static inline void BlDtAddPropertyU32(struct BlDtNode *parent, const char *name, uint32_t data) {
    BlDtAddPropertyU32s(parent, name, &data, 1);
}

static inline void BlDtAddPropertyString(struct BlDtNode *parent, const char *name, const char *data) {
    BlDtAddPropertyStrings(parent, name, &data, 1);
}

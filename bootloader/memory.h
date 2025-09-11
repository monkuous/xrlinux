#pragma once

#include <stddef.h>
#include <stdint.h>

void BlAddHeapRange(uintptr_t base, size_t size);

void *BlAllocateHeap(size_t size, size_t alignment, bool permanent);
void *BlResizeHeap(void *ptr, size_t newSize, size_t alignment);
void BlFreeHeap(void *ptr);

#define BL_ALLOCATE(type, count) ((type *)BlAllocateHeap(sizeof(type) * (count), _Alignof(type), false))
#define BL_RESIZE(type, ptr, newCount) ((type *)BlResizeHeap((ptr), sizeof(type) * (newCount), _Alignof(type)))

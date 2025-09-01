#pragma once

#include <stddef.h>
#include <stdint.h>

void BlAddHeapRange(uintptr_t base, size_t size);

void *BlAllocateHeap(size_t size, size_t alignment);
void *BlResizeHeap(void *ptr, size_t newSize);
void BlFreeHeap(void *ptr);

#define BL_ALLOCATE(type, count) BlAllocateHeap(sizeof(type) * (count), _Alignof(type))

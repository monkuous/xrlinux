#include "memory.h"
#include "compiler.h"
#include "list.h"
#include "logging.h"

struct HeapRange {
    struct BlListNode Node;
    struct BlListNode FreeNode;
    uintptr_t End;
    bool Free;
};

#define HEAP_ALIGNMENT _Alignof(struct HeapRange)

static struct BlList BlHeapRanges;
static struct BlList BlFreeRanges;

void BlAddHeapRange(uintptr_t base, size_t size) {
    uintptr_t end = ALIGN_DOWN(base + size, HEAP_ALIGNMENT);
    base = ALIGN_UP(base, HEAP_ALIGNMENT);
    if (base >= end) return;

    struct HeapRange *prev = nullptr;

    BL_LIST_FOREACH(BlHeapRanges, struct HeapRange, Node, range) {
        if ((uintptr_t)range >= base) break;
        prev = range;
    }

    struct HeapRange *next = prev ? BL_LIST_NEXT(struct HeapRange, Node, *prev) : nullptr;

    if (prev != nullptr && prev->End > base) base = prev->End;
    if (next != nullptr && end > (uintptr_t)next) end = (uintptr_t)next;
    if (base >= end) return;

    bool mergePrev = prev != nullptr && prev->Free && prev->End == base;
    bool mergeNext = next != nullptr && next->Free && end == (uintptr_t)next;

    if (mergePrev && mergeNext) {
        prev->End = next->End;
        BlListRemove(&BlHeapRanges, &next->Node);
        BlListRemove(&BlFreeRanges, &next->FreeNode);
    } else if (mergePrev) {
        prev->End = end;
    } else {
        auto range = (struct HeapRange *)base;

        if (mergeNext) {
            BlListRemove(&BlHeapRanges, &next->Node);
            BlListRemove(&BlFreeRanges, &next->FreeNode);
            BlCopyMemoryOverlapping(range, next, sizeof(*range));
        } else {
            size_t available = end - base;
            if (available < sizeof(struct HeapRange)) return;

            range->End = end;
            range->Free = true;
        }

        BlListInsertAfter(&BlHeapRanges, prev ? &prev->Node : nullptr, &range->Node);
        BlListInsertAfter(&BlFreeRanges, nullptr, &range->FreeNode);
    }
}

void *BlAllocateHeap(size_t size, size_t alignment) {
    //BlPrint("BlAllocateHeap(%zu,%zu)\n", size, alignment);
    //BlDumpHeap();

    if (size == 0) return nullptr;
    if (alignment < HEAP_ALIGNMENT) alignment = HEAP_ALIGNMENT;

    size = ALIGN_UP(size, HEAP_ALIGNMENT);

    BL_LIST_FOREACH(BlFreeRanges, struct HeapRange, FreeNode, range) {
        uintptr_t rangeBase = (uintptr_t)range;
        uintptr_t rangeEnd = range->End;
        uintptr_t valueBase = ALIGN_UP(rangeBase + sizeof(*range), alignment);
        uintptr_t allocBase = valueBase - sizeof(*range);
        uintptr_t allocEnd = valueBase + size;

        if (allocEnd < allocBase || allocBase < rangeBase || allocEnd >= rangeEnd) continue;

        if (rangeEnd - allocEnd >= sizeof(*range)) {
            auto newRange = (struct HeapRange *)allocEnd;
            newRange->End = rangeEnd;
            newRange->Free = true;
            BlListInsertAfter(&BlHeapRanges, &range->Node, &newRange->Node);
            BlListInsertAfter(&BlFreeRanges, &range->FreeNode, &newRange->FreeNode);
        } else {
            allocEnd = rangeEnd;
        }

        if (allocBase - rangeBase >= sizeof(*range)) {
            range->End = allocBase;
        } else {
            auto newAnchor = BL_LIST_PREV(struct HeapRange, Node, *range);
            BlListRemove(&BlHeapRanges, &range->Node);
            BlListRemove(&BlFreeRanges, &range->FreeNode);
            range = newAnchor;
        }

        auto newRange = (struct HeapRange *)allocBase;
        newRange->End = allocEnd;
        newRange->Free = false;
        BlListInsertAfter(&BlHeapRanges, &range->Node, &newRange->Node);

        return (void *)valueBase;
    }

    BlCrash("out of memory");
}

void *BlResizeHeap(void *ptr, size_t newSize, size_t alignment) {
    if (!ptr) return BlAllocateHeap(newSize, alignment);

    if (newSize == 0) {
        BlFreeHeap(ptr);
        return nullptr;
    }

    newSize = ALIGN_UP(newSize, HEAP_ALIGNMENT);

    struct HeapRange *range = ptr - sizeof(*range);
    size_t oldSize = range->End - (size_t)ptr;

    if (oldSize >= newSize) {
        size_t extra = oldSize - newSize;

        if (extra >= sizeof(*range)) {
            struct HeapRange *newRange = ptr + oldSize;
            newRange->End = range->End;
            newRange->Free = true;
            BlListInsertAfter(&BlHeapRanges, &range->Node, &newRange->Node);
            BlListInsertAfter(&BlFreeRanges, nullptr, &newRange->FreeNode);
            range->End -= extra;
        }

        return ptr;
    }

    auto next = BL_LIST_NEXT(struct HeapRange, Node, *range);

    if (next != nullptr && (uintptr_t)next == range->End && next->Free) {
        BlListRemove(&BlHeapRanges, &next->Node);
        BlListRemove(&BlFreeRanges, &next->FreeNode);

        uintptr_t newEnd = (uintptr_t)ptr + newSize;

        if (next->End - newEnd >= sizeof(*range)) {
            auto newRange = (struct HeapRange *)newEnd;
            BlCopyMemoryOverlapping(newRange, next, sizeof(*newRange));
            BlListInsertAfter(&BlHeapRanges, &range->Node, &newRange->Node);
            BlListInsertAfter(&BlFreeRanges, nullptr, &newRange->FreeNode);
        } else {
            newEnd = next->End;
        }

        range->End = newEnd;
        return ptr;
    }

    void *newPtr = BlAllocateHeap(newSize, alignment);
    BlCopyMemory(newPtr, ptr, oldSize);
    BlFreeHeap(ptr);
    return newPtr;
}

void BlFreeHeap(void *ptr) {
    if (!ptr) return;

    struct HeapRange *range = ptr - sizeof(*range);

    auto prev = BL_LIST_PREV(struct HeapRange, Node, *range);
    auto next = BL_LIST_NEXT(struct HeapRange, Node, *range);

    bool mergePrev = prev != nullptr && prev->Free && prev->End == (uintptr_t)range;
    bool mergeNext = next != nullptr && next->Free && range->End == (uintptr_t)next;

    if (mergePrev) {
        BlListRemove(&BlHeapRanges, &range->Node);

        if (mergeNext) {
            BlListRemove(&BlHeapRanges, &next->Node);
            BlListRemove(&BlFreeRanges, &next->FreeNode);
            prev->End = next->End;
        } else {
            prev->End = range->End;
        }
    } else {
        if (mergeNext) {
            BlListRemove(&BlHeapRanges, &next->Node);
            BlListRemove(&BlFreeRanges, &next->FreeNode);
            range->End = next->End;
        }

        range->Free = true;
        BlListInsertAfter(&BlFreeRanges, nullptr, &range->FreeNode);
    }
}

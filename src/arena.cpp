#include <memory>
#include "arena.h"

Arena create_arena(size_t size) {
    Arena a;
    a.base = malloc(size);
    a.size = size;
    a.offset = 0;
    return a;
}

void* arena_alloc(Arena* a, size_t size) {
    void* ptr = (char*)a->base + a->offset;
    a->offset += size;
    return ptr;
}

void arena_free(Arena* a) {
    free(a->base);
    a->offset = 0;
}
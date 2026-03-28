#pragma once

/// <summary>
/// Memory arena for bulk allocation and deallocation
/// </summary>
/// <param name="base">Pointer to the start of the memory block</param>
/// <param name="size">Total size of the memory block in bytes</param>
/// <param name="offset">How many bytes have been allocated so far</param>
typedef struct {
    void* base;
    size_t size;
    size_t offset;
} Arena;

/// <summary>
/// Creates a memory arena of a given size
/// </summary>
/// <param name="size">Total size of the arena in bytes</param>
/// <returns>Resultant Arena object</returns>
Arena create_arena(size_t size);

/// <summary>
/// Allocates a chunk of memory from the arena
/// </summary>
/// <param name="a">Pointer to the arena to allocate from</param>
/// <param name="size">Size of the allocation in bytes</param>
/// <returns>Pointer to the allocated memory</returns>
void* arena_alloc(Arena* a, size_t size);

/// <summary>
/// Frees all memory allocated by the arena
/// </summary>
/// <param name="a">Pointer to the arena to free</param>
void arena_free(Arena* a);
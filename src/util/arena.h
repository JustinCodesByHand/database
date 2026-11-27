#ifndef MINIDB_UTIL_ARENA_H
#define MINIDB_UTIL_ARENA_H

#include <stddef.h>
#include <stdint.h>

/* One block; every allocation dies together in arena_free. */
typedef struct {
  uint8_t *buf;
  size_t capacity;
  size_t used;
} Arena;

/* Returns NULL if capacity is 0 or the block cannot be allocated. */
Arena *arena_create(size_t capacity);

/* Bump allocator: rounds `used` up to max_align_t alignment before handing out
 * memory. Returns NULL when the request does not fit; `used` is unchanged. */
void *arena_alloc(Arena *a, size_t size);

/* Frees everything at once. arena_free(NULL) is safe. */
void arena_free(Arena *a);

#endif

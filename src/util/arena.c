#include "util/arena.h"

#include <stdlib.h>

Arena *arena_create(size_t capacity) {
  if (capacity == 0) {
    return NULL;
  }

  Arena *a = malloc(sizeof(*a));
  if (a == NULL) {
    return NULL;
  }
  a->buf = malloc(capacity);
  if (a->buf == NULL) {
    free(a);
    return NULL;
  }
  a->capacity = capacity;
  a->used = 0;
  return a;
}

void *arena_alloc(Arena *a, size_t size) {
  if (a == NULL) {
    return NULL;
  }

  /* Invariant: used <= capacity, so remaining never underflows. */
  size_t remaining = a->capacity - a->used;
  const size_t alignment = _Alignof(max_align_t);
  const size_t rem = a->used % alignment;
  size_t aligned = a->used;

  if (rem != 0) {
    const size_t pad = alignment - rem;
    if (pad > remaining) {
      return NULL;
    }
    aligned += pad;
    remaining -= pad;
  }
  if (size > remaining) {
    return NULL;
  }

  void *p = a->buf + aligned;
  a->used = aligned + size;
  return p;
}

void arena_free(Arena *a) {
  if (a == NULL) {
    return;
  }
  free(a->buf);
  free(a);
}

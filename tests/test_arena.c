#include "test.h"

#include <stddef.h>
#include <stdint.h>

#include "util/arena.h"

static void arena_alloc_returns_aligned_blocks(void) {
  Arena *a = arena_create(256);
  ASSERT_NOT_NULL(a);

  void *p1 = arena_alloc(a, 1);
  ASSERT_NOT_NULL(p1);
  ASSERT_EQ_INT(0, (uintptr_t)p1 % _Alignof(max_align_t));

  void *p2 = arena_alloc(a, sizeof(double));
  ASSERT_NOT_NULL(p2);
  ASSERT_EQ_INT(0, (uintptr_t)p2 % _Alignof(max_align_t));
  ASSERT(p2 != p1);

  arena_free(a);
}

static void arena_exhaustion_returns_null_without_bumping_used(void) {
  Arena *a = arena_create(16);
  ASSERT_NOT_NULL(a);

  void *p = arena_alloc(a, 16);
  ASSERT_NOT_NULL(p);
  ASSERT_NULL(arena_alloc(a, 1));
  ASSERT_EQ_INT(16, a->used);

  arena_free(a);
}

static void arena_rejects_zero_capacity_and_oversized_requests(void) {
  ASSERT_NULL(arena_create(0));

  Arena *a = arena_create(8);
  ASSERT_NOT_NULL(a);
  ASSERT_NULL(arena_alloc(a, 9));
  ASSERT_EQ_INT(0, a->used);

  arena_free(a);
  arena_free(NULL);
}

void suite_arena(void) {
  SUITE("Arena");

  RUN_TEST(arena_alloc_returns_aligned_blocks);
  RUN_TEST(arena_exhaustion_returns_null_without_bumping_used);
  RUN_TEST(arena_rejects_zero_capacity_and_oversized_requests);
}

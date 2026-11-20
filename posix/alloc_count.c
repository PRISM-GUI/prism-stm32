/* A running count of heap allocations, for the timing line (app/frame_stats.hpp).
   Every allocation in newlib goes through _malloc_r: malloc, calloc, C++ operator new (via
   malloc), and the allocating case of realloc. The firmware links with -Wl,--wrap=_malloc_r
   (meson.build), which sends those calls here first. */
#include <reent.h>
#include <stddef.h>
#include <stdint.h>

static volatile uint32_t allocations;

void *__real__malloc_r(struct _reent *r, size_t n);

void *__wrap__malloc_r(struct _reent *r, size_t n) {
    __atomic_fetch_add(&allocations, 1u, __ATOMIC_RELAXED);
    return __real__malloc_r(r, n);
}

uint32_t prism_allocation_count(void) {
    return __atomic_load_n(&allocations, __ATOMIC_RELAXED);
}

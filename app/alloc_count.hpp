#pragma once

// A running count of heap allocations by any thread (it wraps around): on the board,
// posix/alloc_count.c counts newlib's _malloc_r; on the host, host/alloc_count_host.cpp counts
// operator new.

#include <cstdint>

extern "C" std::uint32_t prism_allocation_count(void);

// The board's allocation guard (posix/alloc_count.c; meson option `alloc_guard`): armed once
// startup is done, any later allocation is a violation (a trap with -Dalloc_guard=true).
extern "C" void prism_allocation_guard_arm(void);
extern "C" void prism_allocation_guard_disarm(void);
extern "C" std::uint32_t prism_allocation_violations(void);
extern "C" void *prism_allocation_first_violation(void);

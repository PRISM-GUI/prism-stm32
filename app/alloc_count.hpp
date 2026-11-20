#pragma once

// A running count of heap allocations by any thread (it wraps around): on the board,
// posix/alloc_count.c counts newlib's _malloc_r; on the host, host/alloc_count_host.cpp counts
// operator new.

#include <cstdint>

extern "C" std::uint32_t prism_allocation_count(void);

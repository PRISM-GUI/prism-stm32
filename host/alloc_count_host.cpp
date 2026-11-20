// The host's allocation counter for scope_bench (see app/alloc_count.hpp): counts the global
// operator new. simplify: C malloc (stb_truetype's glyph bitmaps) is not counted on the host;
// the board counts every allocation (posix/alloc_count.c).

#include "app/alloc_count.hpp"

#include <atomic>
#include <cstdlib>
#include <new>

namespace {
std::atomic<std::uint32_t> g_allocations{0};
}

extern "C" std::uint32_t prism_allocation_count(void)
{
    return g_allocations.load(std::memory_order_relaxed);
}

void *operator new(std::size_t n)
{
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void *p = std::malloc(n == 0 ? 1 : n))
        return p;
    throw std::bad_alloc{};
}

void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

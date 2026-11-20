// The two hooks PRISM's platform library asks of a program linking it, for the host's
// scope_bench: the allocation count counts the global operator new, and the host reports no
// system counters. simplify: C malloc (stb_truetype's glyph bitmaps) is not counted on the host;
// the board counts every allocation (posix/alloc_count.c).

#include <prism/platform/alloc_count.hpp>
#include <prism/platform/frame_stats.hpp>

#include <atomic>
#include <cstdlib>
#include <new>

namespace {
std::atomic<std::uint32_t> g_allocations{0};
}

std::uint32_t prism::platform::allocation_count()
{
    return g_allocations.load(std::memory_order_relaxed);
}

std::optional<prism::platform::SystemCounters> prism::platform::system_counters()
{
    return std::nullopt;
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

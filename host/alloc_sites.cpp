// Where PRISM allocates while building one snapshot of the audio scope: records the call stack
// of every operator new during one warmed-up build_snapshot(), then groups the allocations by
// the innermost prism:: function on each stack. The before/after tool for making the snapshot
// path allocation-free.
//
//   host/build/alloc_sites

#include "app/audio_scope.hpp"
#include "app/synthetic_source.hpp"
#include "raster/fonts.hpp"

#include <execinfo.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

namespace {

constexpr int kDepth = 24;
constexpr int kMaxRecords = 4096;

struct Record {
    std::array<void *, kDepth> frames;
    int depth;
};

// Fixed storage: recording must not allocate.
std::array<Record, kMaxRecords> g_records;
int g_count = 0;
bool g_recording = false;

void record()
{
    if (!g_recording || g_count == kMaxRecords)
        return;
    g_recording = false; // backtrace() must not recurse into here
    Record &r = g_records[std::size_t(g_count++)];
    r.depth = backtrace(r.frames.data(), kDepth);
    g_recording = true;
}

// addr2line names for the given addresses (this executable is linked -no-pie).
std::map<void *, std::string> symbolize(const std::vector<void *> &addrs)
{
    char exe[4096] = {};
    if (readlink("/proc/self/exe", exe, sizeof exe - 1) < 0)
        return {};
    std::string cmd = std::string("addr2line -f -C -e ") + exe;
    for (void *a : addrs) {
        char buf[32];
        std::snprintf(buf, sizeof buf, " %p", a);
        cmd += buf;
    }
    std::map<void *, std::string> names;
    FILE *p = popen(cmd.c_str(), "r");
    char line[4096];
    for (void *a : addrs) {
        if (!std::fgets(line, sizeof line, p))
            break;
        names[a] = std::string(line, std::strcspn(line, "\n"));
        std::fgets(line, sizeof line, p); // file:line
    }
    pclose(p);
    return names;
}

// The innermost frame that is PRISM's own code (not std:: or libstdc++ code instantiated on
// PRISM types, not the allocator).
std::string site_of(const Record &r, std::map<void *, std::string> &names)
{
    for (int i = 1; i < r.depth; ++i) {
        const std::string &n = names[r.frames[std::size_t(i)]];
        const std::string head = n.substr(0, n.find('(')); // return type + qualified name
        const bool library = head.find("std::") != std::string::npos ||
                             head.find("__gnu_cxx::") != std::string::npos;
        if (!library && (n.starts_with("prism::") || n.starts_with("app::")))
            return n.substr(0, 160);
    }
    return "(no prism frame)";
}

} // namespace

void *operator new(std::size_t n)
{
    record();
    if (void *p = std::malloc(n == 0 ? 1 : n))
        return p;
    throw std::bad_alloc{};
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

int main()
{
    app::SyntheticSource source{16000.f};
    app::AudioScope scope{source};
    raster::Fonts fonts;
    scope.tree().set_text_measurer(&fonts);
    {
        void *warm[4];
        backtrace(warm, 4); // the first call loads the unwinder, which allocates
    }
    for (std::uint64_t v = 1; v <= 10; ++v) { // warm up: caches, first layouts
        scope.tick();
        auto snap = scope.tree().build_snapshot(480, 272, v);
        scope.tree().clear_dirty();
    }

    scope.tick();
    g_recording = true;
    auto snap = scope.tree().build_snapshot(480, 272, 11);
    g_recording = false;
    scope.tree().clear_dirty();

    std::vector<void *> addrs;
    for (int i = 0; i < g_count; ++i)
        for (int f = 0; f < g_records[std::size_t(i)].depth; ++f)
            addrs.push_back(g_records[std::size_t(i)].frames[std::size_t(f)]);
    std::ranges::sort(addrs);
    addrs.erase(std::unique(addrs.begin(), addrs.end()), addrs.end());
    auto names = symbolize(addrs);

    if (std::getenv("ALLOC_SITES_RAW") && g_count > 0)
        for (int f = 0; f < g_records[0].depth; ++f)
            std::printf("  frame %d: %s\n", f, names[g_records[0].frames[std::size_t(f)]].c_str());
    std::map<std::string, int> sites;
    for (int i = 0; i < g_count; ++i)
        ++sites[site_of(g_records[std::size_t(i)], names)];
    std::vector<std::pair<int, std::string>> sorted;
    for (auto &[name, count] : sites)
        sorted.emplace_back(count, name);
    std::ranges::sort(sorted, std::greater{});
    std::printf("%d allocations in one snapshot build, by innermost PRISM function:\n", g_count);
    for (auto &[count, name] : sorted)
        std::printf("%4d  %s\n", count, name.c_str());
    return 0;
}

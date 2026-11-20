#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "app/audio_scope.hpp"
#include "raster/fonts.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <numbers>
#include <vector>

namespace {
constexpr float kRate = 16000.f;
constexpr float kBinHz = kRate / 1024.f;

// Hands out exactly the samples the test queued.
struct QueuedSource final : app::SampleSource {
    std::deque<float> queued;
    std::size_t read(std::span<float> out) override
    {
        const auto n = std::min(out.size(), queued.size());
        std::copy_n(queued.begin(), n, out.begin());
        queued.erase(queued.begin(), queued.begin() + std::ptrdiff_t(n));
        return n;
    }
    float sample_rate() const override { return kRate; }
    const char *name() const override { return "queued"; }

    void push_sine(float freq, std::size_t n, float amplitude = 0.5f)
    {
        for (std::size_t i = 0; i < n; ++i)
            queued.push_back(amplitude *
                             std::sin(2.f * std::numbers::pi_v<float> * freq * float(i) / kRate));
    }
};

float peak_hz(std::span<const float> spectrum)
{
    return float(std::distance(spectrum.begin(), std::ranges::max_element(spectrum))) * kBinHz;
}

void set_gain(app::AudioScope &scope, double g)
{
    auto slider = scope.model().gain.get();
    slider.value = g;
    scope.model().gain.set(slider);
}

void set_frozen(app::AudioScope &scope, bool frozen)
{
    auto box = scope.model().frozen.get();
    box.checked = frozen;
    scope.model().frozen.set(box);
}
} // namespace

TEST_CASE("audio scope: a 1 kHz block gives a 320-point waveform and a spectrum peak at 1 kHz")
{
    QueuedSource src;
    app::AudioScope scope{src};
    src.push_sine(1000.f, 1024);
    scope.tick();
    CHECK(scope.waveform().size() == 320);
    CHECK(scope.spectrum().size() == 512);
    CHECK(scope.model().waveform.series_len(0) == 320);
    CHECK(scope.model().spectrum.series_len(0) == 512);
    CHECK(std::abs(peak_hz(scope.spectrum()) - 1000.f) <= kBinHz);
}

TEST_CASE("audio scope: fewer samples than one FFT at startup still gives finite plots")
{
    QueuedSource src;
    app::AudioScope scope{src};
    src.push_sine(1000.f, 100);
    scope.tick();
    CHECK(std::ranges::all_of(scope.waveform(), [](float v) { return std::isfinite(v); }));
    CHECK(std::ranges::all_of(scope.spectrum(), [](float v) { return std::isfinite(v); }));
}

TEST_CASE("audio scope: maximum gain on a full-scale sine clamps the waveform, stays finite")
{
    QueuedSource src;
    app::AudioScope scope{src};
    set_gain(scope, 8.0);
    src.push_sine(1000.f, 1024, 1.f);
    scope.tick();
    CHECK(std::ranges::all_of(scope.waveform(), [](float v) { return v >= -1.f && v <= 1.f; }));
    CHECK(std::ranges::all_of(scope.spectrum(), [](float v) { return std::isfinite(v); }));
}

TEST_CASE("audio scope: samples that arrive while frozen are dropped")
{
    QueuedSource src;
    app::AudioScope scope{src};
    src.push_sine(1000.f, 1024);
    scope.tick();
    const std::vector<float> before(scope.spectrum().begin(), scope.spectrum().end());

    set_frozen(scope, true);
    src.push_sine(2000.f, 1024);
    scope.tick();
    CHECK(std::ranges::equal(scope.spectrum(), before));

    set_frozen(scope, false);
    src.push_sine(3000.f, 512);
    scope.tick();
    const auto bin = [](float hz) { return std::size_t(hz / kBinHz); };
    CHECK(scope.spectrum()[bin(3000.f)] > -20.f);  // the new samples are there
    CHECK(scope.spectrum()[bin(2000.f)] < -40.f);  // the frozen ones are not
}

TEST_CASE("audio scope: the view builds a snapshot and shows the stats line")
{
    QueuedSource src;
    app::AudioScope scope{src};
    raster::Fonts fonts;
    scope.tree().set_text_measurer(&fonts);
    src.push_sine(1000.f, 1024);
    scope.tick();
    scope.show_stats("12 fps");
    CHECK(scope.model().stats.get().value == "12 fps");
    auto snap = scope.tree().build_snapshot(480, 272, 1);
    REQUIRE(snap != nullptr);
    CHECK_FALSE(snap->draw_lists.empty());
    CHECK(scope.model().title.get().value == "PRISM audio scope · queued");
}

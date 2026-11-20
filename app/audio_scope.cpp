#include "app/audio_scope.hpp"

#include <algorithm>

namespace app {
namespace {

using prism::plot::AxisRange;

// A plot series over a buffer the scope owns and rewrites in place: PRISM's pattern for live
// data (perf_lab's RingPlotSource) — the series is added once, each update bumps the revision.
struct BufferSource {
    const std::vector<float> *ys;
    double x_step;
    std::size_t size() const { return ys->size(); }
    double x(std::size_t i) const { return double(i) * x_step; }
    double y(std::size_t i) const { return (*ys)[i]; }
};

AxisRange fixed(double min, double max) { return {.min = min, .max = max, .auto_fit = false}; }

AxisRange spectrum_y_range(bool db) { return db ? fixed(Spectrum::kFloorDb, 0.0) : fixed(0.0, 1.0); }

// Shifts `incoming` into the end of `window`, dropping its oldest samples.
void shift_in(std::vector<float> &window, std::span<const float> incoming)
{
    if (incoming.size() >= window.size()) {
        std::ranges::copy(incoming.last(window.size()), window.begin());
        return;
    }
    std::shift_left(window.begin(), window.end(), std::ptrdiff_t(incoming.size()));
    std::ranges::copy(incoming, window.end() - std::ptrdiff_t(incoming.size()));
}

} // namespace

void ScopeModel::view(prism::app::WidgetTree::ViewBuilder &vb)
{
    vb.vstack([&] {
        vb.widget(title);
        vb.hstack([&] {
            vb.widget(frozen);
            vb.gap(12.f);
            vb.widget(db_scale);
            vb.gap(12.f);
            vb.widget(gain);
        });
        vb.hstack([&] {
            for (auto *plot : {&waveform, &spectrum})
                vb.canvas(*plot)
                    .min_size(prism::core::Height{130.f})
                    .depends_on(plot->x_range)
                    .depends_on(plot->y_range)
                    .depends_on(plot->view)
                    .depends_on(plot->cursor)
                    .depends_on(plot->revision);
        });
        vb.widget(stats);
    });
}

AudioScope::AudioScope(SampleSource &source) : source_(source)
{
    const double rate = source.sample_rate();
    model_.title.set({.value = std::string("PRISM audio scope · ") + source.name(), .heading = true});
    model_.waveform.add_series(BufferSource{&waveform_, 1000.0 / rate}, {});
    model_.waveform.x_range.set(fixed(0.0, double(kWaveformSamples) * 1000.0 / rate));
    model_.waveform.y_range.set(fixed(-1.0, 1.0));
    model_.waveform.x_label.set("ms");
    model_.spectrum.add_series(BufferSource{&spectrum_, rate / double(kFftSize)}, {});
    model_.spectrum.x_range.set(fixed(0.0, rate / 2.0));
    model_.spectrum.x_label.set("Hz");
}

bool AudioScope::drain_source()
{
    const bool frozen = model_.frozen.get().checked;
    bool any = false;
    while (const auto n = source_.read(scratch_)) {
        if (!frozen)
            shift_in(window_, std::span<const float>(scratch_).first(n));
        any = true;
    }
    return any && !frozen;
}

void AudioScope::tick()
{
    const bool fresh = drain_source();
    const double gain = model_.gain.get().value;
    const bool db = model_.db_scale.get().checked;
    if (fresh || gain != shown_gain_ || db != shown_db_)
        update_plots();
}

void AudioScope::update_plots()
{
    const auto gain = float(model_.gain.get().value);
    const bool db = model_.db_scale.get().checked;
    std::ranges::transform(window_, gained_.begin(), [gain](float v) { return v * gain; });
    std::ranges::transform(std::span<const float>(gained_).last(kWaveformSamples), waveform_.begin(),
                           [](float v) { return std::clamp(v, -1.f, 1.f); });
    if (db)
        fft_.compute(gained_, spectrum_);
    else
        fft_.compute_linear(gained_, spectrum_);
    model_.spectrum.y_range.set(spectrum_y_range(db));
    model_.waveform.notify();
    model_.spectrum.notify();
    shown_gain_ = gain;
    shown_db_ = db;
}

void AudioScope::show_stats(const std::string &line)
{
    auto label = model_.stats.get();
    label.value = line;
    model_.stats.set(label);
}

std::unique_ptr<Example> make_scope_example(SampleSource &source)
{
    return std::make_unique<AudioScope>(source);
}

} // namespace app

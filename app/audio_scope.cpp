#include "app/audio_scope.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

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
            vb.widget(fit);
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
    model_.title.set({.value = std::string("PRISM scope · ") + source.name(), .heading = true});
    model_.waveform.add_series(BufferSource{&waveform_, 1000.0 / rate}, {});
    model_.waveform.x_range.set(fixed(0.0, double(kWaveformSamples) * 1000.0 / rate));
    model_.waveform.y_range.set(fixed(-1.0, 1.0));
    model_.waveform.x_label.set("ms");
    model_.spectrum.add_series(BufferSource{&spectrum_, rate / double(kFftSize)}, {});
    model_.spectrum.x_range.set(fixed(0.0, rate / 2.0));
    model_.spectrum.x_label.set("Hz");
    model_.spectrum.y_range.set(spectrum_y_range(true));
}

bool AudioScope::drain_source()
{
    const bool frozen = model_.frozen.get().checked;
    bool any = false;
    while (const auto n = source_.read(scratch_)) {
        samples_since_report_ += n;
        for (float v : std::span<const float>(scratch_).first(n))
            peak_since_report_ = std::max(peak_since_report_, std::abs(v));
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
    if (db != shown_db_)
        model_.spectrum.y_range.set(spectrum_y_range(db));
    if (fresh || gain != shown_gain_ || db != shown_db_)
        update_plots();
    if (const auto clicks = model_.fit.get().click_count; clicks != fit_clicks_) {
        fit_clicks_ = clicks;
        fit_ranges();
    }
}

// Like an oscilloscope's autoset: once, on the samples shown now. The waveform gets its peak
// with some headroom; the spectrum its peak at the top and 60 dB below it (dB), or 0 .. peak.
void AudioScope::fit_ranges()
{
    constexpr double kHeadroom = 1.2;
    float peak = 0.f;
    for (float v : waveform_)
        peak = std::max(peak, std::abs(v));
    const double half = std::max(double(peak) * kHeadroom, 1e-4);
    model_.waveform.y_range.set(fixed(-half, half));
    const double top = *std::ranges::max_element(spectrum_);
    model_.spectrum.y_range.set(shown_db_ ? fixed(top - 60.0, top + 6.0)
                                          : fixed(0.0, std::max(top * kHeadroom, 1e-6)));
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
    model_.waveform.notify();
    model_.spectrum.notify();
    shown_gain_ = gain;
    shown_db_ = db;
}

void AudioScope::show_stats(std::string_view line)
{
    // In place: the label's string keeps its memory, so a new line allocates nothing.
    model_.stats.mutate([line](prism::ui::Label<> &label) { label.value.assign(line); });
    show_level();
}

// "PRISM scope · mic · 16000 S/s · peak -42 dBFS": what arrived since the last report.
void AudioScope::show_level()
{
    const char *name = source_.name();
    int n = 0;
    if (samples_since_report_ == 0)
        n = std::snprintf(title_text_.data(), title_text_.size(), "PRISM scope · %s · no samples", name);
    else if (peak_since_report_ == 0.f)
        n = std::snprintf(title_text_.data(), title_text_.size(), "PRISM scope · %s · %zu S/s · silent",
                          name, samples_since_report_);
    else
        n = std::snprintf(title_text_.data(), title_text_.size(), "PRISM scope · %s · %zu S/s · peak %.0f dBFS",
                          name, samples_since_report_, 20.0 * std::log10(double(peak_since_report_)));
    const std::string_view text(title_text_.data(), std::size_t(std::clamp(n, 0, int(title_text_.size()) - 1)));
    model_.title.mutate([text](prism::ui::Label<> &label) { label.value.assign(text); });
    samples_since_report_ = 0;
    peak_since_report_ = 0.f;
}

std::unique_ptr<Example> make_scope_example(SampleSource &source)
{
    return std::make_unique<AudioScope>(source);
}

} // namespace app

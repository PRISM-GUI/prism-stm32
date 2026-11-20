#pragma once

// The audio scope example: a SampleSource (the board's microphones, or a generated sweep) shown
// as a waveform plot (the last 20 ms) and a spectrum plot (0 .. half the sample rate), with
// freeze, gain and dB/linear controls and the runtime's timing line. Everything goes through
// PRISM (plot::PlotModel on vb.canvas), so what is slow on the board is PRISM's cost.

#include "app/example.hpp"
#include "app/sample_source.hpp"
#include "app/spectrum.hpp"

#include <prism/app/widget_tree.hpp>
#include <prism/core/field.hpp>
#include <prism/widgets/plot.hpp>

#include <span>
#include <string>
#include <vector>

namespace app {

struct ScopeModel {
    prism::core::Field<prism::ui::Label<>> title{{.value = "PRISM audio scope", .heading = true}};
    prism::core::Field<prism::ui::Checkbox> frozen{{.checked = false, .label = "freeze"}};
    prism::core::Field<prism::ui::Checkbox> db_scale{{.checked = true, .label = "dB"}};
    prism::core::Field<prism::ui::Slider<>> gain{
        {.value = 1.0, .min = 0.5, .max = 8.0, .step = 0.5, .label = "gain"}};
    prism::plot::PlotModel waveform, spectrum;
    prism::core::Field<prism::ui::Label<>> stats{{.value = "measuring…"}};

    void view(prism::app::WidgetTree::ViewBuilder &vb);
};

class AudioScope final : public Example {
public:
    explicit AudioScope(SampleSource &source);

    prism::app::WidgetTree &tree() override { return tree_; }
    void tick() override;
    void show_stats(const std::string &line) override;

    ScopeModel &model() { return model_; }
    std::span<const float> waveform() const { return waveform_; }  // what the waveform plot shows
    std::span<const float> spectrum() const { return spectrum_; }  // what the spectrum plot shows

    static constexpr std::size_t kFftSize = 1024;
    static constexpr std::size_t kWaveformSamples = 320; // 20 ms at 16 kHz

private:
    bool drain_source();  // true when new samples entered the window
    void update_plots();

    SampleSource &source_;
    Spectrum fft_{kFftSize};
    std::vector<float> window_ = std::vector<float>(kFftSize, 0.f); // the last kFftSize samples
    std::vector<float> scratch_ = std::vector<float>(kFftSize);
    std::vector<float> gained_ = std::vector<float>(kFftSize);
    std::vector<float> waveform_ = std::vector<float>(kWaveformSamples, 0.f);
    std::vector<float> spectrum_ = std::vector<float>(kFftSize / 2, Spectrum::kFloorDb);
    double shown_gain_ = 0.0; // the settings the plots were last computed with
    bool shown_db_ = false;
    ScopeModel model_;
    prism::app::WidgetTree tree_{model_};
};

std::unique_ptr<Example> make_scope_example(SampleSource &source);

} // namespace app

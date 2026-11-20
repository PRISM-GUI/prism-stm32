#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "app/adc_sample.hpp"

TEST_CASE("adc sample: 16-bit readings map onto [-1, 1), mid-scale is 0")
{
    CHECK(app::adc_to_sample(0) == -1.f);
    CHECK(app::adc_to_sample(32768) == 0.f);
    CHECK(app::adc_to_sample(65535) == doctest::Approx(1.f - 1.f / 32768.f));
}

TEST_CASE("adc sample: a block converts in place of the ring's input")
{
    const std::uint16_t raw[] = {0, 16384, 32768, 49152};
    float out[4]{};
    app::adc_to_samples(raw, out);
    CHECK(out[0] == -1.f);
    CHECK(out[1] == -0.5f);
    CHECK(out[2] == 0.f);
    CHECK(out[3] == 0.5f);
}

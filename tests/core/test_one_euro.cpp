#include "doctest.h"
#include "one_euro.hpp"
using namespace core;

TEST_CASE("lowpass alpha=1 is passthrough") {
    LowPass lp;
    CHECK(lp.filter(5.0f, 1.0f) == doctest::Approx(5.0f));
    CHECK(lp.filter(10.0f, 1.0f) == doctest::Approx(10.0f));
}

TEST_CASE("lowpass alpha<1 lags toward the target") {
    LowPass lp;
    CHECK(lp.filter(0.0f, 0.5f) == doctest::Approx(0.0f));   // first = seed
    float y = lp.filter(10.0f, 0.5f);
    CHECK(y > 0.0f);
    CHECK(y < 10.0f);
}

TEST_CASE("oneeuro converges to a constant signal") {
    OneEuro f;
    float y = 0.0f;
    for (int i = 0; i < 300; ++i) y = f.filter(42.0f, 1.0f / 60.0f);
    CHECK(y == doctest::Approx(42.0f).epsilon(0.001));
}

TEST_CASE("oneeuro reduces high-frequency jitter energy") {
    OneEuro f;
    const float mean = 50.0f;
    float in_energy = 0.0f, out_energy = 0.0f;
    for (int i = 0; i < 400; ++i) {
        float noise = (i % 2 == 0) ? 5.0f : -5.0f;   // 30 Hz jitter at 60 fps
        float x = mean + noise;
        float y = f.filter(x, 1.0f / 60.0f);
        if (i > 100) {                                // let it settle
            in_energy  += noise * noise;
            out_energy += (y - mean) * (y - mean);
        }
    }
    CHECK(out_energy < in_energy * 0.5f);             // at least halves jitter
}

TEST_CASE("oneeuro still tracks a sustained ramp (does not stick)") {
    OneEuro f;
    float y = 0.0f;
    for (int i = 0; i < 200; ++i) y = f.filter((float)i, 1.0f / 60.0f);
    // After a long ramp the output should be close to the input, not lagging far.
    CHECK(y > 190.0f);
}

TEST_CASE("reset clears filter state") {
    OneEuro f;
    for (int i = 0; i < 100; ++i) f.filter(100.0f, 1.0f / 60.0f);
    f.reset();
    // First sample after reset seeds directly to the new value.
    CHECK(f.filter(7.0f, 1.0f / 60.0f) == doctest::Approx(7.0f));
}

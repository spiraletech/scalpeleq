#include "../Source/ScalpelMercuryField.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    scalpel::xray::LiquidSurface<> surface;
    std::array<float, 256> silence{};
    silence.fill(-144.f);
    for (int i = 0; i < 300; ++i) surface.advance(silence, 1.0 / 60.0, false);
    for (int i = 0; i <= 100; ++i) assert(surface.at(i / 100.f) == 0.f);

    auto musical = silence;
    for (int i = 42; i < 74; ++i) musical[(std::size_t)i] = -14.f;
    for (int i = 0; i < 60; ++i) surface.advance(musical, 1.0 / 60.0, false);
    assert(surface.at(.22f) > .12f);
    assert(surface.at(.80f) < .01f);

    scalpel::xray::LiquidSurface<> reduced;
    for (int i = 0; i < 60; ++i) reduced.advance(musical, 1.0 / 60.0, true);
    assert(reduced.at(.22f) < surface.at(.22f) * .50f);

    for (int i = 0; i < 360; ++i) surface.advance(silence, .9, false);
    for (int i = 0; i <= 100; ++i) {
        const float v = surface.at(i / 100.f);
        assert(std::isfinite(v) && v < .001f);
    }
    std::cout << "SCALPEL X-RAY: silence inert, localized FFT surface, reduced motion, bounded decay PASS\n";
}

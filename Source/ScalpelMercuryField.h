#pragma once

// SCALPEL X-RAY: message-thread-only liquid surface simulation.
// This is visual telemetry, never an audio effect or FFT replacement. The
// measured spectral envelope continues to define the authoritative contour.
#include <algorithm>
#include <array>
#include <cmath>

namespace scalpel::xray {
template <std::size_t Samples = 128>
class LiquidSurface {
public:
    static_assert(Samples >= 4);
    void clear() noexcept { height_.fill(0.f); velocity_.fill(0.f); prior_.fill(0.f); }

    template <std::size_t Bins>
    void advance(const std::array<float, Bins>& spectrumDb, double seconds, bool reducedMotion) noexcept {
        static_assert(Bins >= 2);
        // A bounded, damped finite-difference system, independent of host
        // sample rate and stable after a long / suspended editor frame.
        const float dt = std::clamp((float)seconds, 0.f, .033f);
        if (dt == 0.f) return;
        const float motion = reducedMotion ? .22f : 1.f;
        std::array<float, Samples> nextVelocity{};
        for (std::size_t i = 0; i < Samples; ++i) {
            const float bin = (float)i * (float)(Bins - 1) / (float)(Samples - 1);
            const auto left = (std::size_t)bin;
            const auto right = std::min(left + 1, Bins - 1);
            const float db = spectrumDb[left] + (spectrumDb[right] - spectrumDb[left]) * (bin - (float)left);
            const float level = std::clamp((std::isfinite(db) ? db + 78.f : 0.f) / 72.f, 0.f, 1.f);
            const float attack = std::max(0.f, level - prior_[i]);
            const float target = level * .22f * motion;
            const float previous = height_[i > 0 ? i - 1 : i];
            const float following = height_[i + 1 < Samples ? i + 1 : i];
            const float curvature = previous + following - 2.f * height_[i];
            const float acceleration = 52.f * (target - height_[i]) + 112.f * curvature
                                     - 12.f * velocity_[i] + 9.f * attack * motion;
            nextVelocity[i] = std::clamp(velocity_[i] + acceleration * dt, -3.f, 3.f);
            prior_[i] = level;
        }
        for (std::size_t i = 0; i < Samples; ++i) {
            velocity_[i] = nextVelocity[i];
            height_[i] = std::clamp(height_[i] + velocity_[i] * dt, 0.f, .32f);
        }
    }

    float at(float unit) const noexcept {
        const float x = std::clamp(unit, 0.f, 1.f) * (float)(Samples - 1);
        const auto index = (std::size_t)x;
        const auto next = std::min(index + 1, Samples - 1);
        return height_[index] + (height_[next] - height_[index]) * (x - (float)index);
    }

private:
    std::array<float, Samples> height_{}, velocity_{}, prior_{};
};
} // namespace scalpel::xray

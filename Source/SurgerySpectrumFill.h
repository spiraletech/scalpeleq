#pragma once
// SURGERY EQ: display-only fast-attack / adjustable-release FFT envelope.
// Measurements remain on the analyzer worker; this smoothing is performed
// at GUI refresh rate, never in processBlock() or the audio sample loop.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
namespace surgery {
template<std::size_t Bins> class LiveSpectrumEnvelope final {
public:
    using Frame=std::array<float,Bins>;
    LiveSpectrumEnvelope() noexcept { reset(); }
    void reset() noexcept {values_.fill(-144.0f);}
    void update(const Frame& latest,double dtSeconds,double releaseSeconds) noexcept {
        const double dt=std::clamp(dtSeconds,0.001,0.2);
        const double seconds=std::clamp(releaseSeconds,0.05,10.0);
        const float keep=(float)std::exp(-dt/seconds);
        for(std::size_t i=0;i<Bins;++i){
            const float requested=std::isfinite(latest[i])?
                std::clamp(latest[i],-144.0f,0.0f):-144.0f;
            auto& shown=values_[i];
            shown=requested>=shown?requested:requested+(shown-requested)*keep;
        }
    }
    const Frame& frame() const noexcept {return values_;}
private:
    Frame values_{};
};
} // namespace surgery

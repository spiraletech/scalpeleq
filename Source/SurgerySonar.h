#pragma once
// SURGERY EQ SONAR: deterministic, stationary spectral heat, no audio changes.
// Input values are already calibrated dBFS samples from the FFT worker.
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace surgery {
template<std::size_t Bins> class SonarField final {
public:
    void reset() noexcept {heat_.fill(0.0f);motion_.fill(0.0f);last_.fill(0.0f);}
    void update(const std::array<float,Bins>& db,double elapsedSeconds,double memorySeconds) noexcept {
        const double dt=std::clamp(elapsedSeconds,0.001,0.2);
        const double memory=std::clamp(memorySeconds,0.05,10.0);
        const float decay=(float)std::exp(-dt/memory);
        for(std::size_t i=0;i<Bins;++i){
            // Display intensity is *not* the physical measurement, retained separately.
            const float clamped=std::clamp(db[i],-144.0f,0.0f);
            const float energy=std::clamp((clamped+108.0f)/108.0f,0.0f,1.0f);
            const float onset=std::clamp(energy-last_[i],0.0f,1.0f);
            heat_[i]=std::max(energy,heat_[i]*decay);
            motion_[i]=std::max(onset,motion_[i]*decay*decay);
            last_[i]=energy;
        }
    }
    float heat(std::size_t bin) const noexcept {return heat_[std::min(bin,Bins-1)];}
    float transient(std::size_t bin) const noexcept {return motion_[std::min(bin,Bins-1)];}
private:
    std::array<float,Bins> heat_{},motion_{},last_{};
};
} // namespace surgery

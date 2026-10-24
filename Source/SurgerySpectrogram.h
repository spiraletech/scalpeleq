#pragma once
// SURGERY EQ: bounded SPSC FFT-frame history. One analyzer worker producer,
// one message-thread consumer. Neither lock nor allocation in the audio callback.
// A full queue drops new *visual-only* frames, never audio.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
namespace surgery {
template<std::size_t Bins, std::size_t Capacity=128>
class SpectrogramFifo final {
    static_assert(Bins>0 && Capacity>=4);
public:
    using Frame=std::array<float,Bins>;
    bool push(const Frame& frame) noexcept {
        const auto w=write_.load(std::memory_order_relaxed);
        const auto next=(w+1)%Capacity;
        if(next==read_.load(std::memory_order_acquire))return false;
        data_[w]=frame;
        write_.store(next,std::memory_order_release);
        return true;
    }
    bool pop(Frame& result) noexcept {
        const auto r=read_.load(std::memory_order_relaxed);
        if(r==write_.load(std::memory_order_acquire))return false;
        result=data_[r];
        read_.store((r+1)%Capacity,std::memory_order_release);
        return true;
    }
    void resetWhenStopped() noexcept {
        read_.store(0,std::memory_order_relaxed);
        write_.store(0,std::memory_order_relaxed);
    }
private:
    std::array<Frame,Capacity> data_{};
    std::atomic<std::size_t> write_{0},read_{0};
};
// Color scaling is independent of the EQ and of the FFT measurement.
// Each waterfall pixel corresponds to a historical frequency-bin dBFS value.
inline float heatIntensity(float db,float floorDb=-108.0f) noexcept {
    if(!std::isfinite(db))return 0.0f;
    const auto normalized=std::clamp((db-floorDb)/(-floorDb),0.0f,1.0f);
    return std::pow(normalized,1.35f);
}
inline float tightnessPercent(float q) noexcept {
    const float minQ=0.3f,maxQ=96.0f;
    return 100.0f*std::clamp(
        std::log(std::clamp(q,minQ,maxQ)/minQ)/std::log(maxQ/minQ),0.0f,1.0f);
}
} // namespace surgery

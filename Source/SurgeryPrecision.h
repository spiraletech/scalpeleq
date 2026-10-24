#pragma once
// SURGERY EQ precision: one ROOT owns the allowed centre-frequency interval.
// Spectral skirts of an IIR filter may extend outside its centre interval;
// no claim of hard band-limited filtering is made by this helper.
#include <algorithm>
#include <cmath>
namespace surgery::precision {
struct Bounds {float lo,hi;};
inline Bounds rootBounds(float left,float right) noexcept {
    return {std::clamp(std::min(left,right),20.0f,20000.0f),
            std::clamp(std::max(left,right),20.0f,20000.0f)};
}
inline float boundedFrequency(float hz,float left,float right) noexcept {
    const auto b=rootBounds(left,right);
    return std::clamp(hz,b.lo,b.hi);
}
inline float scrubFrequency(float startingHz,float dxPixels,
                           float graphFrequency,bool ctrlFine,
                           float left,float right) noexcept {
    const float hz=ctrlFine?
        startingHz*std::exp2(std::clamp(dxPixels,-5000.0f,5000.0f)/2400.0f):
        graphFrequency;
    return boundedFrequency(hz,left,right);
}
inline float scrubWheelFrequency(float hz,float turns,float left,float right) noexcept {
    return boundedFrequency(hz*std::exp2(turns/48.0f),left,right);
}
inline float adjustQ(float q,float wheelTurns,bool fine) noexcept {
    const float step=fine?0.025f:0.25f;
    return std::clamp(q+wheelTurns*step,0.3f,96.0f);
}
// Display-only logarithmic Q tightness. Lives with precision controls,
 // independent of any retired waterfall/spectrogram implementation.
inline float tightnessPercent(float q) noexcept {
    constexpr float minQ=0.3f,maxQ=96.0f;
    return 100.0f*std::clamp(
        std::log(std::clamp(q,minQ,maxQ)/minQ)/std::log(maxQ/minQ),0.0f,1.0f);
}
inline float bandwidthOctaves(float q) noexcept {
    return 2.0f*std::asinh(1.0f/(2.0f*std::clamp(q,0.3f,96.0f)))/
           std::log(2.0f);
}
}

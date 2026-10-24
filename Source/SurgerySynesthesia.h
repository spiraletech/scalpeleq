#pragma once
#include "SurgeryWeatherInterpretation.h"
namespace surgery::synesthesia {
struct RGB {float r=0,g=0,b=0;};
inline RGB palette(float balance) noexcept {
    constexpr std::array<unsigned,6> p{0x27ddff,0x306bff,0x9e8bff,0xb07df4,0xf05bbf,0xffab8c};
    const float u=weather::unit(balance)*5;const int a=std::min(4,(int)u);float t=u-a;t=t*t*(3-2*t);
    auto channel=[&](int shift){return (((p[(size_t)a]>>shift)&255)*(1-t)+((p[(size_t)a+1]>>shift)&255)*t)/255.f;};
    return {channel(16),channel(8),channel(0)};
}
inline float localEnergy(const weather::Meaning& f,float frequencyPosition) noexcept {
    const float p=weather::unit(frequencyPosition)*3;const int a=std::min(2,(int)p);const float t=p-a;
    return f.musicEnergy[(size_t)a]*(1-t)+f.musicEnergy[(size_t)a+1]*t;
}
inline float musicBias(const weather::Meaning& f) noexcept {
    return .12f*(f.centroid-.5f)+.10f*(f.musicEnergy[3]-f.musicEnergy[0])+.02f*(f.musicEnergy[1]+f.musicEnergy[2]);
}
inline RGB read(const weather::Meaning& f,float frequencyPosition,float accent=0) noexcept {
    const float balance=weather::unit(.04f+.80f*frequencyPosition+musicBias(f));
    auto c=palette(balance);const float warm=weather::unit(accent*(f.crest+f.flash*5));
    c.r+=(1-c.r)*warm;c.g+=(.86f-c.g)*warm;c.b+=(.72f-c.b)*warm;return c;
}
} // namespace surgery::synesthesia

#pragma once
// X-RAY 3.0: pitch-class colour interpretation only. This consumes existing
// log-frequency display measurements. It never changes the FFT or audio DSP;
// broadband/noisy material intentionally produces little pitch colour bias.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace scalpel::xray::chroma {
struct Result {
    std::array<float,12> weight{};
    float confidence=0.f;
    int dominant=-1;
};

template<std::size_t N>
Result estimate(const std::array<float,N>& dbfs) noexcept {
    static_assert(N>=32);
    Result result;
    constexpr double logRange=6.907755278982137; // ln(20000/20)
    double total=0;
    for(std::size_t i=2;i+2<N;++i){
        const float db=dbfs[i];
        if(!std::isfinite(db)||db<-80.f)continue;
        const double hz=20.0*std::exp(logRange*((i+0.5)/N));
        if(hz<130.0||hz>4600.0)continue; // Unresolved low pitch / noisy top.
        const float neighbor=std::max(std::max(dbfs[i-2],dbfs[i-1]),
                                       std::max(dbfs[i+1],dbfs[i+2]));
        const float prominence=std::clamp((db-neighbor-1.5f)/14.f,0.f,1.f);
        if(prominence<=0.f)continue;
        const double midi=69.0+12.0*std::log2(hz/440.0);
        const int note=static_cast<int>(std::round(midi));
        const double detune=std::abs(midi-note);
        if(detune>0.46)continue;
        const float tuning=(float)std::exp(-3.0*detune*detune);
        const float amplitude=std::pow(10.f,std::clamp(db,-80.f,0.f)/20.f);
        const float fundamentalBias=(float)std::clamp(std::sqrt(440.0/hz),.35,1.5);
        const float weight=amplitude*prominence*tuning*fundamentalBias;
        const int pitchClass=(note%12+12)%12;
        result.weight[(std::size_t)pitchClass]+=weight;
        total+=weight;
    }
    if(total<1.e-5)return result;
    int best=0;
    for(int i=1;i<12;++i)
        if(result.weight[(std::size_t)i]>result.weight[(std::size_t)best])best=i;
    const float loudest=result.weight[(std::size_t)best];
    if(loudest<1.e-6f)return result;
    result.confidence=std::clamp((loudest-(float)total/12.f)/
        (loudest+1.e-6f),0.f,1.f);
    result.dominant=best;
    for(auto& v:result.weight)v=std::clamp(v/(loudest+1.e-6f),0.f,1.f);
    return result;
}

// Deliberately aesthetic pitch-class palette; not a physical or psychological
// assertion that particular notes have objective colours.
struct RGB {float r,g,b;};
inline constexpr std::array<RGB,12> palette{{
    {.78f,.39f,.72f},{.44f,.58f,.92f},{.32f,.85f,.82f},
    {.34f,.83f,.60f},{.68f,.83f,.48f},{.97f,.68f,.39f},
    {.92f,.47f,.49f},{.77f,.45f,.90f},{.43f,.68f,.94f},
    {.39f,.89f,.74f},{.90f,.68f,.47f},{.95f,.44f,.69f}
}};
} // namespace scalpel::xray::chroma

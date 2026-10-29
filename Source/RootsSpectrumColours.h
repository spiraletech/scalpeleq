#pragma once
// Architecture A: each ROOT is one frequency interval defined by its TWO
// handles. The same interval model is used by the spectral colour renderer
// and the real-time band-audition filters. No Q-dependent colour hallucination.
#include <algorithm>
#include <array>
#include <cmath>
namespace prism::roots::visual {
constexpr int kRoots=7,kNodes=14;
constexpr double kGraphLowHz=20.0,kGraphHighHz=20000.0;
struct Node {double hz=1000.0,q=1.5;bool enabled=true;};
struct Region {double lowHz=20.0,highHz=20000.0;bool enabled=true;};
struct ColourWeights {
    std::array<double,kRoots> weight{};
    int strongest=-1;
    int contributors=0;
};
constexpr int rootOfNode(int node) noexcept {
    return std::clamp(node/2,0,kRoots-1);
}
inline double graphUnit(double hz) noexcept {
    const double f=std::clamp(hz,kGraphLowHz,kGraphHighHz);
    return std::log(f/kGraphLowHz)/std::log(kGraphHighHz/kGraphLowHz);
}
inline double graphFrequency(double u) noexcept {
    return kGraphLowHz*std::pow(kGraphHighHz/kGraphLowHz,std::clamp(u,0.0,1.0));
}
// FL Studio-inspired stationary spectral heatmap: the x coordinate is
// ALWAYS a log-frequency address, never an elapsed-time/history index.
// Source dB is the measured analyzer value before user-selectable visual tilt.
inline double heatmapFrequencyAt(int column,int columns) noexcept {
    if(columns<=0)return kGraphLowHz;
    const double u=(std::clamp(column,0,columns-1)+0.5)/columns;
    return graphFrequency(u);
}
inline float heatmapStrength(float measuredDb) noexcept {
    if(!std::isfinite(measuredDb))return 0.0f;
    const float normalized=std::clamp((measuredDb+60.0f)/60.0f,0.0f,1.0f);
    // Lift quiet, real harmonics gently; absolute -60 dBFS is dark.
    return std::pow(normalized,0.72f);
}
inline float heatmapVerticalWeight(float heightUnit) noexcept {
    const float v=std::clamp(heightUnit,0.0f,1.0f);
    // Soft vertical glow with no movement or fabricated frequency content.
    return std::pow(std::max(0.0f,4.0f*v*(1.0f-v)),0.62f);
}
// Alpha is defined in graph coordinates: 0 is 0 dBFS, 1 is -60 dBFS.
// Raw measured dBFS controls brightness; the displayed contour controls the
// clip boundary (which may include a user-selected presentation slope).
inline float underCurveAlpha(float measuredDb,float contourDb,float verticalUnit) noexcept {
    const float strength=heatmapStrength(measuredDb);
    if(strength<=0.f||!std::isfinite(contourDb)||!std::isfinite(verticalUnit))return 0.f;
    const float top=std::clamp(-contourDb/60.f,0.f,1.f);
    if(top>=1.f||verticalUnit<top||verticalUnit>=1.f)return 0.f;
    const float depth=(verticalUnit-top)/(1.f-top);
    // A soft volume below the actual contour, with no floor stripe.
    return std::clamp(strength*std::exp(-2.6f*depth)*
        std::min(1.f,(verticalUnit-top)*75.f),0.f,.86f);
}
inline Region region(int root,const std::array<Node,kNodes>& nodes) noexcept {
    const int first=std::clamp(root,0,kRoots-1)*2;
    const auto& a=nodes[(size_t)first];
    const auto& b=nodes[(size_t)(first+1)];
    const double one=std::clamp(a.hz,kGraphLowHz,kGraphHighHz);
    const double two=std::clamp(b.hz,kGraphLowHz,kGraphHighHz);
    return {std::min(one,two),std::max(one,two),a.enabled||b.enabled};
}
inline ColourWeights at(double hz,const std::array<Node,kNodes>& nodes) noexcept {
    ColourWeights out{};
    if(hz<kGraphLowHz||hz>kGraphHighHz)return out;
    for(int root=0;root<kRoots;++root) {
        const auto area=region(root,nodes);
        if(!area.enabled)continue;
        if(hz>=area.lowHz && hz<=area.highHz) {
            out.weight[(size_t)root]=1.0;
            if(out.strongest<0)out.strongest=root;
            ++out.contributors;
        }
    }
    if(out.contributors>0)
        for(auto& v:out.weight)v/=out.contributors;
    return out;
}
} // namespace prism::roots::visual

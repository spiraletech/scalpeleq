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

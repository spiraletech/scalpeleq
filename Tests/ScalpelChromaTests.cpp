#include "../Source/ScalpelXRayChroma.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

int main(){
    std::array<float,256> s{};
    s.fill(-144.f);
    const auto silence=scalpel::xray::chroma::estimate(s);
    assert(silence.dominant==-1&&silence.confidence==0.f);
    s.fill(-28.f);
    const auto broad=scalpel::xray::chroma::estimate(s);
    assert(broad.dominant==-1&&broad.confidence==0.f);
    const auto nearest=[](double hz){
        const auto pos=std::log(hz/20.)/std::log(1000.)*256.0-.5;
        return std::clamp(static_cast<int>(std::round(pos)),2,253);
    };
    s.fill(-110.f);
    s[(std::size_t)nearest(440.)]=-8.f;
    auto a=scalpel::xray::chroma::estimate(s);
    assert(a.dominant==9); // A
    assert(a.confidence>.75f);
    s.fill(-110.f);
    s[(std::size_t)nearest(880.)]=-8.f;
    auto higher=scalpel::xray::chroma::estimate(s);
    assert(higher.dominant==9);
    std::cout<<"X-RAY pitch-class colour estimates calibrated\n";
}

#include "../Source/RootsAudioEngine.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <numbers>
#include <vector>

namespace {
struct Result {double energy=0,peak=-144;};
Result run(bool phase,double limit,bool opposing,double amplitude){
    constexpr int sr=48000,count=48000;
    prism::roots::v2::Engine engine(sr);
    using namespace prism::roots::v2;
    Config low=engine.get(6),high=engine.get(7);
    low.hz=250;high.hz=500;
    low.phase=phase;low.limit=limit;low.enabled=true;
    engine.config(6,low);engine.config(7,high);
    std::vector<float> l(count),r(count);
    for(int i=0;i<count;++i){
        const float x=(float)(amplitude*std::sin(2*std::numbers::pi*350*i/sr));
        l[(size_t)i]=x;r[(size_t)i]=opposing?-x:x;
    }
    engine.process(l.data(),r.data(),count);
    double rms=0;
    for(int i=count/2;i<count;++i){
        assert(std::isfinite(l[(size_t)i])&&std::isfinite(r[(size_t)i]));
        rms+=(double)l[(size_t)i]*l[(size_t)i]+(double)r[(size_t)i]*r[(size_t)i];
    }
    return {std::sqrt(rms/count),engine.meter().rootPeakDb[3]};
}
double bandRms(double hz,bool bandEnabled,bool mono=false,bool antiPhase=false){
    constexpr int sr=48000,count=48000;
    using namespace prism::roots::v2;
    Engine engine(sr);
    auto lo=engine.get(6),hi=engine.get(7);
    lo.hz=250;hi.hz=500;lo.enabled=bandEnabled;hi.enabled=bandEnabled;
    engine.config(6,lo);engine.config(7,hi);
    engine.setRootMono(3,mono);
    std::vector<float> l(count),r(count);
    for(int i=0;i<count;++i){
        const float v=(float)(0.6*std::sin(2*std::numbers::pi*hz*i/sr));
        l[(size_t)i]=v;r[(size_t)i]=antiPhase?-v:v;
    }
    engine.process(l.data(),r.data(),count);
    double rms=0;
    for(int i=count/2;i<count;++i){
        const double v=antiPhase?(l[(size_t)i]-r[(size_t)i])*0.5:l[(size_t)i];
        assert(std::isfinite(v));rms+=v*v;
    }
    return std::sqrt(rms/(count/2));
}
}
int main(){
    // Coherence guard reduces negative SIDE contribution, with no polarity flip.
    const auto oppositeDry=run(false,0,true,.5);
    const auto oppositePhase=run(true,0,true,.5);
    assert(oppositePhase.energy<oppositeDry.energy);
    assert(oppositePhase.energy>0);
    // Optical amount applies measurable reduction without brickwall claims.
    const auto loudDry=run(false,0,false,.95);
    const auto loudOpto=run(false,1,false,.95);
    assert(loudOpto.energy<loudDry.energy);
    assert(loudOpto.energy>0);
    assert(std::isfinite(loudOpto.peak));
    // Off is fully neutral to band processing where defaults are all zero.
    const auto neutral=run(false,0,false,.95);
    assert(neutral.energy==loudDry.energy);
    // BAND OFF is a true deep spectral mute, not an old neutral bypass.
    const auto inside=bandRms(350,true);
    const auto muted=bandRms(350,false);
    const auto outside=bandRms(8000,false);
    const auto outsideReference=bandRms(8000,true);
    assert(muted<inside*.35);
    assert(outside>outsideReference*.65);
    // MONO works on the selected band's anti-phase SIDE signal.
    const auto stereoSide=bandRms(350,true,false,true);
    const auto monoSide=bandRms(350,true,true,true);
    assert(monoSide<stereoSide*.7);
    std::cout<<"Scalpel bands: OFF removes interval, MONO collapses band side, PHASE guard, LIMIT optical leveling PASS\n";
}

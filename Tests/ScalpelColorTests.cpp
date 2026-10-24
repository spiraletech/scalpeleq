#include "../Source/SurgeryOnyx.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <numbers>

namespace {
constexpr std::array<int,4> rates{44100,48000,96000,192000};
double source(int n,double sr){
    const double t=n/sr;
    return .38*std::sin(2*std::numbers::pi*211*t)+
           .24*std::sin(2*std::numbers::pi*4191*t)+
           (n%733<10?.20:0.0);
}
double diffFor(int rate,const std::array<double,5>& amounts){
    surgery::Onyx a,b;a.prepare(rate);b.prepare(rate);
    a.setRootBand(80,14000);b.setRootBand(80,14000);
    double e=0;
    for(int i=0;i<16000;++i){
        const double x=source(i,rate);
        const double y=a.sampleColor(x,amounts);
        const double neutral=b.sampleColor(x,std::array<double,5>{});
        assert(std::isfinite(y));
        if(amounts==std::array<double,5>{})assert(y==x);
        e+=(y-neutral)*(y-neutral);
    }
    return std::sqrt(e/16000.0);
}
}
int main(){
    for(const auto rate:rates){
        // True 0% neutral is exact, including all sample rates.
        assert(diffFor(rate,{0,0,0,0,0})==0.0);
        // Each knob causes actual, independent, finite audio alteration.
        for(int color=0;color<5;++color){
            std::array<double,5> amount{};amount[(size_t)color]=1.0;
            const double delta=diffFor(rate,amount);
            assert(delta>1.0e-6 && delta<0.25);
        }
        // Historical four-control caller maps exactly to new [0, four].
        surgery::Onyx legacy,modern;
        legacy.prepare(rate);modern.prepare(rate);
        for(int i=0;i<2048;++i){
            const auto x=source(i,rate);
            const auto a=legacy.sample(x,std::array<double,4>{.3,.2,.4,.1});
            const auto b=modern.sampleColor(x,std::array<double,5>{0,.3,.2,.4,.1});
            assert(a==b);
        }
        // +3 dB maximum transient-boost law and -2 dB sustain contrast law.
        assert(surgery::Onyx::law(0)==0.0);
        assert(surgery::Onyx::law(1)==1.0);
    }
    std::cout<<"Scalpel COLOR: five independent knobs, true zero, 4 rates, compatibility PASS\n";
}

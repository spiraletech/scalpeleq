#include "../Source/SurgeryCarveEngine.h"
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
using surgery::CarveEngine;
static double toneRms(double f,CarveEngine& e,bool on) {
    std::array<float,512> l{},r{};
    double sum=0;int64_t pos=0;
    for(int b=0;b<90;++b){
        for(int i=0;i<512;++i){
            const double v=0.25*std::sin(2*std::numbers::pi*f*(pos+i)/48000.0);
            l[(size_t)i]=r[(size_t)i]=(float)v;
        }
        if(on)e.process(l.data(),r.data(),512);
        if(b>55)for(float v:l)sum+=(double)v*v;
        pos+=512;
    }
    return std::sqrt(sum/(34.0*512.0));
}
int main(){
    CarveEngine e;e.prepare(48000);
    if(CarveEngine::boundaryWeight(1000,500,2000)<0.999) return 1;
    if(CarveEngine::boundaryWeight(200,500,2000)>0.001) return 2;
    auto c=surgery::CarveConfig{};c.enabled=true;c.hz=1000;c.depthDb=24;c.q=5;
    e.bounds(4,500,2000);e.configure(4,0,c);
    const double unity=toneRms(1000,e,false);
    const double removed=toneRms(1000,e,true);
    if(!(removed<unity*0.24)){std::cerr<<"notch not attenuating "<<removed<<" "<<unity<<"\n";return 3;}
    const double remote=toneRms(8000,e,true);
    const double remoteDry=toneRms(8000,e,false);
    if(!(remote>remoteDry*.94)){std::cerr<<"out of band damage\n";return 4;}
    CarveEngine null;e.prepare(48000);
    std::array<float,128> l{},r{};
    for(int i=0;i<128;++i)l[(size_t)i]=r[(size_t)i]=(float)std::sin(i*.1);
    auto orig=l;
    null.process(l.data(),r.data(),l.size());
    if(l!=orig||r!=orig){std::cerr<<"neutral not bit exact\n";return 5;}
    // No NaNs or infinities on hot input in mid/side and stereo modes.
    c.channel=surgery::Channel::side;e.configure(4,0,c);
    l.fill(2.0f);r.fill(-2.0f);
    e.process(l.data(),r.data(),l.size());
    for(float x:l)if(!std::isfinite(x)){std::cerr<<"nonfinite\n";return 6;}
    // Hidden legacy threshold still deterministically controls explicitly enabled dynamics.
    auto dynamicRms=[&](double threshold,bool dynamic){
        CarveEngine instance;instance.prepare(48000);instance.bounds(4,500,2000);
        surgery::CarveConfig d;d.enabled=true;d.hz=1000;d.depthDb=18;d.q=5;
        d.dynamic=dynamic;d.thresholdDb=threshold;instance.configure(4,0,d);
        return toneRms(1000,instance,true);
    };
    const auto low=dynamicRms(-48,true),high=dynamicRms(-6,true);
    if(!(low<unity*.25&&high>unity*.95)){std::cerr<<"legacy dynamic threshold response\n";return 7;}
    if(std::abs(dynamicRms(-48,false)-dynamicRms(-6,false))>1e-6){
        std::cerr<<"hidden threshold changed static CARVE\n";return 8;
    }
    std::cout<<"Surgery CARVE four proof groups passed\n";return 0;
}

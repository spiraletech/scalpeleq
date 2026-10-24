#include "../Source/RootsAudioEngine.h"
#include <cmath>
#include <iostream>
#include <numbers>
#include <vector>
using namespace prism::roots::v2;
constexpr int N=16384;
constexpr double sr=48000.0;
void fill(std::vector<float>& l,std::vector<float>& r,
          double mAmp=0.22,double sAmp=0.04) {
    for(int i=0;i<N;++i) {
        const double t=2.0*std::numbers::pi*900.0*i/sr;
        const double m=mAmp*std::sin(t);
        const double s=sAmp*std::sin(t);
        l[(size_t)i]=(float)(m+s);
        r[(size_t)i]=(float)(m-s);
    }
}
double sideRms(const std::vector<float>& l,const std::vector<float>& r){
    double sum=0;
    for(int i=N/2;i<N;++i){
        const double side=((double)l[(size_t)i]-(double)r[(size_t)i])*0.5;
        sum+=side*side;
    }
    return std::sqrt(sum/(N/2));
}
double midRms(const std::vector<float>& l,const std::vector<float>& r){
    double sum=0;
    for(int i=N/2;i<N;++i){
        const double mid=((double)l[(size_t)i]+(double)r[(size_t)i])*0.5;
        sum+=mid*mid;
    }
    return std::sqrt(sum/(N/2));
}
void bounds(Engine& engine){
    auto a=engine.get(0);a.hz=200.0;engine.config(0,a);
    auto b=engine.get(1);b.hz=2300.0;engine.config(1,b);
}
int main(){
    // Proof 1: all root effects bypassed => transparent full-stereo samples.
    {
        Engine engine(sr);std::vector<float> l(N),r(N);
        fill(l,r);const auto L=l,R=r;
        engine.process(l.data(),r.data(),N);
        if(l!=L||r!=R){std::cerr<<"P1: neutral bypass failed\n";return 1;}
        std::cout<<"P1 PASS: transparent unprocessed ROOT\n";
    }
    // Proof 2: ROOT SPACE acts across both boundary handles as one band.
    {
        Engine plain(sr),collapsed(sr);
        bounds(plain);bounds(collapsed);
        auto c=collapsed.get(0);c.space=0.0;c.domain=Domain::mid;collapsed.config(0,c);
        std::vector<float> l(N),r(N),l2(N),r2(N);
        fill(l,r);fill(l2,r2);
        plain.process(l.data(),r.data(),N);
        collapsed.process(l2.data(),r2.data(),N);
        const auto original=sideRms(l,r),narrow=sideRms(l2,r2);
        if(!(original>1e-3&&narrow<original*0.75)){
           std::cerr<<"P2 ROOT space didn't narrow band "<<narrow<<"/"<<original<<"\n";return 2;
        }
        std::cout<<"P2 PASS: ROOT SPACE still works with MID-only compressor domain\n";
    }
    // Proof 3: root compressor responds to its threshold, not per-handle
    // knobs, and has real measured gain reduction.
    {
        Engine off(sr),on(sr);
        bounds(off);bounds(on);
        auto c=on.get(0);c.comp=Compressor::fet4;c.thresholdDb=-42;
        on.config(0,c);
        std::vector<float> l(N),r(N),l2(N),r2(N);
        fill(l,r,0.34,0.01);fill(l2,r2,0.34,0.01);
        off.process(l.data(),r.data(),N);
        on.process(l2.data(),r2.data(),N);
        if(!(midRms(l2,r2)<midRms(l,r)*0.96&&on.meter().largestGrDb>1.0)){
            std::cerr<<"P3 compressor didn't respond to ROOT threshold\n";return 3;
        }
        std::cout<<"P3 PASS: right-click ROOT compressor and threshold affect audio\n";
    }
    // Proof 4: global stereo space works independently of per-ROOT space;
    // mono equals L/R, and wide actually increases SIDE when safe.
    {
        Engine mono(sr),wide(sr);
        bounds(mono);bounds(wide);
        mono.setSpace(0.0);wide.setSpace(100.0);
        std::vector<float> a(N),b(N),c(N),d(N);
        fill(a,b);fill(c,d);
        mono.process(a.data(),b.data(),N);
        wide.process(c.data(),d.data(),N);
        // A real-time width knob uses an 8ms smooth collapse. Judge
        // exact L/R identity only AFTER the transition, not sample zero.
        for(int i=N/2;i<N;++i)if(a[(size_t)i]!=b[(size_t)i]){
            std::cerr<<"P4 mono output asymmetry after smoothing\n";return 4;
        }
        if(!(sideRms(c,d)>0.04/std::sqrt(2.0)*1.08)){
            std::cerr<<"P4 global stereo width did not increase SIDE\n";return 4;
        }
        std::cout<<"P4 PASS: global SPACE mono/wide changes real stereo audio\n";
    }
    return 0;
}

#include "../Source/RootsAudioEngine.h"
#include "../Source/SurgeryCarveEngine.h"
#include <cmath>
#include <iostream>
#include <numbers>
#include <vector>
constexpr int N=32768;constexpr double sr=48000.0;
static void tone(std::vector<float>& l,std::vector<float>& r,double f,
                 double mid=0.25,double side=0.12){
    for(int i=0;i<N;++i){
        const double s=std::sin(2.0*std::numbers::pi*f*i/sr);
        l[(size_t)i]=(float)((mid+side)*s);
        r[(size_t)i]=(float)((mid-side)*s);
    }
}
static double rms(const std::vector<float>& l,const std::vector<float>& r,bool side){
    double sum=0;
    for(int i=N/2;i<N;++i){
        const double x=((double)l[(size_t)i]+(side?-1.0:1.0)*(double)r[(size_t)i])*0.5;
        sum+=x*x;
    }
    return std::sqrt(sum/(N/2));
}
static void widen(prism::roots::v2::Engine& e) {
    auto a=e.get(0);a.hz=500;e.config(0,a);
    auto b=e.get(1);b.hz=2200;e.config(1,b);
}
int main(){
    using namespace prism::roots::v2;
    // A: ROOT-local MONO removes SIDE around 900Hz, not unrelated 8kHz.
    Engine mono(sr),normal(sr);widen(mono);widen(normal);mono.setRootMono(0,true);
    std::vector<float> a(N),b(N),c(N),d(N);
    tone(a,b,900);tone(c,d,900);
    normal.process(c.data(),d.data(),N);
    mono.process(a.data(),b.data(),N);
    if(!(rms(a,b,true)<rms(c,d,true)*0.78))return 1;
    mono.prepare(sr);widen(mono);mono.setRootMono(0,true);
    tone(a,b,8000);tone(c,d,8000);
    mono.process(a.data(),b.data(),N);
    normal.prepare(sr);widen(normal);
    normal.process(c.data(),d.data(),N);
    if(!(rms(a,b,true)>rms(c,d,true)*0.85))return 2;
    // B: ROOT gain follows the right-click MID/SIDE routing.
    Engine midOnly(sr),sideOnly(sr);widen(midOnly);widen(sideOnly);
    auto mid=midOnly.get(0);mid.domain=Domain::mid;midOnly.config(0,mid);
    auto side=sideOnly.get(0);side.domain=Domain::side;sideOnly.config(0,side);
    midOnly.setRootGain(0,12);sideOnly.setRootGain(0,12);
    tone(a,b,900);tone(c,d,900);
    midOnly.process(a.data(),b.data(),N);
    sideOnly.process(c.data(),d.data(),N);
    if(!(rms(a,b,false)>rms(c,d,false)*1.40 &&
         rms(c,d,true)>rms(a,b,true)*1.40))return 3;
    // C: CARVE respects the selected ROOT's channel, not just per-CARVE state.
    surgery::CarveEngine carve;carve.prepare(sr);
    surgery::CarveConfig cfg;cfg.enabled=true;cfg.hz=900;cfg.depthDb=26;cfg.q=4;
    carve.bounds(0,500,2200);carve.configure(0,0,cfg);
    carve.rootDomain(0,surgery::Channel::side);
    tone(a,b,900);tone(c,d,900);
    carve.process(a.data(),b.data(),N);
    if(!(rms(a,b,true)<rms(c,d,true)*0.36 &&
         rms(a,b,false)>rms(c,d,false)*0.95))return 4;
    // D: every CARVE centre is genuinely clamped in the DSP to its ROOT.
    surgery::CarveConfig outside;outside.enabled=true;
    outside.hz=12000;outside.q=16.0;outside.depthDb=12.0;
    carve.bounds(0,300,630);
    carve.configure(0,1,outside);
    if(std::abs(carve.get(0,1).hz-630.0)>0.01)return 6;
    outside.hz=40.0;carve.configure(0,2,outside);
    if(std::abs(carve.get(0,2).hz-300.0)>0.01)return 7;
    // E: Fresh each ROOT owns four distinct neutral CARVE defaults.
    for(int root=0;root<7;++root){
        float prior=0;
        for(int slot=0;slot<4;++slot){
            float hz=surgery::defaultCarveHz(root,slot);
            if(!(hz>prior))return 5;
            prior=hz;
        }
    }
    std::cout<<"ROOT-local mono, MID/SIDE routing and 28 CARVE defaults passed\n";
    return 0;
}

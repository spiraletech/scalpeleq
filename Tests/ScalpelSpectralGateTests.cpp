// Release gates intentionally return failure until production satisfies them.
// No assert(): checks remain active in optimized/Release builds.
#include "../Source/RootsAudioEngine.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <iomanip>
using namespace prism::roots::v2;
struct Measurement { double mid,side,peak; };
Measurement tone(int sr,double low,double high,double hz,Domain domain,bool enabled) {
    Engine engine(sr);
    auto a=engine.get(0),b=engine.get(1);
    a.hz=low;b.hz=high;a.domain=b.domain=domain;a.enabled=b.enabled=enabled;
    engine.config(0,a);engine.config(1,b);
    constexpr int block=256;
    std::array<float,block> l{},r{};
    double mm=0,ss=0,peak=0;int samples=0;
    // Two seconds settling, one second measurement; low-frequency probes
    // include at least 20 complete periods in the measurement window.
    for(int start=0;start<sr*3;start+=block){
        const int n=std::min(block,sr*3-start);
        for(int i=0;i<n;++i){
            const double t=2*std::numbers::pi*hz*(start+i)/sr;
            const double m=.2*std::sin(t),s=.13*std::cos(t);
            l[i]=(float)(m+s);r[i]=(float)(m-s);
        }
        engine.process(l.data(),r.data(),n);
        for(int i=0;i<n;++i){
            if(!std::isfinite(l[i])||!std::isfinite(r[i]))throw std::runtime_error("non-finite output");
            if(start+i<sr*2)continue;
            const double m=((double)l[i]+r[i])*.5,s=((double)l[i]-r[i])*.5;
            mm+=m*m;ss+=s*s;++samples;
            peak=std::max(peak,std::max(std::abs((double)l[i]),std::abs((double)r[i])));
        }
    }
    return {std::sqrt(mm/samples),std::sqrt(ss/samples),peak};
}
double db(double a,double b){return 20*std::log10(std::max(a,1.e-15)/std::max(b,1.e-15));}
int main(int argc,char** argv){
    std::ofstream csv(argc>1?argv[1]:"spectral-gate.csv");
    csv<<"sample_rate,low_hz,high_hz,probe_hz,log_position,domain,mid_change_db,side_change_db,gate,pass\n";
    int interiorFailures=0,domainFailures=0,total=0;
    for(int sr:{44100,48000,96000,192000})
    for(auto range:{std::pair{250.,500.},std::pair{950.,1050.},std::pair{100.,10000.}})
    for(double position:{0.,.01,.10,.25,.50,.75,.90,.99,1.}){
        const auto [low,high]=range;
        const double hz=low*std::pow(high/low,position);
        const auto reference=tone(sr,low,high,hz,Domain::stereo,true);
        const auto cut=tone(sr,low,high,hz,Domain::stereo,false);
        const double mid=db(cut.mid,reference.mid),side=db(cut.side,reference.side);
        // Preliminary interior is the central 50% of logarithmic width.
        const bool interior=position>=.25&&position<=.75;
        const bool pass=!interior||(mid<=-60&&side<=-60);
        if(!pass)++interiorFailures;
        csv<<sr<<','<<low<<','<<high<<','<<hz<<','<<position<<",stereo,"<<mid<<','<<side<<','<<(interior?"interior_60dB":"boundary_report")<<','<<pass<<'\n';++total;
    }
    for(int sr:{44100,48000,96000,192000})for(auto domain:{Domain::mid,Domain::side}){
        auto ref=tone(sr,250,500,350,domain,true),cut=tone(sr,250,500,350,domain,false);
        const double m=db(cut.mid,ref.mid),s=db(cut.side,ref.side);
        const bool pass=std::abs(domain==Domain::mid?s:m)<.001;
        if(!pass)++domainFailures;
        csv<<sr<<",250,500,350,0.485,"<<(domain==Domain::mid?"mid":"side")<<','<<m<<','<<s<<",unselected_domain_0.001dB,"<<pass<<'\n';++total;
    }
    std::cout<<"probes="<<total<<" interior_60dB_failures="<<interiorFailures<<" domain_isolation_failures="<<domainFailures<<'\n';
    return interiorFailures||domainFailures?1:0;
}

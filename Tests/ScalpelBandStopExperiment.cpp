#include "../Source/Experimental/ScalpelBandStop.h"
#include <fstream>
#include <iostream>
#include <random>
#include <chrono>
#include <cstdlib>
#include <new>
static bool audit=false;static size_t allocations=0;
void* operator new(std::size_t n){if(audit)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(int argc,char** argv){
    using scalpel::experimental::BandStop;
    const std::string dir=argc>1?argv[1]:".";
    std::ofstream csv(dir+"/candidate-response.csv"),time(dir+"/candidate-signals.csv");
    csv<<"sample_rate,low_hz,high_hz,log_position,probe_hz,response_db\n";
    time<<"sample_rate,signal,peak_output,max_sample_jump,finite,allocations\n";
    int failures=0,interiorFailures=0,probes=0;
    for(int sr:{44100,48000,96000,192000})for(auto range:{std::pair{250.,500.},std::pair{950.,1050.},std::pair{100.,10000.}}){
        BandStop filter;const auto [low,high]=range;
        if(!filter.configure(sr,low,high)){++failures;continue;}
        for(double p:{-.5,0.,.01,.10,.25,.50,.75,.90,.99,1.,1.5}){
            double hz=low*std::pow(high/low,p);if(hz>=sr/2)continue;
            const double db=20*std::log10(std::max(1.e-15,std::abs(filter.response(sr,hz))));
            csv<<sr<<','<<low<<','<<high<<','<<p<<','<<hz<<','<<db<<'\n';++probes;
            if(p>=.25&&p<=.75&&db>-60)++interiorFailures;
        }
    }
    for(int sr:{44100,48000,96000,192000})for(int signal=0;signal<5;++signal){
        BandStop filter;filter.configure(sr,250,500);
        std::mt19937 random(8128);std::uniform_real_distribution<double> noise(-.25,.25);
        double peak=0,jump=0,previous=0;bool finite=true;allocations=0;
        audit=true;
        for(int i=0;i<sr*2;++i){
            const double t=(double)i/sr;
            double x=signal==0?(i==0?.5:0):signal==1?noise(random):
                signal==2?.15*(std::sin(2*std::numbers::pi*100*t)+std::sin(2*std::numbers::pi*350*t)+std::sin(2*std::numbers::pi*8000*t)):
                signal==3?.25*std::sin(2*std::numbers::pi*(20*t+(20000.-20.)/4*t*t)):
                .25*std::sin(2*std::numbers::pi*350*t);
            if(signal==4&&i%256==0){
                const double low=100*std::pow(10.,.5+.5*std::sin(2*std::numbers::pi*3*t));
                if(!filter.configure(sr,low,low*2))finite=false;
            }
            const double y=filter.sample(x);finite=finite&&std::isfinite(y);
            peak=std::max(peak,std::abs(y));jump=std::max(jump,std::abs(y-previous));previous=y;
        }
        audit=false;
        const char* name=signal==0?"impulse":signal==1?"white_noise":signal==2?"multitone":signal==3?"sweep":"live_drag";
        time<<sr<<','<<name<<','<<peak<<','<<jump<<','<<finite<<','<<allocations<<'\n';
        if(!finite||allocations)++failures;
        // A normalized 0.25 sine must not become louder than full scale
        // during dragging; state transients are a critical audio-safety gate.
        if(signal==4&&peak>1.)++failures;
    }
    std::cout<<"candidate_probes="<<probes<<" interior_60dB_failures="<<interiorFailures<<" signal_or_safety_failures="<<failures<<'\n';
    return failures||interiorFailures?1:0;
}

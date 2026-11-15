// Actual steady sine measurements complement the candidate's coefficient plot.
#include "../Source/Experimental/ScalpelBandStop.h"
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
    std::ofstream out(argc>1?argv[1]:"candidate-measured.csv");
    out<<"sample_rate,low_hz,high_hz,log_position,probe_hz,measured_db,theoretical_db,group_delay_ms\n";
    int failures=0;
    for(int sr:{44100,48000,96000,192000})for(auto range:{std::pair{250.,500.},std::pair{950.,1050.},std::pair{100.,10000.}})
    for(double position:{-.5,0.,.25,.5,.75,1.,1.5}){
        const auto [low,high]=range;const double hz=low*std::pow(high/low,position);
        if(hz>=sr*.49)continue;
        scalpel::experimental::BandStop filter;if(!filter.configure(sr,low,high))return 2;
        double inputPower=0,outputPower=0;
        // At narrow bandwidths high-order IIR tails require longer settling.
        for(int i=0;i<sr*5;++i){
            const double x=.25*std::sin(2*std::numbers::pi*hz*i/sr),y=filter.sample(x);
            if(!std::isfinite(y))return 3;
            if(i>=sr*4){inputPower+=x*x;outputPower+=y*y;}
        }
        const double measured=10*std::log10(std::max(1.e-30,outputPower/inputPower));
        const double theoretical=20*std::log10(std::max(1.e-15,std::abs(filter.response(sr,hz))));
        const double delta=.01;
        const double phaseDelta=std::arg(filter.response(sr,hz+delta)/filter.response(sr,hz-delta));
        const double delay=-1000*phaseDelta/(4*std::numbers::pi*delta);
        out<<sr<<','<<low<<','<<high<<','<<position<<','<<hz<<','<<measured<<','<<theoretical<<','<<delay<<'\n';
        if(position>=.25&&position<=.75&&measured>-60)++failures;
    }
    std::cout<<"measured_interior_60dB_failures="<<failures<<'\n';return failures?1:0;
}

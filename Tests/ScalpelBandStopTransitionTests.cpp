#include "../Source/Experimental/ScalpelBandStop.h"
#include <iostream>
int main(){
    int failures=0;
    for(int sr:{44100,48000,96000,192000}){
        scalpel::experimental::CrossfadedBandStop filter;filter.configure(sr,250,500);
        double peak=0,jump=0,previous=0;
        for(int i=0;i<sr*3;++i){
            const double t=(double)i/sr;
            if(i%256==0){
                const double low=100*std::pow(10.,.5+.5*std::sin(2*std::numbers::pi*3*t));
                if(!filter.configure(sr,low,low*2))++failures;
            }
            const double y=filter.sample(.25*std::sin(2*std::numbers::pi*350*t));
            if(!std::isfinite(y)){++failures;break;}
            peak=std::max(peak,std::abs(y));jump=std::max(jump,std::abs(y-previous));previous=y;
        }
        // Engineering experiment only: 0 dBFS ceiling for a -12 dBFS tone.
        const bool pass=peak<1.;if(!pass)++failures;
        std::cout<<"sr="<<sr<<" peak="<<peak<<" max_sample_jump="<<jump<<" pass="<<pass<<'\n';
    }
    return failures?1:0;
}

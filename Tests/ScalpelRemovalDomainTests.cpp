#include "../Source/RootsAudioEngine.h"
#include <iostream>
using namespace prism::roots::v2;
int main(){
    int failures=0;
    for(int sr:{44100,48000,96000,192000})for(auto domain:{Domain::mid,Domain::side}){
        Engine engine(sr);auto a=engine.get(0),b=engine.get(1);
        a.hz=250;b.hz=500;a.enabled=b.enabled=false;a.domain=b.domain=domain;
        engine.config(0,a);engine.config(1,b);
        double error=0,removed=0,reference=0;
        for(int block=0;block<sr/64*2;++block){
            std::array<float,64> l{},r{},dryM{},dryS{};
            for(int i=0;i<64;++i){
                const double t=2*std::numbers::pi*350*(block*64+i)/sr;
                l[i]=(float)(.2*std::sin(t)+.13*std::cos(t));
                r[i]=(float)(.2*std::sin(t)-.13*std::cos(t));
                dryM[i]=(l[i]+r[i])*.5f;dryS[i]=(l[i]-r[i])*.5f;
            }
            engine.process(l.data(),r.data(),64);
            for(int i=0;i<64;++i){
                if(block<sr/64)continue;
                const double m=((double)l[i]+r[i])*.5,s=((double)l[i]-r[i])*.5;
                error=std::max(error,std::abs(domain==Domain::mid?s-dryS[i]:m-dryM[i]));
                removed+=std::pow(domain==Domain::mid?m:s,2);
                reference+=std::pow(domain==Domain::mid?dryM[i]:dryS[i],2);
            }
        }
        const double reduction=10*std::log10(removed/reference);
        const bool pass=error<1.e-7&&reduction<-20;
        std::cout<<"sr="<<sr<<" domain="<<(int)domain<<" unselected_max_error="<<error<<" selected_change_db="<<reduction<<" pass="<<pass<<'\n';
        if(!pass)++failures;
    }
    return failures?1:0;
}

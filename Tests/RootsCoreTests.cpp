#include "../Source/PrismRootsCore.h"
#include <cmath>
#include <iostream>
#include <numbers>
#include <vector>

using prism::roots::Engine;
using prism::roots::Domain;
using prism::roots::Node;
constexpr double sr=48000.0;
constexpr std::size_t N=8192;

bool near(double a,double b,double eps){return std::abs(a-b)<=eps;}
void fill(std::vector<float>& l,std::vector<float>& r,
          double midAmp,double sideAmp,double freq=1000.0){
    for(std::size_t i=0;i<l.size();++i){
        const double sine=std::sin(2.0*std::numbers::pi*freq*i/sr);
        l[i]=(float)((midAmp+sideAmp)*sine);
        r[i]=(float)((midAmp-sideAmp)*sine);
    }
}
double rms(const std::vector<float>& a,std::size_t skip=0){
    double sum=0;
    for(std::size_t i=skip;i<a.size();++i)sum+=(double)a[i]*a[i];
    return std::sqrt(sum/(double)(a.size()-skip));
}

int main(){
    // PROOF 1: bypass unity, nonidentical stereo, sample-accurate.
    {
        Engine core(sr);std::vector<float> l(N),r(N);
        fill(l,r,0.25,0.075);
        auto originalL=l,originalR=r;
        core.process(l.data(),r.data(),N);
        for(std::size_t i=0;i<N;++i)
            if(!near(l[i],originalL[i],1e-6)||
               !near(r[i],originalR[i],1e-6)){
                std::cerr<<"PROOF 1 failed: non-neutral bypass\n";return 1;
            }
        std::cout<<"PROOF 1 PASS: neutral stereo bypass\n";
    }

    // PROOF 2: two overlapping nodes in one territory (MID) add gain;
    // node bounds +/-60 dB are enforced.
    {
        Engine single(sr),paired(sr);
        Node n{true,1000.0,6.0,2.5,Domain::stereo};
        single.updateNode(8,n);
        paired.updateNode(8,n);paired.updateNode(9,n);
        std::vector<float> l(N),r(N),l2(N),r2(N);
        fill(l,r,0.2,0.0);l2=l;r2=r;
        const auto dry=l;
        single.process(l.data(),r.data(),N);
        paired.process(l2.data(),r2.data(),N);
        const double one=20*std::log10(rms(l,N/2)/rms(dry,N/2));
        const double two=20*std::log10(rms(l2,N/2)/rms(dry,N/2));
        n.gainDb=1000.0;paired.updateNode(13,n);
        if(!near(one,6.0,0.25)||!near(two,12.0,0.4)||
           paired.node(13).gainDb!=60.0){
            std::cerr<<"PROOF 2 failed: "<<one<<" "<<two<<"\n";return 2;
        }
        std::cout<<"PROOF 2 PASS: overlapping EQ and gain range\n";
    }

    // PROOF 3: MID-only filter leaves pure SIDE energy unchanged;
    // explicit MONO is sample-identical L == R.
    {
        Engine core(sr);
        core.updateNode(8,Node{true,1000.0,12.0,2.0,Domain::mid});
        std::vector<float> l(N),r(N);
        fill(l,r,0.0,0.1);
        const auto dry=l;
        core.process(l.data(),r.data(),N);
        if(!near(rms(l,N/2),rms(dry,N/2),2e-4)){
            std::cerr<<"PROOF 3 failed: MID EQ affected SIDE\n";return 3;
        }
        core.reset();core.setMono(true);
        fill(l,r,0.1,0.05);
        core.process(l.data(),r.data(),N);
        for(std::size_t i=0;i<N;++i)if(l[i]!=r[i]){
            std::cerr<<"PROOF 3 failed: MONO L != R\n";return 3;
        }
        std::cout<<"PROOF 3 PASS: MID/SIDE isolation, exact mono\n";
    }

    // PROOF 4: widening guard measures correlation; protects positively
    // correlated stereo, and doesn't invent an irreversible polarity flip.
    {
        Engine core(sr);core.setSpace(100.0);
        std::vector<float> l(N),r(N);
        for(int repeat=0;repeat<8;++repeat){
            for(std::size_t i=0;i<N;++i){
                const double t=(double)(i+repeat*N)/sr;
                const double m=0.08*std::sin(2*std::numbers::pi*750*t);
                const double s=0.075*std::sin(2*std::numbers::pi*1100*t);
                l[i]=(float)(m+s);r[i]=(float)(m-s);
            }
            core.process(l.data(),r.data(),N);
        }
        const auto met=core.meter();
        if(met.correlation<-0.025||met.appliedWidth>1.11||
           met.appliedWidth<1.0){
            std::cerr<<"PROOF 4 failed: correlation "<<met.correlation
                     <<" width "<<met.appliedWidth<<"\n";return 4;
        }
        core.reset();core.setSpace(50.0);fill(l,r,0.0,0.1);
        const auto dry=l;
        core.process(l.data(),r.data(),N);
        if(core.meter().correlation>-0.99||
           !near(rms(l,N/2),rms(dry,N/2),1e-4)){
            std::cerr<<"PROOF 4 failed: modified existing antiphase\n";return 4;
        }
        std::cout<<"PROOF 4 PASS: measured phase guard, no polarity flip\n";
    }
    std::cout<<"PRISM ROOTS — FOUR DSP PROOFS PASSED\n";
    return 0;
}

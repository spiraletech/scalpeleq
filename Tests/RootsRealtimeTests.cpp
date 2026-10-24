#include "../Source/RootsAudioEngine.h"
#include <cmath>
#include <iostream>
#include <numbers>
#include <vector>
using namespace prism::roots::v2;
constexpr double sr=48000.0;
constexpr int n=16384;
bool approx(double a,double b,double tol){return std::abs(a-b)<tol;}
double amplitude(const std::vector<float>& a,double hz) {
    double re=0.0,im=0.0;
    for(int i=n/2;i<n;++i) {
        const double ang=2.0*std::numbers::pi*hz*i/sr;
        re+=a[(size_t)i]*std::cos(ang); im+=a[(size_t)i]*std::sin(ang);
    }
    return 2.0*std::hypot(re,im)/(n/2);
}
double rms(const std::vector<float>& a) {
    double v=0;
    for(int i=n/2;i<n;++i)v+=(double)a[(size_t)i]*a[(size_t)i];
    return std::sqrt(v/(n/2));
}
void sine(std::vector<float>& l,std::vector<float>& r,double hz,double amp,bool anti=false) {
    for(int i=0;i<n;++i){
        const float x=(float)(amp*std::sin(2.0*std::numbers::pi*hz*i/sr));
        l[(size_t)i]=x;r[(size_t)i]=anti?-x:x;
    }
}
int main(){
    // PROOF A: exact bypass (zero-gain filters, no compression, SPACE50).
    {
        Engine engine(sr);std::vector<float> l(n),r(n);
        for(int i=0;i<n;++i){l[i]=(float)(std::sin(i*0.071)*0.2);r[i]=(float)(std::cos(i*0.113)*0.1);}
        const auto beforeL=l,beforeR=r;
        engine.process(l.data(),r.data(),n);
        if(l!=beforeL||r!=beforeR){std::cerr<<"A FAIL bypass\n";return 1;}
        std::cout<<"A PASS bit-exact neutral bypass\n";
    }
    // PROOF B: Exactly one gain per parent ROOT, with ±60 dB limits.
    // Changing either boundary alters the gain territory without assigning
    // any gain or vertical movement to the handle.
    {
        Engine engine(sr);std::vector<float> l(n),r(n);
        auto left=engine.get(0);left.hz=150.0;engine.config(0,left);
        auto right=engine.get(1);right.hz=2400.0;engine.config(1,right);
        sine(l,r,1000.0,0.01);
        const auto dry=l;
        // A legacy node gain must NEVER affect samples anymore.
        left.gainDb=38.0;engine.config(0,left);
        if(engine.get(0).gainDb!=0.0)return 2;
        engine.process(l.data(),r.data(),n);
        if(l!=dry||r!=dry){
            std::cerr<<"B FAIL obsolete handle gain still audible\n";return 2;
        }
        engine.setRootGain(0,6.0);
        if(engine.rootGain(0)!=6.0 || engine.rootGain(1)!=0.0)return 2;
        sine(l,r,1000.0,0.01);
        engine.process(l.data(),r.data(),n);
        const double boost=20.0*std::log10(amplitude(l,1000.0)/amplitude(dry,1000.0));
        if(!(boost>2.5&&boost<8.0)){
            std::cerr<<"B FAIL root gain boost "<<boost<<"\n";return 2;
        }
        engine.setRootGain(0,-6.0);
        sine(l,r,1000.0,0.01);
        engine.process(l.data(),r.data(),n);
        const double cut=20.0*std::log10(amplitude(l,1000.0)/amplitude(dry,1000.0));
        if(!(cut<1.0)){
            std::cerr<<"B FAIL root gain attenuation "<<cut<<"\n";return 2;
        }
        engine.setRootGain(0,10000.0);
        if(engine.rootGain(0)!=60.0)return 2;
        engine.setRootGain(0,-10000.0);
        if(engine.rootGain(0)!=-60.0)return 2;
        std::cout<<"B PASS per-root-only gain, retired node gain, and ±60 clamp\n";
    }
    // PROOF C: SOLO listens to the interval defined by TWO nodes of ROOT 1.
    // Selecting/switching ROOT 2 changes the audition over an 8ms crossfade.
    {
        Engine engine(sr);std::vector<float> l(n),r(n);
        for(int i=0;i<n;++i){
            const float x=(float)(0.15*std::sin(2*std::numbers::pi*100*i/sr)
                       +0.15*std::sin(2*std::numbers::pi*1000*i/sr));
            l[(size_t)i]=r[(size_t)i]=x;
        }
        auto a=engine.get(0);a.hz=550;engine.config(0,a);
        auto b=engine.get(1);b.hz=1800;engine.config(1,b);
        engine.setSoloRoot(0);
        engine.process(l.data(),r.data(),n);
        const auto high=amplitude(l,1000.0);
        const auto low=amplitude(l,100.0);
        if(!(high>0.075&&high>8.0*low)){
            std::cerr<<"C FAIL root-band specificity "<<high<<","<<low<<"\n";return 3;
        }
        // The same parent ROOT gain must work while that ROOT is soloed.
        engine.setRootGain(0,6.0);
        sine(l,r,1000.0,0.15);
        engine.process(l.data(),r.data(),n);
        if(!(amplitude(l,1000.0)>high*1.65)){
            std::cerr<<"C FAIL parent gain not audible in ROOT SOLO\n";return 3;
        }
        engine.setSoloRoot(1);
        std::vector<float> shortL(128),shortR(128);
        for(int i=0;i<128;++i)shortL[(size_t)i]=shortR[(size_t)i]=(float)
             (0.15*std::sin(2*std::numbers::pi*100.0*(i+n)/sr)
             +0.15*std::sin(2*std::numbers::pi*1000.0*(i+n)/sr));
        engine.process(shortL.data(),shortR.data(),128);
        if(engine.soloTransitionSamplesRemaining()<=0){
            std::cerr<<"C FAIL absent root-level crossfade\n";return 3;
        }
        std::vector<float> remainingL(512),remainingR(512);
        engine.process(remainingL.data(),remainingR.data(),512);
        if(engine.soloTransitionSamplesRemaining()!=0||engine.soloRoot()!=1){
            std::cerr<<"C FAIL unfinished root-level crossfade\n";return 3;
        }
        std::cout<<"C PASS whole-root bandpass audition and 8ms A/B crossfade\n";
    }
    // PROOF D: selected FET-inspired compressor attenuates a strong
    // sustained band, mono L/R exact; phase guard preserves default anti-phase.
    {
        Engine engine(sr);std::vector<float> l(n),r(n);
        sine(l,r,1000,0.65);
        const double dry=rms(l);
        // Test inside the ROOT passband, not at the 2-pole filter
        // crossover where partial cancellation can raise broadband RMS.
        Config c=engine.get(0);c.hz=200;c.q=2.0;c.comp=Compressor::fet4;
        c.thresholdDb=-30.0;c.detector=Detector::linked;
        engine.config(0,c);
        auto upper=engine.get(1);upper.hz=3000.0;engine.config(1,upper);
        engine.process(l.data(),r.data(),n);
        if(!(rms(l)<dry*0.96 && engine.meter().largestGrDb>1.0)){
            std::cerr<<"D FAIL compression "<<rms(l)<<" / "<<dry<<"\n";return 4;
        }
        engine.prepare(sr);engine.setMono(true);sine(l,r,1000,0.23,true);
        engine.process(l.data(),r.data(),n);
        for(int i=0;i<n;++i) if(l[i]!=r[i]){
            std::cerr<<"D FAIL mono\n";return 4;
        }
        engine.prepare(sr);engine.setMono(false);engine.setSpace(50.0);
        sine(l,r,1000,0.2,true);
        engine.process(l.data(),r.data(),n);
        if(engine.meter().correlation> -0.99){
            std::cerr<<"D FAIL existing antiphase preserved\n";return 4;
        }
        std::cout<<"D PASS compressor gain reduction and mono/phase\n";
    }
    // PROOF E: ONYX changes only the selected spectral ROOT, and threshold
    // produces measurably more gain reduction at a lower setting.
    {
        auto render=[](double frequency,double sparkle,double threshold,bool comp) {
            Engine engine(sr);
            Config c=engine.get(0);
            c.hz=500.0;c.comp=comp?Compressor::fet4:Compressor::off;
            c.thresholdDb=threshold;c.sparkle=sparkle;
            engine.config(0,c);
            auto upper=engine.get(1);upper.hz=1800.0;engine.config(1,upper);
            std::vector<float> l(n),r(n);
            sine(l,r,frequency,0.35);
            engine.process(l.data(),r.data(),n);
            return rms(l);
        };
        const double dryIn=render(1000.0,0.0,-30.0,false);
        const double wetIn=render(1000.0,1.0,-30.0,false);
        const double dryOut=render(8000.0,0.0,-30.0,false);
        const double wetOut=render(8000.0,1.0,-30.0,false);
        const double deltaIn=std::abs(wetIn-dryIn);
        const double deltaOut=std::abs(wetOut-dryOut);
        if(!(deltaIn>0.0001 && deltaIn>deltaOut*2.0)){
            std::cerr<<"E FAIL ONYX territory isolation "<<deltaIn<<","<<deltaOut<<"\n";
            return 5;
        }
        const double light=render(1000.0,0.0,-5.0,true);
        const double heavy=render(1000.0,0.0,-40.0,true);
        if(!(heavy<light*0.96)){
            std::cerr<<"E FAIL threshold ineffective "<<heavy<<","<<light<<"\n";
            return 6;
        }
        std::cout<<"E PASS per-ROOT ONYX isolation and threshold reduction\n";
    }
    std::cout<<"FOUR ROOTS v2 AUDIO PROOFS PASSED\n";
    return 0;
}

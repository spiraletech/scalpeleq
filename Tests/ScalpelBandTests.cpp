#include "../Source/RootsAudioEngine.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <numbers>
#include <utility>
#include <vector>

namespace {
struct Result {double energy=0,peak=-144;};
struct PhaseResult {double error=0,rms=0,correlation=0;};
PhaseResult phaseAlignment(bool correction,double offset=1.10,
                           prism::roots::v2::Domain domain=prism::roots::v2::Domain::stereo){
    constexpr int sr=48000,count=96000;
    constexpr double frequency=350.0;
    prism::roots::v2::Engine engine(sr);
    using namespace prism::roots::v2;
    Config low=engine.get(6),high=engine.get(7);
    low.hz=250;high.hz=500;low.phase=correction;low.enabled=true;low.domain=high.domain=domain;
    engine.config(6,low);engine.config(7,high);
    std::vector<float> l(count),r(count);
    for(int i=0;i<count;++i){
        const double phase=2*std::numbers::pi*frequency*i/sr;
        l[(size_t)i]=(float)(.4*std::sin(phase));
        r[(size_t)i]=(float)(.4*std::sin(phase-offset));
    }
    engine.process(l.data(),r.data(),count);
    double ls=0,lc=0,rs=0,rc=0,ll=0,rr=0,lr=0;
    const int begin=count/2;
    for(int i=begin;i<count;++i){
        const double phase=2*std::numbers::pi*frequency*i/sr;
        const double sn=std::sin(phase),cs=std::cos(phase);
        ls+=l[(size_t)i]*sn;lc+=l[(size_t)i]*cs;
        rs+=r[(size_t)i]*sn;rc+=r[(size_t)i]*cs;
        ll+=l[(size_t)i]*l[(size_t)i];
        rr+=r[(size_t)i]*r[(size_t)i];
        lr+=l[(size_t)i]*r[(size_t)i];
    }
    const double phaseL=std::atan2(lc,ls);
    const double phaseR=std::atan2(rc,rs);
    const double error=std::abs(std::remainder(
        phaseR-phaseL,2*std::numbers::pi));
    const double n=(double)(count-begin);
    return {error,std::sqrt((ll+rr)/(2*n)),
            lr/std::sqrt(std::max(1.0e-20,ll*rr))};
}
Result run(bool phase,double limit,bool opposing,double amplitude){
    constexpr int sr=48000,count=48000;
    prism::roots::v2::Engine engine(sr);
    using namespace prism::roots::v2;
    Config low=engine.get(6),high=engine.get(7);
    low.hz=250;high.hz=500;
    low.phase=phase;low.limit=limit;low.enabled=true;
    engine.config(6,low);engine.config(7,high);
    std::vector<float> l(count),r(count);
    for(int i=0;i<count;++i){
        const float x=(float)(amplitude*std::sin(2*std::numbers::pi*350*i/sr));
        l[(size_t)i]=x;r[(size_t)i]=opposing?-x:x;
    }
    engine.process(l.data(),r.data(),count);
    double rms=0;
    for(int i=count/2;i<count;++i){
        assert(std::isfinite(l[(size_t)i])&&std::isfinite(r[(size_t)i]));
        rms+=(double)l[(size_t)i]*l[(size_t)i]+(double)r[(size_t)i]*r[(size_t)i];
    }
    return {std::sqrt(rms/count),engine.meter().rootPeakDb[3]};
}
double bandRms(double hz,bool bandEnabled,bool mono=false,bool antiPhase=false){
    constexpr int sr=48000,count=48000;
    using namespace prism::roots::v2;
    Engine engine(sr);
    auto lo=engine.get(6),hi=engine.get(7);
    lo.hz=250;hi.hz=500;lo.enabled=bandEnabled;hi.enabled=bandEnabled;
    engine.config(6,lo);engine.config(7,hi);
    engine.setRootMono(3,mono);
    std::vector<float> l(count),r(count);
    for(int i=0;i<count;++i){
        const float v=(float)(0.6*std::sin(2*std::numbers::pi*hz*i/sr));
        l[(size_t)i]=v;r[(size_t)i]=antiPhase?-v:v;
    }
    engine.process(l.data(),r.data(),count);
    double rms=0;
    for(int i=count/2;i<count;++i){
        const double v=antiPhase?(l[(size_t)i]-r[(size_t)i])*0.5:l[(size_t)i];
        assert(std::isfinite(v));rms+=v*v;
    }
    return std::sqrt(rms/(count/2));
}
std::pair<double,double> domainRms(prism::roots::v2::Domain domain,bool solo){
    constexpr int sr=48000,count=48000;
    using namespace prism::roots::v2;
    Engine engine(sr);
    auto lo=engine.get(0),hi=engine.get(1);
    lo.hz=200;hi.hz=2300;lo.domain=hi.domain=domain;
    engine.config(0,lo);engine.config(1,hi);
    engine.setRootGain(0,solo?0:9);
    if(solo)engine.setSoloRoot(0);
    std::vector<float> l(count),r(count);
    for(int i=0;i<count;++i){
        const double a=2*std::numbers::pi*900*i/sr;
        const double m=.18*std::sin(a),s=.12*std::cos(a);
        l[(size_t)i]=(float)(m+s);r[(size_t)i]=(float)(m-s);
    }
    engine.process(l.data(),r.data(),count);
    double mid=0,side=0;
    for(int i=count/2;i<count;++i){
        const double m=(l[(size_t)i]+r[(size_t)i])*.5;
        const double s=(l[(size_t)i]-r[(size_t)i])*.5;
        mid+=m*m;side+=s*s;
    }
    return {std::sqrt(mid/(count/2)),std::sqrt(side/(count/2))};
}
double soloMonoResidual(){
    constexpr int sr=48000,count=32768;
    using namespace prism::roots::v2;
    Engine engine(sr);
    auto lo=engine.get(8),hi=engine.get(9);
    lo.hz=500;hi.hz=2000;lo.tape=.5;lo.sparkle=.5;
    engine.config(8,lo);engine.config(9,hi);
    engine.setRootGain(4,12);engine.setRootMono(4,true);engine.setSoloRoot(4);
    std::vector<float> l(count),r(count);
    for(int i=0;i<count;++i){
        const double x=.12*std::sin(2*std::numbers::pi*900*i/sr)
            +.035*std::sin(2*std::numbers::pi*6200*i/sr);
        l[(size_t)i]=(float)x;r[(size_t)i]=(float)(.85*x);
    }
    engine.process(l.data(),r.data(),count);
    double maximum=0;
    for(int i=count/2;i<count;++i)
        maximum=std::max(maximum,std::abs((double)l[(size_t)i]-r[(size_t)i]));
    return maximum;
}
}
// Run the same CARVE-before-dynamics path used by the production adapter.
double preUiRender(double gain,prism::roots::v2::Compressor comp,double offset,
                   double level,bool side,bool solo,double* other=nullptr,double ratio=0.0) {
    using namespace prism::roots::v2;
    constexpr int rate=48000;
    Engine engine(rate);
    surgery::CarveEngine carve;
    carve.prepare(rate);
    carve.bounds(4,500,2000);
    const auto channel=side?surgery::Channel::side:surgery::Channel::mid;
    carve.rootDomain(4,channel);
    surgery::CarveConfig eq;
    eq.enabled=true;eq.hz=1000;eq.q=2;eq.gainDb=gain;
    carve.configure(4,0,eq);
    auto band=engine.get(8);
    band.comp=comp;band.compOffsetDb=offset;band.compRatio=ratio;
    band.domain=side?Domain::side:Domain::mid;
    engine.config(8,band);
    if(solo)engine.setSoloRoot(4);
    std::vector<float> left(rate),right(rate);
    for(int i=0;i<rate;++i){
        const double x=level*std::sin(2*std::numbers::pi*i*1000/rate);
        left[(size_t)i]=(float)x;
        right[(size_t)i]=(float)(side?-x:x);
    }
    engine.process(left.data(),right.data(),left.size(),&carve);
    double energy=0.0,leak=0.0;
    for(int i=rate/2;i<rate;++i){
        const double l=left[(size_t)i],r=right[(size_t)i];
        const double processed=(side?l-r:l+r)*0.5;
        const double untouched=(side?l+r:l-r)*0.5;
        energy+=processed*processed;
        leak+=untouched*untouched;
    }
    if(other)*other=std::sqrt(leak/(rate/2));
    return 10.0*std::log10(energy/(level*level*rate/4));
}

bool verifyBandGain(){
    using namespace prism::roots::v2;
    for(const double rate:{44100.,48000.,96000.}){
        for(int root=0;root<kTerritories;++root){
            const auto low=Engine::defaults[(size_t)(root*2)];
            const auto high=Engine::defaults[(size_t)(root*2+1)];
            const auto band=scalpel::bands::GainBand::fromEdges(rate,low,high);
            const auto z=[rate](double hz){return std::polar(1.0,-2*std::numbers::pi*hz/rate);};
            for(const double target:{-60.,-12.,12.,60.}){
                const scalpel::bands::GainResponse response(band,target);
                for(const auto& sample:std::array<std::pair<double,double>,3>{{
                    {low,target*.5},{band.centerHz,target},{high,target*.5}}}){
                    const auto measured=20.0*std::log10(std::abs(response.transfer(z(sample.first))));
                    if(!std::isfinite(measured)||std::abs(measured-sample.second)>.003)return false;
                }
                if((root!=0&&root!=3&&root!=6)||std::abs(target)!=60.)continue;
                Engine engine(rate);
                engine.setRootGain(root,target);
                constexpr double amplitude=.0005;
                const int count=(int)(rate*2.6),start=(int)(rate*2.);
                std::vector<float> l(count),r(count);
                for(int i=0;i<count;++i)
                    r[(size_t)i]=l[(size_t)i]=(float)(amplitude*std::sin(2*std::numbers::pi*band.centerHz*i/rate));
                engine.process(l.data(),r.data(),l.size());
                double out=0.0,in=0.0;
                for(int i=start;i<count;++i){
                    const auto dry=amplitude*std::sin(2*std::numbers::pi*band.centerHz*i/rate);
                    in+=dry*dry;
                    out+=(double)l[(size_t)i]*l[(size_t)i];
                }
                const auto measured=10.0*std::log10(out/in);
                if(!std::isfinite(measured)||std::abs(measured-target)>.12){
                    std::cerr<<"Band gain endpoint "<<rate<<' '<<root+1<<' '<<target<<' '<<measured<<"\n";
                    return false;
                }
            }
        }
    }
    return true;
}

double halfReleaseMs(prism::roots::v2::Compressor profile,int burstMs,double& firstDb){
    using namespace prism::roots::v2;
    constexpr int rate=48000,frame=48,root=4;
    Engine engine(rate);
    auto c=engine.get(root*2);
    c.comp=profile;
    c.compOffsetDb=-9;
    engine.config(root*2,c);
    std::array<float,frame> left{},right{};
    for(int t=0;t<500;++t)engine.process(left.data(),right.data(),frame);
    double endDb=0.0;
    for(int t=0;t<burstMs+500;++t){
        for(int i=0;i<frame;++i){
            const auto phase=2*std::numbers::pi*1000*(t*frame+i)/rate;
            left[(size_t)i]=right[(size_t)i]=t<burstMs?(float)(.35*std::sin(phase)):0.f;
        }
        engine.process(left.data(),right.data(),frame);
        const auto gr=engine.meter().dynamics[root].reductionDb;
        if(t==0)firstDb=gr;
        if(t==burstMs-1)endDb=gr;
        if(t>=burstMs&&endDb>0.0&&gr<=endDb*.5)return t-burstMs+1;
    }
    return 0.0;
}

int main(){
    using prism::roots::v2::Compressor;
    if(!verifyBandGain())return 86;
    std::cout<<"BAND GAIN: shared analytic shoulders and center in all seven bands; measured ±60 dB at 44.1/48/96 kHz\n";
    for(const double gain:{-40.,-12.,0.,12.,40.}){
        double leakage=1.0;
        const double measured=preUiRender(gain,Compressor::off,0.0,.001,true,false,&leakage);
        if(!std::isfinite(measured)||std::abs(measured-gain)>.03||leakage>1e-8){
            std::cerr<<"Signed CARVE mismatch: "<<gain<<" -> "<<measured<<" leakage "<<leakage<<"\n";
            return 80;
        }
    }
    const double clean=preUiRender(0,Compressor::dynamics176E,0,.35,false,false);
    const double dense=preUiRender(0,Compressor::dynamicsCE1B,0,.35,false,false);
    const double driven=preUiRender(0,Compressor::dynamics176E,-9,.35,false,false);
    const double quiet=preUiRender(0,Compressor::dynamics176E,0,.015,false,false);
    const double carved=preUiRender(12,Compressor::dynamics176E,0,.08,false,false);
    const double audition=preUiRender(12,Compressor::off,0,.005,true,true);
    if(!(clean<0&&dense<clean&&driven<clean&&quiet>0.5&&carved<11.5&&audition>9.0)){
        std::cerr<<"Pre-UI dynamics/solo: "<<clean<<' '<<dense<<' '<<driven<<' '<<quiet<<' '<<carved<<' '<<audition<<"\n";
        return 81;
    }
    for(const auto comp:{Compressor::dynamics176E,Compressor::dynamicsCE1B}){
        const double loudNeg=preUiRender(0,comp,-9,.35,false,false);
        const double loudCenter=preUiRender(0,comp,0,.35,false,false);
        const double loudPos=preUiRender(0,comp,9,.35,false,false);
        const double quietNeg=preUiRender(0,comp,-9,.012,false,false);
        const double quietCenter=preUiRender(0,comp,0,.012,false,false);
        const double quietPos=preUiRender(0,comp,9,.012,false,false);
        const double silenceFloor=preUiRender(0,comp,9,1e-5,false,false);
        if(!(loudNeg<loudCenter-1.0&&loudCenter<loudPos-1.0&&std::abs(quietNeg)<.04
            &&quietCenter>1.0&&quietPos>quietCenter+.50&&quietPos<=9.05&&std::abs(silenceFloor)<.04)){
            std::cerr<<"Signed comp threshold: "<<loudNeg<<' '<<loudCenter<<' '<<loudPos<<" / "
                <<quietNeg<<' '<<quietCenter<<' '<<quietPos<<" / "<<silenceFloor<<"\n";return 85;
        }
    }
    double fastAttack=0.0,opticalAttack=0.0;
    const auto fastRelease=halfReleaseMs(Compressor::dynamics176E,500,fastAttack);
    const auto shortRelease=halfReleaseMs(Compressor::dynamicsCE1B,10,opticalAttack);
    const auto sustainedRelease=halfReleaseMs(Compressor::dynamicsCE1B,500,opticalAttack);
    if(!(fastRelease>0.0&&shortRelease>0.0&&fastAttack>opticalAttack
        &&sustainedRelease>shortRelease*1.25)){
        std::cerr<<"Dynamics timing: "<<fastAttack<<' '<<opticalAttack<<' '
            <<shortRelease<<' '<<sustainedRelease<<"\n";
        return 87;
    }
    std::cout<<"Signed CARVE endpoints, threshold polarity, peak attack and optical release memory verified\n";

    // PHASE CORRECTION must reduce a measured L/R phase offset without
    // achieving the result by attenuating stereo energy.
    for(const auto profile:{Compressor::dynamics176E,Compressor::dynamicsCE1B}){
        const double low=preUiRender(0,profile,0,.5,false,false,nullptr,2.0);
        const double high=preUiRender(0,profile,0,.5,false,false,nullptr,12.0);
        if(!(high<low-1.0))return 82;
    }
    const auto phaseDry=phaseAlignment(false);
    const auto phaseCorrected=phaseAlignment(true);
    assert(phaseDry.error>0.80);
    assert(phaseCorrected.error<phaseDry.error*0.45);
    assert(phaseCorrected.correlation>phaseDry.correlation+0.30);
    assert(phaseCorrected.rms>phaseDry.rms*0.80);
    assert(phaseCorrected.rms<phaseDry.rms*1.20);
    for(const double offset:{-1.10,0.0,1.10,3.141592653589793}){
        const auto dry=phaseAlignment(false,offset);
        const auto corrected=phaseAlignment(true,offset);
        // The dry portion outside the band remains in the mix, including
        // at a full polarity reversal through finite filter skirts.
        const bool oppositePolarity=offset>3.0;
        if(corrected.error>(oppositePolarity?0.55:0.40)
            ||std::abs(corrected.rms/dry.rms-1.0)>(oppositePolarity?0.20:0.13)){
            std::cerr<<"Stereo phase error or energy: "<<offset<<' '<<corrected.error<<' '<<corrected.rms/dry.rms<<"\n";
            return 83;
        }
    }
    for(const auto domain:{prism::roots::v2::Domain::mid,prism::roots::v2::Domain::side}){
        const auto routedDry=phaseAlignment(false,1.1,domain);
        const auto routedWet=phaseAlignment(true,1.1,domain);
        if(routedDry.error!=routedWet.error||routedDry.rms!=routedWet.rms)return 84;
    }
    // Optical amount applies measurable reduction without brickwall claims.
    const auto loudDry=run(false,0,false,.95);
    const auto loudOpto=run(false,1,false,.95);
    assert(loudOpto.energy<loudDry.energy);
    assert(loudOpto.energy>0);
    assert(std::isfinite(loudOpto.peak));
    // Off is fully neutral to band processing where defaults are all zero.
    const auto neutral=run(false,0,false,.95);
    assert(neutral.energy==loudDry.energy);
    // BAND OFF is a true deep spectral mute, not an old neutral bypass.
    const auto inside=bandRms(350,true);
    const auto muted=bandRms(350,false);
    const auto outside=bandRms(8000,false);
    const auto outsideReference=bandRms(8000,true);
    assert(muted<inside*.35);
    assert(outside>outsideReference*.65);
    // MONO works on the selected band's anti-phase SIDE signal.
    const auto stereoSide=bandRms(350,true,false,true);
    const auto monoSide=bandRms(350,true,true,true);
    assert(monoSide<stereoSide*.7);
    // EQ DOMAIN is a real per-band MID/SIDE processing target: with +9 dB
    // ROOT gain, MID alters only MID and SIDE alters only SIDE.
    const auto mid=domainRms(prism::roots::v2::Domain::mid,false);
    const auto side=domainRms(prism::roots::v2::Domain::side,false);
    const double baselineM=.18/std::sqrt(2),baselineS=.12/std::sqrt(2);
    assert(mid.first>baselineM*1.08&&std::abs(mid.second-baselineS)<.005);
    assert(side.second>baselineS*1.08&&std::abs(side.first-baselineM)<.005);
    // SOLO audition must obey the selected EQ DOMAIN even at neutral gain.
    const auto soloMid=domainRms(prism::roots::v2::Domain::mid,true);
    const auto soloSide=domainRms(prism::roots::v2::Domain::side,true);
    assert(soloMid.first>.01&&soloMid.second<1.0e-4);
    assert(soloSide.second>.01&&soloSide.first<1.0e-4);
    assert(soloMonoResidual()<1e-6);
    std::cout<<"Scalpel bands: OFF spectral mute, MONO selected-side reduction, MID/SIDE EQ DOMAIN routing and SOLO audition, PHASE CORRECTION, LIMIT optical leveling PASS\n";
}

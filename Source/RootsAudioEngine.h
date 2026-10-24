#pragma once
// PRISM ROOTS realtime engine v2: 14 independent overlapping EQ bands, stereo
// correlation guard, frequency-band audition and inspired compressor profiles.
// Standalone C++20; no allocation, locks, logging or DAW calls in process().
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include "RootsSpectrumColours.h"
#include "SurgeryOnyx.h"

namespace prism::roots::v2 {
constexpr int kTerritories=7,kNodes=14;
enum class Domain:int {stereo=0,mid=1,side=2};
enum class Compressor:int {off=0,optical3=1,optical4=2,tube3=3,fet4=4};
enum class Detector:int {linked=0,dual=1,mid=2,side=3};
struct Config {
    double hz=1000.0,gainDb=0.0,q=1.0,space=50.0,thresholdDb=0.0;
    Domain domain=Domain::stereo;
    Compressor comp=Compressor::off;
    Detector detector=Detector::linked;
    double dynamic=0.0,sparkle=0.0,shine=0.0,smooth=0.0,tape=0.0;
    bool phase=false;
    double limit=0.0;
    bool enabled=true,solo=false;
};
struct Meter {
    double correlation=1.0,midRms=0.0,sideRms=0.0,spaceApplied=1.0,largestGrDb=0.0;
    std::array<double,kTerritories> rootPeakDb{};
};

class Filter {
public:
    void peak(double sr,const Config& c) noexcept {
        const double f=std::clamp(c.hz,1.0,std::min(20000.0,sr*0.45));
        const double w=2.0*std::numbers::pi*f/sr;
        const double a=std::sin(w)/(2.0*std::clamp(c.q,0.15,24.0));
        const double g=std::pow(10.0,std::clamp(c.gainDb,-60.0,60.0)/40.0);
        const double norm=1.0/(1.0+a/g);
        b0_=(1.0+a*g)*norm; b1_=-2.0*std::cos(w)*norm;
        b2_=(1.0-a*g)*norm; a1_=b1_; a2_=(1.0-a/g)*norm;
    }
    // RBJ constant skirt-gain band pass: peak is Q, so normalization by
    // Q gives unity at center frequency. Spectral audition is frequency-only,
    // NOT blind instrument/source separation.
    void tap(double sr,const Config& c) noexcept {
        const double w=2.0*std::numbers::pi*std::clamp(
                         c.hz,1.0,std::min(20000.0,sr*0.45))/sr;
        const double a=std::sin(w)/(2.0*std::clamp(c.q,0.15,24.0));
        const double norm=1.0/(1.0+a);
        b0_=a*norm;b1_=0.0;b2_=-a*norm;
        a1_=-2.0*std::cos(w)*norm;a2_=(1.0-a)*norm;
    }
    double sample(double v) noexcept {
        const double out=b0_*v+z1_;
        z1_=b1_*v-a1_*out+z2_;
        z2_=b2_*v-a2_*out;
        return out;
    }
    void setEdge(double sr,double frequency,bool highPass) noexcept {
        const double hz=std::clamp(frequency,20.0,std::min(20000.0,sr*0.45));
        const double w=2.0*std::numbers::pi*hz/sr;
        const double co=std::cos(w),alpha=std::sin(w)/(2.0*0.7071067811865476);
        const double norm=1.0/(1.0+alpha);
        b0_=(highPass?(1.0+co):(1.0-co))*0.5*norm;
        b1_=(highPass?-(1.0+co):(1.0-co))*norm;
        b2_=b0_;a1_=-2.0*co*norm;a2_=(1.0-alpha)*norm;
    }
    void reset() noexcept {z1_=z2_=0.0;}
private:
    double b0_=1.0,b1_=0.0,b2_=0.0,a1_=0.0,a2_=0.0,z1_=0.0,z2_=0.0;
};

class Engine {
public:
    explicit Engine(double sampleRate=48000.0) noexcept { prepare(sampleRate); }
    static constexpr std::array<double,kNodes> defaults{
        20.,60.,60.,120.,120.,250.,250.,500.,500.,2000.,2000.,6000.,6000.,20000.
    };
    void prepare(double sampleRate) noexcept {
        sr_=std::max(8000.0,sampleRate);
        correlationAlpha_=std::exp(-1.0/(0.025*sr_));
        sideAlpha_=std::exp(-1.0/(0.008*sr_));
        gainSmoothingAlpha_=std::exp(-1.0/(0.010*sr_));
        optoAttackAlpha_=std::exp(-1.0/(0.010*sr_));
        optoReleaseAlpha_=std::exp(-1.0/(0.350*sr_));
        optoReleaseDeepAlpha_=std::exp(-1.0/(1.000*sr_));
        globalMidPower_=globalSidePower_=0.0;appliedSpace_=1.0;
        grMax_=0.0;meter_={};
        currentSolo_=-1;previousSolo_=-1;previousRequest_=-1;soloWet_=0;soloFadeLeft_=0;soloFadeSamples_=0;
        smoothedRootDelta_.fill(0.0);
        for(int i=0;i<kNodes;++i) {
            auto& node=nodes_[(size_t)i];
            if(!haveConfig_) {node.config.hz=defaults[(size_t)i];}
            node.midEq.reset();node.sideEq.reset();
            node.midTap.reset();node.sideTap.reset();
            node.e1=node.e2=node.sideEnergy=node.midEnergy=0.0;
            node.optoEnvelope=0.0;node.phaseMix=0.0;
            node.onyxMid.prepare(sr_);node.onyxSide.prepare(sr_);
            node.fx={node.config.dynamic,node.config.sparkle,node.config.shine,node.config.smooth,node.config.tape};
            node.enableMix=node.config.enabled?1.0:0.0;
            node.monoMix=rootMono_[(size_t)(i/2)]?1.0:0.0;
            node.compMix=node.config.comp==Compressor::off?0.0:1.0;
            node.threshold=node.config.thresholdDb;
            updateFilters(i);
            config(i,node.config);
        }
        haveConfig_=true;
        for(int root=0;root<kTerritories;++root){
            auto& taps=soloBands_[(size_t)root];
            for(auto& f:taps.filters)f.reset();
            for(auto& f:rootGainBands_[(size_t)root].filters)f.reset();
            for(auto& f:rootFxBands_[(size_t)root].filters)f.reset();
            updateSoloBand(root);
        }
    }
    void config(int index, Config c) noexcept {
        if(index<0||index>=kNodes)return;
        c.hz=std::clamp(c.hz,20.0,std::min(20000.0,sr_*0.45));
        c.q=std::clamp(c.q,0.15,24.0);
        c.gainDb=0.0; // historical node gain retired: handles are Hz only
        c.space=std::clamp(c.space,0.0,100.0);
        c.thresholdDb=std::clamp(c.thresholdDb,-60.0,0.0);
        auto& n=nodes_[(size_t)index];
        const bool changed=n.config.hz!=c.hz || n.config.q!=c.q;
        n.config=c;
        if(c.comp!=Compressor::off)n.profile=c.comp;
        n.attack=std::exp(-1.0/((n.profile==Compressor::fet4?0.0008:n.profile==Compressor::tube3?0.012:0.010)*sr_));
        n.release=std::exp(-1.0/((n.profile==Compressor::fet4?0.14:n.profile==Compressor::tube3?0.32:0.65)*sr_));
        if(changed){updateFilters(index);updateSoloBand(index/2);}
    }
    Config get(int index) const noexcept {return nodes_[(size_t)std::clamp(index,0,kNodes-1)].config;}
    void setSpace(double value) noexcept {space_=std::clamp(value,0.0,100.0);}
    // Exactly seven parent-root gains. Frequency handles never own gain.
    void setRootGain(int root,double gainDb) noexcept {
        if(root>=0&&root<kTerritories)
            rootGainDb_[(size_t)root]=std::clamp(gainDb,-60.0,60.0);
    }
    double rootGain(int root) const noexcept {
        return root>=0&&root<kTerritories?rootGainDb_[(size_t)root]:0.0;
    }
    void setMono(bool mono) noexcept {mono_=mono;} // legacy only; UI uses setRootMono()
    void setRootMono(int root,bool enabled) noexcept {
        if(root>=0&&root<kTerritories)rootMono_[(size_t)root]=enabled;
    }
    // SOLO authority belongs to one ROOT, never to an individual handle.
    void setSoloRoot(int root) noexcept {
        soloRoot_=root>=0&&root<kTerritories?root:-1;
    }
    int soloRoot() const noexcept {return soloRoot_;}
    Meter meter() const noexcept {return meter_;}
    int soloTransitionSamplesRemaining() const noexcept {return soloFadeLeft_;}
    void process(float* l,float* r,std::size_t count) noexcept {
        if(!l||!r||count==0)return;
        const int requestedSolo=soloRoot_;
        if(requestedSolo!=previousRequest_) {
            // Two soloed roots crossfade over 8 ms to prevent hard
            // discontinuities during rapid A/B auditioning.
            previousSolo_=(requestedSolo>=0&&currentSolo_>=0&&soloWet_>0)?currentSolo_:-1;
            if(requestedSolo>=0)currentSolo_=requestedSolo;
            previousRequest_=requestedSolo;
            soloFadeSamples_=previousSolo_>=0
                ?std::max(1,(int)std::lround(sr_*0.008)):0;
            soloFadeLeft_=soloFadeSamples_;
        }
        const bool solo=requestedSolo>=0;
        double sumL=0.0,sumR=0.0,sumCross=0.0,sumM=0.0,sumS=0.0;
        double greatestGr=0.0;
        std::array<double,kTerritories> blockRootPeak{};
        const double globalWidth=mono_?0.0:spaceToWidth(space_);
        std::array<double,kTerritories> gainTargets{};
        for(int root=0;root<kTerritories;++root)
            gainTargets[(size_t)root]=std::pow(10.0,rootGainDb_[(size_t)root]/20.0)-1.0;
        for(std::size_t i=0;i<count;++i) {
            // Sample-rate smoothing applies even during SOLO, so the ROOT
            // gain knob always affects the selected frequency territory.
            for(int root=0;root<kTerritories;++root){
                auto& smooth=smoothedRootDelta_[(size_t)root];
                const double target=gainTargets[(size_t)root];
                smooth=gainSmoothingAlpha_*smooth+(1.0-gainSmoothingAlpha_)*target;
                if(std::abs(target)<1.0e-12 && std::abs(smooth)<1.0e-8)
                    smooth=0.0;
            }
            const double dryM=((double)l[i]+(double)r[i])*0.5;
            const double dryS=((double)l[i]-(double)r[i])*0.5;
            double m=dryM,s=dryS;
            // One DSP state per ROOT; each pair of handles is ONLY the
            // boundary definition. One stereo/dynamics profile per ROOT.
            std::array<double,kTerritories> rootM{},rootS{},processedM{},processedS{};
            for(int root=0;root<kTerritories;++root) {
                auto& node=nodes_[(size_t)(root*2)];
                auto& tap=rootFxBands_[(size_t)root].filters;
                // Keep both components measurable for phase-safe SPACE,
                // even if compression was restricted to MID or SIDE.
                const double bandMid=tap[1].sample(tap[0].sample(dryM));
                const double bandSide=tap[3].sample(tap[2].sample(dryS));
                const double ea=1.0-correlationAlpha_;
                node.midEnergy+=ea*(bandMid*bandMid-node.midEnergy);
                node.sideEnergy+=ea*(bandSide*bandSide-node.sideEnergy);
                rootM[(size_t)root]=bandMid;rootS[(size_t)root]=bandSide;
                double fm=bandMid,fs=bandSide;
                applyRootEffects(root,bandMid,bandSide,fm,fs,greatestGr);
                processedM[(size_t)root]=fm;processedS[(size_t)root]=fs;
                blockRootPeak[(size_t)root]=std::max(blockRootPeak[(size_t)root],
                    std::max(std::abs(fm+fs),std::abs(fm-fs)));
                m+=fm-bandMid;s+=fs-bandSide;
            }
            {
                // One gain per ROOT, applied to the *whole* interval.
                // Shared HP/LP coefficients with the SOLO path guarantee the
                // coloured territory and affected signal agree in frequency.
                // This is parallel band gain with natural filter skirts,
                // not mathematically flat brick-wall frequency selection.
                for(int root=0;root<kTerritories;++root) {
                    const double db=rootGainDb_[(size_t)root];

                    if(std::abs(db)<0.00001&&std::abs(smoothedRootDelta_[(size_t)root])<1.0e-8)continue;
                    const double delta=smoothedRootDelta_[(size_t)root]*nodes_[(size_t)(root*2)].enableMix;
                    if(delta==0.0)continue;
                    const double bm=rootM[(size_t)root],bs=rootS[(size_t)root];
                    const auto domain=nodes_[(size_t)(root*2)].config.domain;
                    if(domain!=Domain::side)m+=delta*bm;
                    if(domain!=Domain::mid)s+=delta*bs*(1.0-nodes_[(size_t)(root*2)].monoMix);
                }
            }
            const double soloStep=1.0/(0.008*sr_);
            soloWet_=solo?std::min(1.0,soloWet_+soloStep):std::max(0.0,soloWet_-soloStep);
            if(soloWet_>0.0&&currentSolo_>=0) {
                const double normalM=m,normalS=s;
                auto audition=[&](int root)->std::array<double,2> {
                    const auto& node=nodes_[(size_t)(root*2)];
                    const double delta=smoothedRootDelta_[(size_t)root]*node.enableMix;
                    const double bm=rootM[(size_t)root],bs=rootS[(size_t)root];
                    const auto domain=node.config.domain;
                    const double auditionMid=processedM[(size_t)root]+(domain==Domain::side?0.0:delta*bm);
                    const double auditionSide=processedS[(size_t)root]+(domain==Domain::mid?0.0:delta*bs*(1.0-node.monoMix));
                    return {auditionMid,auditionSide};
                };
                const auto next=audition(currentSolo_);
                if(soloFadeLeft_>0&&previousSolo_>=0){
                    const auto previous=audition(previousSolo_);
                    const double amount=1.0-(double)soloFadeLeft_/(double)soloFadeSamples_;
                    m=previous[0]*(1.0-amount)+next[0]*amount;
                    s=previous[1]*(1.0-amount)+next[1]*amount;
                } else {m=next[0];s=next[1];}
                m=normalM+soloWet_*(m-normalM);s=normalS+soloWet_*(s-normalS);
            }
            if(soloFadeLeft_>0){
                --soloFadeLeft_;
                if(soloFadeLeft_==0)previousSolo_=-1;
            }
            globalMidPower_=correlationAlpha_*globalMidPower_+
                            (1.0-correlationAlpha_)*m*m;
            globalSidePower_=correlationAlpha_*globalSidePower_+
                             (1.0-correlationAlpha_)*s*s;
            double allowed=globalWidth;
            if(globalWidth>1.0&&globalSidePower_>1e-15)
                allowed=std::min(globalWidth,
                    std::max(1.0,std::sqrt(0.98*globalMidPower_/globalSidePower_)));
            appliedSpace_=sideAlpha_*appliedSpace_+(1.0-sideAlpha_)*allowed;
            if(mono_ || (globalWidth==0.0 && appliedSpace_<1e-6))
                appliedSpace_=0.0; // finish smoothed collapse at exact mono
            const double outL=m+appliedSpace_*s;
            const double outR=m-appliedSpace_*s;
            l[i]=(float)outL;r[i]=(float)outR;
            sumL+=outL*outL;sumR+=outR*outR;sumCross+=outL*outR;
            sumM+=m*m;sumS+=(appliedSpace_*s)*(appliedSpace_*s);
        }
        meter_.correlation=(sumL>1e-20&&sumR>1e-20)?
           std::clamp(sumCross/std::sqrt(sumL*sumR),-1.0,1.0):1.0;
        meter_.midRms=std::sqrt(sumM/(double)count);
        meter_.sideRms=std::sqrt(sumS/(double)count);
        meter_.spaceApplied=appliedSpace_;
        meter_.largestGrDb=greatestGr;
        for(int root=0;root<kTerritories;++root)
            meter_.rootPeakDb[(size_t)root]=20.0*std::log10(std::max(1.0e-12,blockRootPeak[(size_t)root]));
    }
private:
    using OnyxMemory=surgery::Onyx;
    struct Node {
        Config config{};
        double attack=0.99,release=0.999;
        Filter midEq,sideEq,midTap,sideTap;
        double e1=0.0,e2=0.0,midEnergy=0.0,sideEnergy=0.0;
        double optoEnvelope=0.0,phaseMix=0.0;
        OnyxMemory onyxMid{},onyxSide{};
        std::array<double,5> fx{};
        double enableMix=1,monoMix=0,compMix=0,threshold=0;
        Compressor profile=Compressor::optical3;
    };
    void applyRootEffects(int root, double bandMid, double bandSide,
                          double& m, double& s, double& greatestGr) noexcept {
        auto& node=nodes_[(size_t)(root*2)];
        const auto& c=node.config;
        const double originalM=m,originalS=s;
        const double a=1.0-gainSmoothingAlpha_;
        const std::array<double,5> targets{c.dynamic,c.sparkle,c.shine,c.smooth,c.tape};
        for(size_t i=0;i<5;++i)node.fx[i]+=a*(targets[i]-node.fx[i]);
        node.enableMix+=a*((c.enabled?1.0:0.0)-node.enableMix);
        node.monoMix+=a*((rootMono_[(size_t)root]?1.0:0.0)-node.monoMix);
        node.compMix+=a*((c.comp==Compressor::off?0.0:1.0)-node.compMix);
        node.threshold+=a*(c.thresholdDb-node.threshold);
        const double tapM=c.domain==Domain::side?0.0:bandMid;
        const double tapS=c.domain==Domain::mid?0.0:bandSide;
        if(std::abs(c.space-50.0)>1e-5){
            const double requested=spaceToWidth(c.space);
            double allowed=requested;
            if(requested>1.0&&node.sideEnergy>1e-15)
                allowed=std::min(requested,std::max(1.0,
                   std::sqrt(0.98*node.midEnergy/node.sideEnergy)));
            // Only the SIDE content in this ROOT is changed.
            s+=(allowed-1.0)*bandSide;
        }
        // PHASE: automatic, *conservative* stereo coherence recovery within
        // this ROOT. When band side power exceeds mid power, trim the
        // anti-correlated SIDE component; never flip polarity or invent a
        // phase rotation. Gain transitions use the 10 ms engine smoother.
        node.phaseMix+=a*((c.phase?1.0:0.0)-node.phaseMix);
        const double coherence=(node.midEnergy-node.sideEnergy)/
            (node.midEnergy+node.sideEnergy+1.0e-12);
        const double sideCorrection=std::clamp(-coherence*0.85,0.0,0.85);
        s-=node.phaseMix*sideCorrection*tapS;
        // Process spectral colour on the selected ROOT only, never master.
        {
            const double processedM=onyxSample(tapM,node.onyxMid,node);
            const double processedS=onyxSample(tapS,node.onyxSide,node);
            m+=processedM-tapM;
            s+=processedS-tapS;
        }
        {
            const double ratio=node.profile==Compressor::optical4 ||
                      node.profile==Compressor::fet4?4.0:3.0;
            const double stereoPeak=std::max(std::abs(tapM+tapS),
                              std::abs(tapM-tapS));
            const double level1=c.detector==Detector::mid?std::abs(tapM):
                  c.detector==Detector::side?std::abs(tapS):
                  c.detector==Detector::linked?stereoPeak:std::abs(tapM);
            const double level2=c.detector==Detector::dual?std::abs(tapS):level1;
            node.e1=follow(node.e1,level1,node.attack,node.release);
            node.e2=follow(node.e2,level2,node.attack,node.release);
            if(node.compMix>1e-8) {
            const double g1=compressGain(node.e1,node.threshold,ratio,node.profile);
            const double g2=compressGain(node.e2,node.threshold,ratio,node.profile);
            const double mg=c.detector==Detector::side?1.0:g1;
            const double sg=c.detector==Detector::mid?1.0:
                  c.detector==Detector::dual?g2:g1;
            greatestGr=std::max(greatestGr,-20.0*std::log10(
                std::max(1e-9,std::min(mg,sg)))*node.compMix*node.enableMix);
            m+=node.compMix*tapM*(mg-1.0);
            s+=node.compMix*tapS*(sg-1.0);
            }
        }
        // LIMIT: one-knob, program-dependent optical leveling. This is NOT
        // a brickwall or true-peak limiter. Amount 0 is exact bypass.
        const double incoming=std::max(std::abs(tapM+tapS),std::abs(tapM-tapS));
        const double coeff=incoming>node.optoEnvelope?optoAttackAlpha_:
            (node.optoEnvelope>0.15?optoReleaseDeepAlpha_:optoReleaseAlpha_);
        node.optoEnvelope=coeff*node.optoEnvelope+(1.0-coeff)*incoming;
        if(c.limit>1.0e-7){
            const double amount=std::clamp(c.limit,0.0,1.0);
            const double threshold=-6.0-24.0*amount;
            const double signalDb=20.0*std::log10(std::max(1.0e-12,node.optoEnvelope));
            const double over=std::max(0.0,signalDb-threshold);
            const double reductionDb=amount*0.70*over; // soft ~3.3:1 slope
            const double gain=std::pow(10.0,-reductionDb/20.0);
            m+=(gain-1.0)*tapM;
            s+=(gain-1.0)*tapS;
            greatestGr=std::max(greatestGr,reductionDb*node.enableMix);
        }
        s-=node.monoMix*(bandSide+s-originalS);
        m=originalM+node.enableMix*(m-originalM);
        s=originalS+node.enableMix*(s-originalS);
    }
    double onyxSample(double x,OnyxMemory& state,const Node& node) const noexcept {
        return state.sample(x,node.fx);
    }
    // Each root's two boundary handles determine the exact frequency
    // interval for BOTH the palette and the audible bandpass.
    void updateSoloBand(int root) noexcept {
        const auto a=nodes_[(size_t)(2*root)].config.hz;
        const auto b=nodes_[(size_t)(2*root+1)].config.hz;
        const double low=std::min(a,b),high=std::max(a,b);
        auto& colour=nodes_[(size_t)(2*root)];
        colour.onyxMid.setRootBand(low,high);colour.onyxSide.setRootBand(low,high);
        auto& tap=soloBands_[(size_t)root].filters;
        tap[0].setEdge(sr_,low,true); tap[1].setEdge(sr_,high,false);
        tap[2].setEdge(sr_,low,true); tap[3].setEdge(sr_,high,false);
        auto& gainTap=rootGainBands_[(size_t)root].filters;
        gainTap[0].setEdge(sr_,low,true);gainTap[1].setEdge(sr_,high,false);
        gainTap[2].setEdge(sr_,low,true);gainTap[3].setEdge(sr_,high,false);
        auto& fxTap=rootFxBands_[(size_t)root].filters;
        fxTap[0].setEdge(sr_,low,true);fxTap[1].setEdge(sr_,high,false);
        fxTap[2].setEdge(sr_,low,true);fxTap[3].setEdge(sr_,high,false);
    }
    void updateFilters(int i) noexcept {
        auto& n=nodes_[(size_t)i];
        n.midEq.peak(sr_,n.config);n.sideEq.peak(sr_,n.config);
        n.midTap.tap(sr_,n.config);n.sideTap.tap(sr_,n.config);
    }
    static double spaceToWidth(double space) noexcept {
        return space<=50.0?space/50.0:1.0+(space-50.0)/100.0;
    }
    static double follow(double env,double level,double attack,double release) noexcept {
        const double coefficient=level>env?attack:release;
        return coefficient*env+(1.0-coefficient)*level;
    }
    static double compressGain(double env,double thresholdDb,double ratio,
                               Compressor profile) noexcept {
        const double level=20.0*std::log10(std::max(1e-12,env));
        // Soft knee specifically for opto/tube; FET intentionally firmer.
        const double knee=profile==Compressor::fet4?2.0:8.0;
        const double over=level-thresholdDb;
        const double gainReduction=over<=-knee*0.5?0.0:
            over>=knee*0.5?(1.0-1.0/ratio)*over:
            (1.0-1.0/ratio)*std::pow(over+knee*0.5,2.0)/(2.0*knee);
        return std::pow(10.0,-std::max(0.0,gainReduction)/20.0);
    }
    void measureUnchanged(const float* l,const float* r,std::size_t n) noexcept {
        double a=0,b=0,c=0,mid=0,side=0;
        for(std::size_t i=0;i<n;++i){
            const double L=l[i],R=r[i],m=(L+R)*0.5,s=(L-R)*0.5;
            a+=L*L;b+=R*R;c+=L*R;mid+=m*m;side+=s*s;
        }
        const double count=(double)n;
        meter_.correlation=(a>1e-20&&b>1e-20)?
                           std::clamp(c/std::sqrt(a*b),-1.0,1.0):1.0;
        meter_.midRms=std::sqrt(mid/count);
        meter_.sideRms=std::sqrt(side/count);
        meter_.spaceApplied=1.0;meter_.largestGrDb=0.0;
    }
    std::array<Node,kNodes> nodes_{};
    struct RootBand { std::array<Filter,4> filters; };
    std::array<RootBand,kTerritories> soloBands_{};
    std::array<RootBand,kTerritories> rootGainBands_{};
    std::array<RootBand,kTerritories> rootFxBands_{};
    std::array<double,kTerritories> rootGainDb_{};
    std::array<bool,kTerritories> rootMono_{};
    std::array<double,kTerritories> smoothedRootDelta_{};
    double gainSmoothingAlpha_=0.99;
    double optoAttackAlpha_=0.998,optoReleaseAlpha_=0.9999,optoReleaseDeepAlpha_=0.99998;
    int soloRoot_=-1;
    double sr_=48000.0,correlationAlpha_=0.999,sideAlpha_=0.99;
    double globalMidPower_=0.0,globalSidePower_=0.0,appliedSpace_=1.0,grMax_=0.0;
    double space_=50.0;
    bool mono_=false,haveConfig_=false;
    int currentSolo_=-1,previousSolo_=-1,previousRequest_=-1,soloFadeLeft_=0,soloFadeSamples_=0;
    double soloWet_=0;
    Meter meter_{};
};
} // namespace prism::roots::v2

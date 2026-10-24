#pragma once

// ROOTS experimental C++20 DSP kernel. NOT connected to PRISM v1.2's signal path.
// No allocation, I/O, locks, or UI work in process().
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace prism::roots {
constexpr int kTerritories=7, kNodesPerTerritory=2, kNodeCount=14;
enum class Domain { stereo, mid, side };

struct Node {
    bool enabled=false;
    double frequencyHz=1000.0;
    double gainDb=0.0; // [-60,+60]
    double q=1.0;
    Domain domain=Domain::stereo;
};
struct StereoMeter {
    double correlation=1.0, midRms=0.0, sideRms=0.0, appliedWidth=1.0;
};

class Biquad {
public:
    void setPeak(double sampleRate,const Node& node) noexcept {
        const double rate=std::max(8000.0,sampleRate);
        const double f=std::clamp(node.frequencyHz,20.0,std::min(20000.0,rate*0.45));
        const double q=std::clamp(node.q,0.15,24.0);
        const double gain=std::clamp(node.gainDb,-60.0,60.0);
        const double w0=2.0*std::numbers::pi*f/rate;
        const double alpha=std::sin(w0)/(2.0*q);
        const double a=std::pow(10.0,gain/40.0);
        const double invA0=1.0/(1.0+alpha/a);
        b0_=(1.0+alpha*a)*invA0;
        b1_=(-2.0*std::cos(w0))*invA0;
        b2_=(1.0-alpha*a)*invA0;
        a1_=b1_;
        a2_=(1.0-alpha/a)*invA0;
    }
    double process(double x) noexcept {
        const double y=b0_*x+z1_;
        z1_=b1_*x-a1_*y+z2_;
        z2_=b2_*x-a2_*y;
        return y;
    }
    void reset() noexcept {z1_=z2_=0.0;}
private:
    double b0_=1.0,b1_=0.0,b2_=0.0,a1_=0.0,a2_=0.0,z1_=0.0,z2_=0.0;
};

class Engine {
public:
    static constexpr std::array<double,kNodeCount> defaultFrequencies {
        35.0,55.0,80.0,110.0,155.0,220.0,320.0,
        430.0,680.0,1450.0,2700.0,4700.0,8300.0,13000.0
    };
    explicit Engine(double sampleRate=48000.0) noexcept {
        prepare(sampleRate);
        for(int i=0;i<kNodeCount;++i) {
            nodes_[(size_t)i].frequencyHz=defaultFrequencies[(size_t)i];
            updateNode(i,nodes_[(size_t)i]);
        }
    }
    void prepare(double sampleRate) noexcept {
        sampleRate_=std::max(8000.0,sampleRate);
        meterAlpha_=std::exp(-1.0/(0.025*sampleRate_));
        widthAlpha_=std::exp(-1.0/(0.006*sampleRate_));
        midPower_=sidePower_=0.0;appliedWidth_=1.0;
        for(auto& stage:filters_)
            for(auto& filter:stage) filter.reset();
        for(int i=0;i<kNodeCount;++i) applyCoefficients(i);
    }
    // Setup thread while stopped, not concurrent with process(): future
    // realtime integration must use atomic parameter snapshots/smoothing.
    void updateNode(int index,Node node) noexcept {
        if(index<0||index>=kNodeCount)return;
        node.frequencyHz=std::clamp(node.frequencyHz,20.0,
             std::min(20000.0,sampleRate_*0.45));
        node.q=std::clamp(node.q,0.15,24.0);
        node.gainDb=std::clamp(node.gainDb,-60.0,60.0);
        nodes_[(size_t)index]=node;
        applyCoefficients(index);
    }
    const Node& node(int index)const noexcept {
        return nodes_[(size_t)std::clamp(index,0,kNodeCount-1)];
    }
    // Proposed SPACE mapping: 0 mono, 50 neutral, 100 up to +50% side.
    void setSpace(double value) noexcept {space_=std::clamp(value,0.0,100.0);}
    void setMono(bool enabled) noexcept {mono_=enabled;}
    double space()const noexcept {return space_;}
    void reset() noexcept {
        for(auto& stage:filters_)
            for(auto& filter:stage) filter.reset();
        midPower_=sidePower_=0.0;appliedWidth_=1.0;meters_={};
    }
    // Buffers contain samples valid elements. No algorithmic buffering latency.
    void process(float* left,float* right,std::size_t samples) noexcept {
        if(!left||!right)return;
        double cross=0.0,lEnergy=0.0,rEnergy=0.0,mEnergy=0.0,sEnergy=0.0;
        const double requestedWidth=mono_?0.0:
            space_<=50.0?space_/50.0:1.0+(space_-50.0)/100.0;
        for(std::size_t i=0;i<samples;++i) {
            double m=((double)left[i]+(double)right[i])*0.5;
            double s=((double)left[i]-(double)right[i])*0.5;
            for(int k=0;k<kNodeCount;++k) {
                const auto& n=nodes_[(size_t)k];
                if(!n.enabled||std::abs(n.gainDb)<1e-10)continue;
                if(n.domain!=Domain::side)
                    m=filters_[(size_t)k][0].process(m);
                if(n.domain!=Domain::mid)
                    s=filters_[(size_t)k][1].process(s);
            }
            midPower_=meterAlpha_*midPower_+(1.0-meterAlpha_)*m*m;
            sidePower_=meterAlpha_*sidePower_+(1.0-meterAlpha_)*s*s;
            // Don't worsen correlation via added widening; don't silently
            // invert or delete already-antiphase input.
            double allowed=requestedWidth;
            if(requestedWidth>1.0&&sidePower_>1e-16) {
                const double cap=std::sqrt(0.98*midPower_/sidePower_);
                allowed=std::min(requestedWidth,std::max(1.0,cap));
            }
            appliedWidth_=widthAlpha_*appliedWidth_+
                          (1.0-widthAlpha_)*allowed;
            if(mono_)appliedWidth_=0.0;
            const double l=m+s*appliedWidth_;
            const double r=m-s*appliedWidth_;
            left[i]=(float)l;right[i]=(float)r;
            lEnergy+=l*l;rEnergy+=r*r;cross+=l*r;
            mEnergy+=m*m;sEnergy+=(s*appliedWidth_)*(s*appliedWidth_);
        }
        const double count=(double)std::max<std::size_t>(1,samples);
        meters_.correlation=(lEnergy>1e-18&&rEnergy>1e-18)?
             std::clamp(cross/std::sqrt(lEnergy*rEnergy),-1.0,1.0):1.0;
        meters_.midRms=std::sqrt(mEnergy/count);
        meters_.sideRms=std::sqrt(sEnergy/count);
        meters_.appliedWidth=appliedWidth_;
    }
    StereoMeter meter()const noexcept{return meters_;}
private:
    void applyCoefficients(int index)noexcept {
        for(auto& filter:filters_[(size_t)index])
            filter.setPeak(sampleRate_,nodes_[(size_t)index]);
    }
    double sampleRate_=48000.0,meterAlpha_=0.999,widthAlpha_=0.99;
    double midPower_=0.0,sidePower_=0.0,appliedWidth_=1.0,space_=50.0;
    bool mono_=false;
    std::array<Node,kNodeCount> nodes_{};
    std::array<std::array<Biquad,2>,kNodeCount> filters_{};
    StereoMeter meters_{};
};
} // namespace prism::roots

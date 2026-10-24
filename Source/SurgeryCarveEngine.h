#pragma once
// SURGERY EQ v1.0 / original Spiral Ether Tech surgical CARVE DSP.
// Real-time-safe: fixed storage, no allocations, no host calls in process().
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace surgery {
constexpr int kRoots=7, kSlots=4, kCarves=kRoots*kSlots;
inline float defaultCarveHz(int root,int slot) noexcept {
    static constexpr double low[]{20,60,120,250,500,2000,6000};
    static constexpr double high[]{60,120,250,500,2000,6000,20000};
    root=std::clamp(root,0,6);slot=std::clamp(slot,0,3);
    return (float)std::exp(std::log(low[root])+
        (slot+1.0)/5.0*std::log(high[root]/low[root]));
}
enum class Channel : int { stereo=0, mid=1, side=2 };
struct CarveConfig {
    double hz=1000.0, depthDb=0.0, q=2.0, thresholdDb=-24.0;
    Channel channel=Channel::stereo;
    bool enabled=false, dynamic=false;
};
class Biquad final {
public:
    void notch(double sr,double frequency,double q,double gainDb) noexcept {
        const double hz=std::clamp(frequency,20.0,sr*0.45);
        const double w=2.0*std::numbers::pi*hz/sr;
        const double A=std::pow(10.0,gainDb/40.0);
        const double alpha=std::sin(w)/(2.0*q);
        const double c=std::cos(w),d=1.0/(1.0+alpha/A);
        b0_=(1.0+alpha*A)*d;b1_=-2.0*c*d;b2_=(1.0-alpha*A)*d;
        a1_=-2.0*c*d;a2_=(1.0-alpha/A)*d;
    }
    void bandpass(double sr,double frequency,double q) noexcept {
        const double w=2.0*std::numbers::pi*std::clamp(frequency,20.0,sr*0.45)/sr;
        const double alpha=std::sin(w)/(2.0*q),c=std::cos(w),d=1.0/(1.0+alpha);
        b0_=alpha*d;b1_=0.0;b2_=-b0_;a1_=-2.0*c*d;a2_=(1.0-alpha)*d;
    }
    double sample(double x) noexcept {
        const double y=b0_*x+z1_;
        z1_=b1_*x-a1_*y+z2_;
        z2_=b2_*x-a2_*y;
        return y;
    }
    void reset() noexcept {z1_=z2_=0.0;}
private:
    double b0_=1.0,b1_=0,b2_=0,a1_=0,a2_=0,z1_=0,z2_=0;
};
class CarveEngine final {
public:
    void prepare(double sr) noexcept {
        sr_=std::max(8000.0,sr);
        attack_=std::exp(-1.0/(0.003*sr_));
        release_=std::exp(-1.0/(0.065*sr_));
        blendAlpha_=std::exp(-1.0/(0.004*sr_));
        for(auto& n:nodes_){
            n.env=0.0;n.mix=0.0;n.countdown=0;
            n.hz=n.config.hz;n.q=n.config.q;n.depth=n.config.depthDb;
            for(auto& f:n.eq)f.reset();
            for(auto& f:n.detector)f.reset();
            update(n);
        }
    }
    void bounds(int root,double a,double b) noexcept {
        if(root<0||root>=kRoots)return;
        low_[(size_t)root]=std::clamp(std::min(a,b),20.0,20000.0);
        high_[(size_t)root]=std::clamp(std::max(a,b),20.0,20000.0);
    }
    void rootEnabled(int root,bool enabled) noexcept {
        if(root>=0&&root<kRoots)enabled_[(size_t)root]=enabled;
    }
    void rootDomain(int root,Channel domain) noexcept {
        if(root>=0&&root<kRoots)domain_[(size_t)root]=domain;
    }
    void configure(int root,int slot,CarveConfig c) noexcept {
        if(root<0||root>=kRoots||slot<0||slot>=kSlots)return;
        c.hz=std::clamp(c.hz,20.0,std::min(20000.0,sr_*0.45));
        const double lower=std::clamp(low_[(size_t)root],20.0,std::min(20000.0,sr_*0.45));
        const double upper=std::clamp(high_[(size_t)root],lower,std::min(20000.0,sr_*0.45));
        c.hz=std::clamp(c.hz,lower,upper);
        c.depthDb=std::clamp(c.depthDb,0.0,36.0);
        c.q=std::clamp(c.q,0.3,96.0);
        c.thresholdDb=std::clamp(c.thresholdDb,-60.0,0.0);
        auto& n=nodes_[(size_t)(root*kSlots+slot)];
        const bool changed=n.config.hz!=c.hz||n.config.q!=c.q||n.config.depthDb!=c.depthDb;
        n.config=c;
        if(changed)n.countdown=0;
    }
    CarveConfig get(int root,int slot) const noexcept {
        return nodes_[(size_t)(std::clamp(root,0,kRoots-1)*kSlots+std::clamp(slot,0,kSlots-1))].config;
    }
    static double boundaryWeight(double hz,double low,double high) noexcept {
        // Full influence within interval; smooth tails extend up to one-third octave.
        const double spread=std::log(2.0)/3.0;
        const double x=std::log(std::max(20.0,hz));
        const double l=std::log(std::max(20.0,low)),h=std::log(std::max(low,high));
        const auto smooth=[](double v){v=std::clamp(v,0.0,1.0);return v*v*(3.0-2.0*v);};
        return smooth((x-l+spread)/spread)*smooth((h+spread-x)/spread);
    }
    void process(float* left,float* right,std::size_t samples,int soloRoot=-1) noexcept {
        if(!left||!right)return;
        for(int root=0;root<kRoots;++root){
            const bool rootActive=enabled_[(size_t)root]&&(soloRoot<0||root==soloRoot);
            for(int slot=0;slot<kSlots;++slot){
                auto& n=nodes_[(size_t)(root*kSlots+slot)];
                const auto& c=n.config;
                const Channel channel=domain_[(size_t)root]==Channel::stereo?
                    c.channel:domain_[(size_t)root];
                const bool active=rootActive&&c.enabled&&c.depthDb>=0.001;
                if(!active&&n.mix<1e-8){
                    // Fully dry nodes cannot replay stale ringing or detector state.
                    n.mix=0;n.env=0;
                    for(auto& f:n.eq)f.reset();
                    for(auto& f:n.detector)f.reset();
                    n.hz=c.hz;n.q=c.q;n.depth=c.depthDb;
                    update(n);continue;
                }
                const double weight=boundaryWeight(c.hz,low_[(size_t)root],high_[(size_t)root]);

                for(std::size_t i=0;i<samples;++i){
                    if(n.countdown--<=0){
                        const double alpha=1.0-std::exp(-32.0/(0.008*sr_));
                        n.hz+=alpha*(c.hz-n.hz);n.q+=alpha*(c.q-n.q);
                        n.depth+=alpha*(c.depthDb-n.depth);
                        update(n);n.countdown=31;
                    }
                    const double l=left[i],r=right[i],mid=0.5*(l+r),side=0.5*(l-r);
                    const double dl=channel==Channel::mid?mid:channel==Channel::side?side:l;
                    const double dr=channel==Channel::stereo?r:dl;
                    if(c.dynamic){
                        const double detected=std::max(std::abs(n.detector[0].sample(dl)),
                                                        std::abs(n.detector[1].sample(dr)));
                        const double a=detected>n.env?attack_:release_;
                        n.env=a*n.env+(1.0-a)*detected;
                    }
                    const double levelDb=20.0*std::log10(std::max(1e-12,n.env));
                    const double target=active?weight*(c.dynamic?std::clamp((levelDb-c.thresholdDb)/12.0,0.0,1.0):1.0):0.0;
                    n.mix=blendAlpha_*n.mix+(1.0-blendAlpha_)*target;
                    const double y1=dl+n.mix*(n.eq[0].sample(dl)-dl);
                    const double y2=channel==Channel::stereo?
                        dr+n.mix*(n.eq[1].sample(dr)-dr):0.0;
                    if(channel==Channel::mid){
                        left[i]=(float)(y1+side);right[i]=(float)(y1-side);
                    }else if(channel==Channel::side){
                        left[i]=(float)(mid+y1);right[i]=(float)(mid-y1);
                    }else{
                        left[i]=(float)y1;right[i]=(float)y2;
                    }
                }
            }
        }
    }
private:
    struct Node {
        CarveConfig config{};
        std::array<Biquad,2> eq,detector;
        double env=0,mix=0,hz=1000,q=2,depth=0;
        int countdown=0;
    };
    void update(Node& n) noexcept {
        for(auto& f:n.eq)f.notch(sr_,n.hz,n.q,-n.depth);
        for(auto& f:n.detector)f.bandpass(sr_,n.hz,n.q);
    }
    std::array<Node,kCarves> nodes_{};
    std::array<double,kRoots> low_{20,60,120,250,500,2000,6000};
    std::array<double,kRoots> high_{60,120,250,500,2000,6000,20000};
    std::array<bool,kRoots> enabled_{true,true,true,true,true,true,true};
    std::array<Channel,kRoots> domain_{};
    double sr_=48000,attack_=0.99,release_=0.999,blendAlpha_=0.99;
};
} // namespace surgery

#pragma once
// SONAR 2.1 — calibrated analysis only. Never called from the audio callback.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
namespace surgery::weather {
constexpr int fftLength=4096,bins=fftLength/2+1,features=256,roots=7;
inline float unit(double x) noexcept {return (float)std::clamp(std::isfinite(x)?x:0.,0.,1.);}
inline float dbPower(double p) noexcept {return (float)(10*std::log10(std::max(1.e-16,std::isfinite(p)?p:0.)));}
inline float intensity(double p) noexcept {return unit((dbPower(p)+72.)/66.);}
struct Snapshot {
    std::array<float,7> power{},rootIntensity{};
    std::array<float,14> boundaries{};
    double seconds=0;
    std::uint64_t sequence=0,event=0;
    float fullPower=0,analysisPower=0,unassignedPower=0,centroidHz=0,centroid=0,bandwidthHz=20000;
    float amplitudeDb=-160,width=0,correlation=0,bass=0,flux=0,density=0,level=0;
    std::array<float,4> musicEnergy{}; // fixed 20-160 / 160-500 / 500-2500 / 2500-20k bands
    float rms=0,crestDb=0,transient=0;
    float eventStrength=0,eventPosition=.5f,fluxPosition=.5f;
    double eventSeconds=-100;
};
// Single analysis-worker producer / single UI consumer. Ownership transfers
// through release/acquire indices; neither side can overwrite an owned slot.
// A closed editor drops analysis snapshots, never blocks audio or the worker.
template<class T,int Capacity=8> class SnapshotQueue {
public:
    bool push(const T& frame) noexcept {
        const auto w=write_.load(std::memory_order_relaxed),next=(w+1)%Capacity;
        if(next==read_.load(std::memory_order_acquire))return false;
        frames_[w]=frame;write_.store(next,std::memory_order_release);return true;
    }
    bool latest(T& frame) noexcept {
        bool found=false;
        for(int n=0;n<Capacity;++n){
            const auto r=read_.load(std::memory_order_relaxed);
            if(r==write_.load(std::memory_order_acquire))break;
            frame=frames_[r];read_.store((r+1)%Capacity,std::memory_order_release);found=true;
        }
        return found;
    }
private:
    std::array<T,Capacity> frames_{};
    alignas(64) std::atomic<unsigned> write_{0};
    alignas(64) std::atomic<unsigned> read_{0};
};
static_assert(std::atomic<unsigned>::is_always_lock_free);
struct StereoSums {double ll=0,rr=0,lr=0,mid=0,side=0,peak=0;int count=0;};
inline StereoSums stereoSums(const float* left,const float* right,int n) noexcept {
    StereoSums s;s.count=n;
    for(int i=0;i<n;++i){const double l=std::isfinite(left[i])?left[i]:0,r=std::isfinite(right[i])?right[i]:0;
        s.peak=std::max({s.peak,std::abs(l),std::abs(r)});s.ll+=l*l;s.rr+=r*r;s.lr+=l*r;s.mid+=(l+r)*(l+r)*.25;s.side+=(l-r)*(l-r)*.25;
    }
    return s;
}
class Measurement final {
public:
    void reset(double rate,double coherentWindowSum,double windowEnergy) noexcept {
        rate_=std::clamp(rate,1000.,768000.);windowSum_=std::max(1.,coherentWindowSum);windowEnergy_=std::max(1.,windowEnergy);
        previous_.fill(0);history_.fill(0);historyCount_=historyIndex_=0;lastEvent_=-100;event_=sequence_=0;weightsValid_=false;
    }
    // Shared bins are subdivided at every actual ROOT edge. Each frequency
    // segment is split equally among covering territories, so sum weights<=1
    // even for overlapping/reversed ROOT intervals. Gaps stay unassigned.
    void boundaries(const std::array<float,14>& hz) noexcept {
        auto clean=hz;for(auto& f:clean)f=(float)std::clamp(std::isfinite(f)?(double)f:20.,20.,std::min(20000.,rate_*.5));
        if(weightsValid_&&clean==lastBounds_)return;
        lastBounds_=clean;weightsValid_=true;const double high=std::min(20000.,rate_*.5),df=rate_/fftLength;
        std::array<double,16> edges{};edges[0]=20;edges[1]=high;
        for(int i=0;i<14;++i)edges[(size_t)i+2]=std::clamp((double)clean[(size_t)i],20.,high);
        std::sort(edges.begin(),edges.end());
        for(int k=0;k<bins;++k){
            auto& weights=rootWeights_[(size_t)k];weights.fill(0);
            const double lo=std::max(0.,(k-.5)*df),hi=std::min(rate_*.5,(k+.5)*df),width=hi-lo;
            analysisWeights_[(size_t)k]=(float)(std::max(0.,std::min(hi,high)-std::max(lo,20.))/width);
            for(int e=1;e<16;++e){
                const double a=std::max(lo,edges[(size_t)e-1]),b=std::min(hi,edges[(size_t)e]);if(b<=a)continue;
                const double centre=(a+b)*.5;std::array<bool,7> covered{};int count=0;
                for(int r=0;r<7;++r){double x=edgesForRoot(r,0),y=edgesForRoot(r,1);covered[(size_t)r]=centre>=std::min(x,y)&&centre<=std::max(x,y);count+=covered[(size_t)r]?1:0;}
                if(count)for(int r=0;r<7;++r)if(covered[(size_t)r])weights[(size_t)r]+=(float)((b-a)/(width*count));
            }
        }
        std::array<double,257> frequencyEdges{};
        for(int i=0;i<=256;++i)frequencyEdges[(size_t)i]=20*std::pow(high/20.,i/256.);
        partCount_=0;int group=0;
        for(int k=0;k<bins;++k){
            const double low=std::max(0.,(k-.5)*df),upper=std::min(rate_*.5,(k+.5)*df);
            while(group<255&&frequencyEdges[(size_t)group+1]<=low)++group;
            for(int i=group;i<256&&frequencyEdges[(size_t)i]<upper;++i){
                const double overlap=std::min(upper,frequencyEdges[(size_t)i+1])-std::max(low,frequencyEdges[(size_t)i]);
                if(overlap>0&&partCount_<(int)parts_.size())parts_[(size_t)partCount_++]={(std::uint16_t)k,(std::uint16_t)i,(float)(overlap/(upper-low))};
            }
        }
    }
    Snapshot process(const float* magnitudes,const StereoSums& stereo,double seconds,double dt) noexcept {
        seconds=std::isfinite(seconds)?std::max(0.,seconds):0.;dt=std::isfinite(dt)?std::clamp(dt,.0005,.2):.008;
        Snapshot out;out.seconds=seconds;out.sequence=++sequence_;
        std::array<double,256> bandPower{};std::array<double,bins> powers{};double full=0,total=0,moment=0,amplitude=0;
        const double df=rate_/fftLength,high=std::min(20000.,rate_*.5);
        out.bandwidthHz=(float)high;
        for(int i=0;i<14;++i)out.boundaries[(size_t)i]=(float)std::clamp((double)lastBounds_[(size_t)i],20.,high);
        for(int k=0;k<bins;++k){
            const double magnitude=std::isfinite(magnitudes[k])?std::clamp((double)magnitudes[k],0.,1.e12):0.;
            const double sided=(k==0||k==bins-1)?1.:2.;
            // Parseval / periodogram band power, NOT coherent-tone amplitude.
            const double rawPower=sided*magnitude*magnitude/(fftLength*windowEnergy_);
            const double power=rawPower<1.e-24?0:rawPower; // below reporting floor; avoid subnormal output
            powers[(size_t)k]=power;full+=power;const double energy=power*analysisWeights_[(size_t)k];total+=energy;moment+=energy*(k*df);
            amplitude=std::max(amplitude,sided*magnitude/windowSum_);
            for(int r=0;r<7;++r)out.power[(size_t)r]+=(float)(power*rootWeights_[(size_t)k][(size_t)r]);
        }
        for(int i=0;i<partCount_;++i){const auto& part=parts_[(size_t)i];bandPower[part.band]+=powers[part.bin]*part.weight;}
        // Read-only synesthesia features from these same completed bins.
        constexpr std::array<double,5> musicEdges{20,160,500,2500,20000};
        std::array<double,4> musicPower{};
        for(int k=0;k<bins;++k){const double lo=std::max(0.,(k-.5)*df),hi=std::min(rate_*.5,(k+.5)*df);
            for(int b=0;b<4;++b)musicPower[(size_t)b]+=powers[(size_t)k]*std::max(0.,std::min(hi,std::min(high,musicEdges[(size_t)b+1]))-std::max(lo,musicEdges[(size_t)b]))/std::max(1.e-12,hi-lo);
        }
        for(int b=0;b<4;++b)out.musicEnergy[(size_t)b]=intensity(musicPower[(size_t)b]);
        out.rms=(float)std::sqrt(std::max(0.,(stereo.ll+stereo.rr)/(2*std::max(1,stereo.count))));
        out.crestDb=out.rms>1.e-8f?(float)std::clamp(20*std::log10(std::max(1.,stereo.peak/out.rms)),0.,60.):0;
        out.transient=unit((out.crestDb-3)/18.);
        out.fullPower=(float)std::min(full,1.e30);out.analysisPower=(float)std::min(total,1.e30);
        out.amplitudeDb=(float)(20*std::log10(std::max(1.e-8,amplitude)));
        double assigned=0;for(int r=0;r<7;++r){assigned+=out.power[(size_t)r];out.rootIntensity[(size_t)r]=intensity(out.power[(size_t)r]);}
        out.unassignedPower=(float)std::max(0.,total-assigned);out.level=intensity(total);
        if(total>1.e-12){out.centroidHz=(float)(moment/total);out.centroid=unit(std::log(std::max(20.,moment/total)/20.)/std::log(high/20.));}
        const double mid=std::sqrt(stereo.mid/std::max(1,stereo.count)),side=std::sqrt(stereo.side/std::max(1,stereo.count));
        out.width=unit(side/(mid+side+1.e-12));
        out.correlation=(float)std::clamp(stereo.lr/(std::sqrt(stereo.ll*stereo.rr)+1.e-18),-1.,1.);
        out.bass=.45f*out.rootIntensity[0]+.35f*out.rootIntensity[1]+.20f*out.rootIntensity[2];
        double flux=0,position=0;int occupied=0;
        for(int i=0;i<256;++i){
            const float m=(float)std::sqrt(bandPower[(size_t)i]/std::max(.25,total));
            const double delta=std::max(0.f,m-previous_[(size_t)i]);flux+=delta*delta;position+=delta*delta*(i+.5)/256.;previous_[(size_t)i]=m;
            occupied+=bandPower[(size_t)i]>std::max(1.e-12,total*.002)?1:0;
        }
        out.density=occupied/256.f;const double eventPosition=flux>0?position/flux:.5;out.fluxPosition=(float)eventPosition;
        flux*=.008/std::clamp(dt,.0005,.2); // comparable reference hop duration
        double mean=0,variance=0;for(int i=0;i<historyCount_;++i)mean+=history_[(size_t)i];mean/=std::max(1,historyCount_);
        for(int i=0;i<historyCount_;++i)variance+=(history_[(size_t)i]-mean)*(history_[(size_t)i]-mean);variance/=std::max(1,historyCount_);
        const double threshold=std::max(.0005,mean+4*std::sqrt(variance));
        out.flux=unit(flux/std::max(.001,threshold*2));
        if(total>1.e-9&&flux>threshold&&seconds-lastEvent_>=.75){lastEvent_=seconds;++event_;lastStrength_=unit(flux/(threshold*3));lastPosition_=(float)eventPosition;}
        // Winsorised rolling baseline: exceptional onsets cannot dominate it.
        const double bounded=std::min(flux,std::max(.001,mean+3*std::sqrt(variance)));
        history_[(size_t)historyIndex_]=bounded<1.e-12?0:(float)bounded;historyIndex_=(historyIndex_+1)%64;historyCount_=std::min(64,historyCount_+1);
        out.event=event_;out.eventSeconds=lastEvent_;out.eventStrength=lastStrength_;out.eventPosition=lastPosition_;
        return out;
    }
private:
    double edgesForRoot(int root,int edge) const noexcept {return std::clamp((double)lastBounds_[(size_t)(root*2+edge)],20.,std::min(20000.,rate_*.5));}
    double rate_=48000,windowSum_=4096,windowEnergy_=6144,lastEvent_=-100;
    std::uint64_t event_=0,sequence_=0;float lastStrength_=0,lastPosition_=.5f;
    struct Part{std::uint16_t bin=0,band=0;float weight=0;};
    std::array<Part,bins+features+4> parts_{};int partCount_=0;
    std::array<float,14> lastBounds_{};bool weightsValid_=false;
    std::array<std::array<float,7>,bins> rootWeights_{};
    std::array<float,bins> analysisWeights_{};
    std::array<float,256> previous_{};
    std::array<float,64> history_{};int historyCount_=0,historyIndex_=0;
};
} // namespace surgery::weather

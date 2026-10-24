#pragma once
#include "SurgeryWeatherMeasurement.h"
namespace surgery::weather {
struct Meaning {float bass=0,centroid=0,flux=0,width=0,rate=0,density=0,level=0,storm=0,flash=0,flashPosition=.5f;std::array<float,4> musicEnergy{};float rms=0,crest=0;};
inline const char* mood(const Meaning& f) noexcept {
    if(f.level<.03f)return "CALM";
    const std::array<float,5> scores{
        .65f*f.bass+.20f*(1-f.flux)+.15f*f.density,
        .45f*f.flux+.35f*f.rate+.20f*f.density,
        .50f*f.width+.25f*(1-f.flux)+.25f*(1-f.density),
        .50f*f.centroid+.35f*f.flux+.15f*f.rate,
        .50f*(1-f.level)+.30f*(1-f.flux)+.20f*(1-f.rate)};
    const char* names[]{"BROODING","TURBULENT","FLOATING","ELECTRIC","CALM"};
    return names[(size_t)std::distance(scores.begin(),std::max_element(scores.begin(),scores.end()))];
}
class Interpretation final {
public:
    void accept(const Snapshot& input) noexcept {
        input_=input;age_=0;received_=true;
        if(input.event!=event_){
            event_=input.event;
            if(input.seconds-input.eventSeconds>=0&&input.seconds-input.eventSeconds<.25){
                eventTimes_[(size_t)eventIndex_]=time_;eventIndex_=(eventIndex_+1)%8;
                if(time_-lastPulse_>=.75){pulse_=unit(input.eventStrength);pulsePosition_=unit(input.eventPosition);lastPulse_=time_;}
            }
        }
    }
    const Meaning& update(double dt,double memory,bool reduced) noexcept {
        dt=std::clamp(std::isfinite(dt)?dt:0.,0.,60.);time_+=dt;age_=received_?0:age_+dt;received_=false;
        const bool fresh=age_<.25&&input_.analysisPower>1.e-12f;
        const float release=(float)(.15+.65*memoryUnit(memory));
        auto smooth=[&](float& shown,float target){const double tau=target>shown?.025:release;shown=unit(target+(shown-target)*std::exp(-dt/tau));if(shown<1.e-8f)shown=0;};
        const bool hasMusicBands=std::any_of(input_.musicEnergy.begin(),input_.musicEnergy.end(),[](float e){return e>0;});
        smooth(f_.bass,fresh?(hasMusicBands?input_.musicEnergy[0]:input_.bass):0);smooth(f_.centroid,fresh?input_.centroid:0);
        smooth(f_.flux,fresh?input_.flux:0);smooth(f_.width,fresh?input_.width:0);
        for(int b=0;b<4;++b)smooth(f_.musicEnergy[(size_t)b],fresh?input_.musicEnergy[(size_t)b]:0);
        smooth(f_.rms,fresh?unit(input_.rms):0);smooth(f_.crest,fresh?input_.transient:0);
        smooth(f_.density,fresh?input_.density:0);smooth(f_.level,fresh?input_.level:0);
        int count=0;for(auto t:eventTimes_)count+=time_-t<4&&t>=0?1:0;smooth(f_.rate,fresh?unit(count/(4./.75)):0);
        for(int i=0;i<7;++i)smooth(root_[(size_t)i],fresh?input_.rootIntensity[(size_t)i]:0);
        if(reduced)pulse_=0;
        pulse_*= (float)std::exp(-dt/.11);if(pulse_<1.e-8f)pulse_=0;f_.flash=reduced?0:std::min(.10f,pulse_*.10f);f_.flashPosition=pulsePosition_;
        f_.storm=unit(.35*f_.bass+.30*f_.flux+.20*f_.rate+.15*f_.density);
        return f_;
    }
    void quiet() noexcept {input_={};f_={};root_.fill(0);age_=100;pulse_=0;received_=false;}
    const Meaning& state() const noexcept {return f_;}
    const Snapshot& measured() const noexcept {return input_;}
    const std::array<float,7>& rootIntensity() const noexcept {return root_;}
    static double memoryUnit(double seconds) noexcept {if(!std::isfinite(seconds))seconds=.18;return std::clamp(std::log(std::clamp(seconds,.05,10.)/.05)/std::log(200.),0.,1.);}
private:
    bool received_=false;
    Snapshot input_{};Meaning f_{};std::array<float,7> root_{};
    std::array<double,8> eventTimes_{-100,-100,-100,-100,-100,-100,-100,-100};
    int eventIndex_=0;std::uint64_t event_=0;double age_=100,time_=0,lastPulse_=-100;float pulse_=0,pulsePosition_=.5f;
};
} // namespace surgery::weather

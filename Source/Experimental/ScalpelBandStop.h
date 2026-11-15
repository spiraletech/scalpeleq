#pragma once
// Experimental, NOT connected to the shipping processor. Evaluate before use.
// 12-pole Butterworth lowpass prototype -> 24-pole digital bandstop.
// Bilinear transform with independently prewarped edges. -3.01 dB at edges;
// rejection applies to an interior, never to the complete closed interval.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>
namespace scalpel::experimental {
class BandStop {
public:
    static constexpr int sections=12;
    bool configure(double sr,double low,double high) noexcept {
        if(!std::isfinite(sr)||!std::isfinite(low)||!std::isfinite(high)||
           sr<44100||low<20||high>std::min(20000.,sr*.45)||high<=low)return false;
        const double wl=2*sr*std::tan(std::numbers::pi*low/sr);
        const double wh=2*sr*std::tan(std::numbers::pi*high/sr);
        const double bw=wh-wl,w0=std::sqrt(wl*wh);
        const double zeroAngle=2*std::atan(w0/(2*sr));
        const double b1=-2*std::cos(zeroAngle);
        std::array<Section,sections> next{};int index=0;
        for(int i=0;i<sections;++i){
            const auto pole=std::polar(1.,std::numbers::pi*(2*i+1+sections)/(2*sections));
            const auto half=bw/(2.*pole);
            const auto delta=std::sqrt(half*half-std::complex<double>{w0*w0,0});
            for(auto analog:{half+delta,half-delta}){
                if(analog.imag()<=0)continue;
                if(index>=sections)return false;
                const auto digital=(2*sr+analog)/(2*sr-analog);
                if(std::abs(digital)>=1)return false;
                auto& s=next[(size_t)index++];
                s.a1=-2*digital.real();s.a2=std::norm(digital);
                const double gain=(1+s.a1+s.a2)/(2+b1);
                s.b0=gain;s.b1=b1*gain;s.b2=gain;
                if(!std::isfinite(gain)||gain<=0)return false;
            }
        }
        if(index!=sections)return false;
        // Deliberately retains delays during coefficient changes. This is a
        // measurable experimental limitation, not a production drag smoother.
        for(int i=0;i<sections;++i){next[i].z1=filters_[i].z1;next[i].z2=filters_[i].z2;}
        filters_=next;return true;
    }
    double sample(double x) noexcept {
        for(auto& s:filters_){const double y=s.b0*x+s.z1;s.z1=s.b1*x-s.a1*y+s.z2;s.z2=s.b2*x-s.a2*y;x=y;}return x;
    }
    void reset() noexcept {for(auto& s:filters_)s.z1=s.z2=0;}
    std::complex<double> response(double sr,double hz) const noexcept {
        const auto z=std::polar(1.,-2*std::numbers::pi*hz/sr),zz=z*z;
        std::complex<double> h=1;
        for(const auto& s:filters_)h*=(s.b0+s.b1*z+s.b2*zz)/(1.+s.a1*z+s.a2*zz);
        return h;
    }
private:
    struct Section {double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;};
    std::array<Section,sections> filters_{};
};
}

namespace scalpel::experimental {
// Follow-up experiment: freeze each active bank's coefficients and fade to a
// reset replacement over 20 ms. Changes during the fade are coalesced. This
// avoids mutating recursive delay states under new coefficients, at the cost
// of target lag, doubled DSP and temporary mixed frequency responses.
class CrossfadedBandStop {
public:
    bool configure(double sr,double low,double high) noexcept {
        BandStop next;
        if(!next.configure(sr,low,high))return false;
        if(!prepared_){banks_[0]=next;prepared_=true;sr_=sr;return true;}
        if(sr!=sr_)return false; // prepare a new object for sample-rate changes
        pending_=next;havePending_=true;
        if(!fading_)beginFade();
        return true;
    }
    double sample(double x) noexcept {
        if(!prepared_)return x;
        if(!fading_&&havePending_)beginFade();
        const double a=banks_[current_].sample(x);
        if(!fading_)return a;
        const double b=banks_[1-current_].sample(x);
        const double mix=.5-.5*std::cos(std::numbers::pi*(double)position_/length_);
        const double y=a+mix*(b-a);
        if(++position_>length_){current_=1-current_;fading_=false;}
        return y;
    }
private:
    void beginFade() noexcept {
        banks_[1-current_]=pending_;havePending_=false;
        fading_=true;position_=0;length_=std::max(1,(int)std::lround(.020*sr_));
    }
    std::array<BandStop,2> banks_{};BandStop pending_{};
    double sr_=48000;int current_=0,position_=0,length_=1;
    bool prepared_=false,fading_=false,havePending_=false;
};
}

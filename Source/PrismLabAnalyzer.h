#pragma once
// Ported from PRISM Analysis' 256-bar spectral field and hover-dB concept.
// Re-engineered calibration for JUCE FFT magnitude and Hann coherent gain.
// Analyzer telemetry only: NEVER feeds frequency-domain data to audio DSP.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace prism::analysis {
class LabBands final {
public:
    static constexpr int count=256;
    static constexpr float floorDb=-144.0f;
    static constexpr float minHz=20.0f;
    static constexpr float maxHz=20000.0f;
    struct Bar { float db=floorDb, peakDb=floorDb, binHz=0.0f; };
    using Frame=std::array<Bar,count>;

    static float frequencyAt(float unit) noexcept {
        return minHz*std::pow(maxHz/minHz,std::clamp(unit,0.0f,1.0f));
    }
    static int indexForFrequency(float hz) noexcept {
        if (!(hz>minHz)) return 0;
        const float unit=std::log(hz/minHz)/std::log(maxHz/minHz);
        return std::clamp(static_cast<int>(unit*count),0,count-1);
    }
    static float centreFrequency(int i) noexcept {
        return frequencyAt((std::clamp(i,0,count-1)+0.5f)/count);
    }
    void reset() noexcept {
        for(auto& v:frame_)v={};
        initialized_=false;
    }
    // JUCE performFrequencyOnlyForwardTransform supplies nonnegative FFT
    // bin magnitudes. fftSize is the time-domain window size, not 2*N.
    // Symmetric Hann coherent gain: sum of its samples = (N-1)/2.
    // 2*|FFT|/sum(window) calibrates a bin-centred sine's peak amplitude.
    // Bars too narrow for an FFT bin use the nearest bin, so their values
    // represent band-peaks LIMITED by actual FFT resolution (sr / N).
    // Supply windowSum=N for JUCE's normalized Hann; default retains the
    // unnormalized symmetric-Hann contract used by standalone callers.
    void processMagnitudes(const float* mags,int binCount,int fftSize,double sampleRate,
                           double windowSum=0.0) noexcept {
        if(!mags || binCount<=1 || fftSize<4 || sampleRate<=0.0)return;
        const int last=std::min(binCount-1,fftSize/2);
        const double gain=windowSum>0.0?2.0/windowSum:4.0/static_cast<double>(fftSize-1);
        for(int i=0;i<count;++i) {
            const double lo=frequencyAt(static_cast<float>(i)/count);
            const double hi=frequencyAt(static_cast<float>(i+1)/count);
            const double mid=std::sqrt(lo*hi);
            int first=std::max(1,static_cast<int>(std::ceil(lo*fftSize/sampleRate)));
            int end=std::min(last,static_cast<int>(std::floor(hi*fftSize/sampleRate)));
            if(first>end)first=end=std::clamp(static_cast<int>(std::lround(mid*fftSize/sampleRate)),1,last);
            float largest=0.0f;
            int strongest=first;
            for(int k=first;k<=end;++k) {
                const float a=std::isfinite(mags[k])?std::max(0.0f,mags[k]):0.0f;
                if(a>largest){largest=a;strongest=k;}
            }
            const double amp=std::max(1.0e-12,largest*gain);
            const float db=std::clamp(static_cast<float>(20.0*std::log10(amp)),floorDb,12.0f);
            auto& b=frame_[static_cast<std::size_t>(i)];
            const float alpha=!initialized_?1.0f:(db>b.db?0.68f:0.16f);
            b.db+=alpha*(db-b.db);
            b.peakDb=!initialized_?db:std::max(db,b.peakDb-0.32f);
            b.binHz=static_cast<float>(strongest*sampleRate/fftSize);
        }
        initialized_=true;
    }
    const Frame& frame() const noexcept {return frame_;}
private:
    Frame frame_{};
    bool initialized_=false;
};
} // namespace prism::analysis

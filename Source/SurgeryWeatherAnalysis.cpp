#include "PluginProcessor.h"
// Existing FFT/FIFO stay untouched. This sidecar reads completed magnitudes
// only on the analyzer worker, after the original analyzer publishes its data.
void PRISMVSTAudioProcessor::prepareWeatherAnalysis(){
    std::array<float,fftSize> window;window.fill(1);
    fftWindow.multiplyWithWindowingTable(window.data(),fftSize);
    weatherWindowSum_=weatherWindowEnergy_=0;
    for(const auto w:window){weatherWindowSum_+=w;weatherWindowEnergy_+=(double)w*w;}
    weatherMeasurement_.reset(currentSampleRate,weatherWindowSum_,weatherWindowEnergy_);
    weatherSeconds_=0;weatherWasEnabled_=false;
}
void PRISMVSTAudioProcessor::publishWeatherAnalysis(){
    const double dt=fftHopSize/std::max(1.,currentSampleRate);weatherSeconds_+=dt;
    const bool enabled=weatherConsumers_.load(std::memory_order_relaxed)>0
        &&((weatherFillValue_->load(std::memory_order_relaxed)>=.5f)||jukeModeValue_->load(std::memory_order_relaxed)<4.5f);
    if(!enabled){weatherWasEnabled_=false;return;}
    if(!weatherWasEnabled_){weatherMeasurement_.reset(currentSampleRate,weatherWindowSum_,weatherWindowEnergy_);weatherWasEnabled_=true;}
    std::array<float,14> boundaries{};
    for(int i=0;i<14;++i)boundaries[(size_t)i]=nodeParameters[(size_t)i][0]->load(std::memory_order_relaxed);
    weatherMeasurement_.boundaries(boundaries);
    const auto stereo=surgery::weather::stereoSums(fftFifo.data(),fftRightFifo.data(),fftSize);
    const auto snapshot=weatherMeasurement_.process(fftData.data(),stereo,weatherSeconds_,dt);
    weatherSnapshots_.push(snapshot);
}

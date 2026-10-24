#include <JuceHeader.h>
#include "../Source/SurgeryWeatherMeasurement.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <numeric>
#include <chrono>
bool runWeatherCalibration(){
    using namespace surgery::weather;
    juce::dsp::FFT fft(12);juce::dsp::WindowingFunction<float> hann(fftLength,juce::dsp::WindowingFunction<float>::hann);
    std::array<float,fftLength> window{},left{},right{};window.fill(1);hann.multiplyWithWindowingTable(window.data(),fftLength);
    double sum=0,energy=0;for(auto w:window){sum+=w;energy+=w*w;}
    std::array<float,fftLength*2> data{};std::array<float,bins> lm{},combined{};
    const std::array<float,14> edges{20,60,60,120,120,250,250,500,500,2000,2000,6000,6000,20000};
    std::ofstream csv("surgery-weather-calibration.csv");csv<<"rate,signal,amplitude_db,power_db,expected_power_db,centroid_hz,width,correlation\n";
    auto fail=[](const char* text){std::cerr<<"WEATHER CALIBRATION FAIL: "<<text<<'\n';return false;};
    Measurement measurement;
    auto frame=[&](double rate,double seconds){
        data.fill(0);for(int i=0;i<fftLength;++i)data[(size_t)i]=left[(size_t)i]*window[(size_t)i];fft.performFrequencyOnlyForwardTransform(data.data());
        std::copy_n(data.begin(),bins,lm.begin());data.fill(0);for(int i=0;i<fftLength;++i)data[(size_t)i]=right[(size_t)i]*window[(size_t)i];fft.performFrequencyOnlyForwardTransform(data.data());
        for(int i=0;i<bins;++i)combined[(size_t)i]=(float)std::sqrt(.5*((double)lm[(size_t)i]*lm[(size_t)i]+(double)data[(size_t)i]*data[(size_t)i]));
        return measurement.process(combined.data(),stereoSums(left.data(),right.data(),fftLength),seconds,384/rate);
    };
    juce::Random random(210106);
    for(double rate:{44100.,48000.,96000.,192000.}){
        measurement.reset(rate,sum,energy);measurement.boundaries(edges);
        const int k=(int)std::round(1000*fftLength/rate);const double hz=k*rate/fftLength;
        for(int scenario=0;scenario<8;++scenario){
            for(int i=0;i<fftLength;++i){
                float value=.25f*(float)std::sin(juce::MathConstants<double>::twoPi*k*i/fftLength);
                if(scenario==1)value=.25f;if(scenario==2)value=(i%2?-.25f:.25f);if(scenario==3)value=0;
                if(scenario==4)value=(random.nextFloat()*2-1)*.2f;
                if(scenario==5){ // bounded pink-shaped low-pass sum, evaluated against independent Parseval reference
                    static double p1=0,p2=0,p3=0;double w=random.nextFloat()*2-1;p1=.99765*p1+w*.099046;p2=.963*p2+w*.296516;p3=.57*p3+w*1.05269;value=(float)((p1+p2+p3+w*.1848)*.035);
                }
                left[(size_t)i]=value;right[(size_t)i]=scenario==6?-value:scenario==7?0:value;
            }
            auto out=frame(rate,scenario*.1);double expected=0;
            for(int i=0;i<fftLength;++i)expected+=.5*(left[(size_t)i]*left[(size_t)i]+right[(size_t)i]*right[(size_t)i])*window[(size_t)i]*window[(size_t)i];expected/=energy;
            if(std::abs(out.fullPower-expected)>std::max(2.e-8,expected*2.e-4))return fail("broadband Parseval power");
            if(scenario<3&&std::abs(out.amplitudeDb-20*std::log10(.25))>.015)return fail("tone/DC/Nyquist amplitude scaling");
            if(scenario==0&&(std::abs(out.rms-.25/std::sqrt(2.))>.0001||std::abs(out.crestDb-3.0103)>.01||out.musicEnergy[2]<.7f||out.musicEnergy[0]>.05f))return fail("shared RMS/crest/music-band calibration");
            if(scenario==0&&(std::abs(out.centroidHz-hz)>rate/fftLength*.1||out.width>.001||out.correlation<.999))return fail("coherent sine centroid/stereo");
            if(scenario==3&&(out.analysisPower!=0||out.level!=0||out.bass!=0||out.density!=0))return fail("silence has activity");
            if(scenario==6&&(out.width<.999||out.correlation>-.999))return fail("antiphase stereo");
            if(scenario==7&&(std::abs(out.width-.5)>.001||std::abs(out.correlation)>.001))return fail("one-sided stereo");
            double assigned=out.unassignedPower;for(auto power:out.power)assigned+=power;
            if(std::abs(assigned-out.analysisPower)>std::max(1.e-8,out.analysisPower*1.e-5))return fail("ROOT conservation");
            csv<<rate<<','<<scenario<<','<<out.amplitudeDb<<','<<dbPower(out.fullPower)<<','<<dbPower(expected)<<','<<out.centroidHz<<','<<out.width<<','<<out.correlation<<'\n';
        }
        // Independent white noise has side/mid RMS parity and near-zero rho.
        for(int i=0;i<fftLength;++i){left[(size_t)i]=(random.nextFloat()*2-1)*.1f;right[(size_t)i]=(random.nextFloat()*2-1)*.1f;}
        auto decorrelated=frame(rate,1);if(std::abs(decorrelated.width-.5)>.035||std::abs(decorrelated.correlation)>.08)return fail("decorrelated stereo");
        std::array<float,14> overlap{};for(int i=0;i<7;++i){overlap[(size_t)i*2]=20000;overlap[(size_t)i*2+1]=20;}
        measurement.boundaries(overlap);auto all=frame(rate,2);double assigned=0;for(auto power:all.power){assigned+=power;if(std::abs(power-all.analysisPower/7)>1.e-6)return fail("overlapping ROOT split");}
        if(std::abs(assigned-all.analysisPower)>1.e-6)return fail("overlapping ROOT double-count");
    }
    measurement.reset(48000,sum,energy);measurement.boundaries(edges);combined.fill(0);
    std::uint64_t firstEvent=0;for(int i=0;i<1200;++i){combined[85]=400;auto out=measurement.process(combined.data(),{},i*.008,.008);if(i==0)firstEvent=out.event;if(i>10&&out.event!=firstEvent)return fail("steady signal triggers false lightning");}
    std::uint64_t last=0;double lastTime=-100;int events=0;
    for(int i=0;i<1200;++i){combined[85]=(i%16<2?1000.f:0.f);auto out=measurement.process(combined.data(),{},10+i*.008,.008);if(out.event!=last){if(out.eventSeconds-lastTime<.749)return fail("flash refractory interval");lastTime=out.eventSeconds;last=out.event;++events;}}
    if(events<2||events>15)return fail("transient sensitivity/rate");
    combined.fill(1.e-30f);auto tiny=measurement.process(combined.data(),{},30,.008);
    if(tiny.fullPower!=0||tiny.analysisPower!=0||tiny.level!=0)return fail("subnormal measurement output");
    combined[10]=std::numeric_limits<float>::quiet_NaN();combined[20]=std::numeric_limits<float>::infinity();
    auto invalid=measurement.process(combined.data(),{},std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity());
    if(!std::isfinite(invalid.amplitudeDb)||!std::isfinite(invalid.flux)||invalid.analysisPower!=0)return fail("nonfinite input containment");
    SnapshotQueue<Snapshot> queue;Snapshot value;for(int i=0;i<7;++i){value.sequence=i;if(!queue.push(value))return fail("queue capacity");}
    if(queue.push(value)||!queue.latest(value)||value.sequence!=6)return fail("queue overflow/latest");
    std::atomic<bool> done=false,bad=false;std::thread producer([&]{for(int i=1;i<30000;++i){Snapshot v;v.sequence=i;v.fullPower=(float)i;queue.push(v);}done=true;});
    while(!done.load()){if(queue.latest(value)&&value.fullPower!=(float)value.sequence)bad=true;}
    while(queue.latest(value))if(value.fullPower!=(float)value.sequence)bad=true;
    producer.join();if(bad)return fail("torn snapshot");
    std::ofstream performance("surgery-weather-measurement-performance.csv");performance<<"sample_rate,mean_ms,estimated_one_core_percent_at_native_hop\n";
    for(double rate:{44100.,48000.,96000.,192000.}){
        measurement.reset(rate,sum,energy);measurement.boundaries(edges);combined.fill(100);
        for(int i=0;i<fftLength;++i){left[(size_t)i]=.1f;right[(size_t)i]=-.1f;}
        const auto start=std::chrono::steady_clock::now();double checksum=0;
        for(int n=0;n<600;++n){const auto sums=stereoSums(left.data(),right.data(),fftLength);const auto out=measurement.process(combined.data(),sums,n*384/rate,384/rate);checksum+=out.centroidHz;}
        const double meanMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/600;
        if(!std::isfinite(checksum))return fail("feature benchmark finite");performance<<rate<<','<<meanMs<<','<<meanMs*(rate/384)/10<<'\n';
    }
    std::cout<<"WEATHER: coherent/DC/Nyquist amplitude, Parseval white/pink/sine power, 4 rates, ROOT energy conservation, M/S/correlation, stable sustained flux, refractory events and bounded SPSC passed; measurement bytes "<<sizeof(Measurement)<<" queue bytes "<<sizeof(queue)<<'\n';
    return true;
}

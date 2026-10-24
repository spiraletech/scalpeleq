#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include <chrono>
#include <fstream>
#include <thread>
#include <vector>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include "SurgeryUI104.h"
#include "SurgeryRecoveryTests.h"
#include "SurgeryChassisTests.h"
#include "SurgeryWeatherUITests.h"

void startAllocationProbe() noexcept;
std::size_t finishAllocationProbe() noexcept;

namespace {
void set(PRISMVSTAudioProcessor& p,const juce::String& id,float value){
    auto* parameter=p.apvts.getParameter(id);
    if(parameter==nullptr)throw std::runtime_error("Missing parameter: "+id.toStdString());
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void tone(juce::AudioBuffer<float>& b,int offset,double rate=48000.0){
    for(int i=0;i<b.getNumSamples();++i){
        const double t=(offset+i)/rate;
        const float x=(float)(0.12*std::sin(t*900*juce::MathConstants<double>::twoPi)
                            +0.035*std::sin(t*6200*juce::MathConstants<double>::twoPi));
        b.setSample(0,i,x);b.setSample(1,i,x*0.85f);
    }
}
bool click(juce::Component& parent,const juce::String& caption){
    for(auto* child:parent.getChildren()){
        if(auto* b=dynamic_cast<juce::TextButton*>(child);b&&b->getButtonText()==caption){
            if(b->onClick)b->onClick();return true;
        }
    }
    return false;
}
}

bool runQualityTests(){
    juce::ScopedJuceInitialiser_GUI gui;
    if(!verifySonarRecovery()||!verifyChassis105()||!verifyWeatherUI())return false;
    juce::MidiBuffer midi;
    {
        auto fresh=std::make_unique<PRISMVSTAudioProcessor>();
        fresh->prepareToPlay(48000,512);
        {
            std::unique_ptr<juce::AudioProcessorEditor> factory(fresh->createEditor());
            auto image=factory->createComponentSnapshot(factory->getLocalBounds());
            juce::FileOutputStream file(juce::File::getCurrentWorkingDirectory().getChildFile("surgery-ui-factory-ROOT.png"));
            file.setPosition(0);file.truncate();
            if(!juce::PNGImageFormat().writeImageToStream(image,file))return false;
        }
        for(int root=0;root<7;++root){
            auto comp=PRISMVSTAudioProcessor::rootsRootControlId(root,"comp");
            auto threshold=PRISMVSTAudioProcessor::rootsRootControlId(root,"threshold");
            if(fresh->apvts.getRawParameterValue(comp)->load()!=0
               ||fresh->apvts.getRawParameterValue(threshold)->load()!=0)return false;
            set(*fresh,threshold,-29.9f);
            if(fresh->apvts.getRawParameterValue(comp)->load()!=0)return false;
        }
        std::unique_ptr<juce::AudioProcessorEditor> ui(fresh->createEditor());
        bool thresholdFound=false;
        for(auto* child:ui->getChildren())if(auto* label=dynamic_cast<juce::Label*>(child)){
            if(label->getText()=="COMP THRESHOLD"){
                for(auto* candidate:ui->getChildren())if(auto* slider=dynamic_cast<juce::Slider*>(candidate)){
                    if(slider->getX()==label->getX()&&slider->isVisible()){
                        thresholdFound=true;
                        if(slider->onDragStart)slider->onDragStart();
                        slider->setValue(-42,juce::sendNotificationSync);
                        if(slider->getDoubleClickReturnValue()!=0)return false;
                    }
                }
            }
            if(label->getText()=="TRIGGER dBFS")return false;
        }
        if(!thresholdFound||fresh->apvts.getRawParameterValue(
            PRISMVSTAudioProcessor::rootsRootControlId(0,"comp"))->load()!=0)return false;
        auto hidden=PRISMVSTAudioProcessor::carveId(0,0,"threshold");
        if(fresh->apvts.getRawParameterValue(hidden)==nullptr)return false;
        set(*fresh,hidden,-37.2f);
        set(*fresh,PRISMVSTAudioProcessor::rootsRootControlId(0,"comp"),3);
        juce::MemoryBlock recalled;fresh->getStateInformation(recalled);
        auto restored=std::make_unique<PRISMVSTAudioProcessor>();
        restored->setStateInformation(recalled.getData(),(int)recalled.getSize());
        if(std::abs(restored->apvts.getRawParameterValue(hidden)->load()+37.2f)>.11f
           ||restored->apvts.getRawParameterValue(PRISMVSTAudioProcessor::rootsRootControlId(0,"comp"))->load()!=3)return false;
        std::cout<<"QUALITY 1.0.2: all ROOTs default OFF/0 dBFS; threshold gesture stays OFF; hidden CARVE threshold and selected compressor recall\n";
    }
    auto render=[&](int size){
        auto p=std::make_unique<PRISMVSTAudioProcessor>();
        set(*p,PRISMVSTAudioProcessor::rootsRootControlId(4,"comp"),4);
        set(*p,PRISMVSTAudioProcessor::rootsRootControlId(4,"threshold"),-38);
        set(*p,PRISMVSTAudioProcessor::rootsRootControlId(4,"onyx_tape"),30);
        set(*p,PRISMVSTAudioProcessor::carveId(4,0,"on"),1);
        set(*p,PRISMVSTAudioProcessor::carveId(4,0,"hz"),900);
        set(*p,PRISMVSTAudioProcessor::carveId(4,0,"depth"),9);
        set(*p,PRISMVSTAudioProcessor::carveId(4,0,"dynamic"),1);
        p->prepareToPlay(48000,512);
        std::vector<float> output;
        for(int start=0;start<8192;start+=size){
            const int count=juce::jmin(size,8192-start);
            juce::AudioBuffer<float> block(2,count);tone(block,start);
            p->processBlock(block,midi);
            output.insert(output.end(),block.getReadPointer(0),block.getReadPointer(0)+count);
        }
        return output;
    };
    const auto reference=render(512);
    for(int size:{1,31,127,1024,4096}){
        const auto output=render(size);
        for(size_t i=0;i<output.size();++i)
            if(std::abs(output[i]-reference[i])>2e-6f){
                std::cerr<<"SURGERY block partition mismatch: "<<size<<" at "<<i<<"\n";return false;
            }
    }
    std::cout<<"QUALITY: ROOT compression/ONYX/dynamic CARVE invariant across host buffer sizes\n";

    auto p=std::make_unique<PRISMVSTAudioProcessor>();
    set(*p,"surgery_bypass",1);set(*p,"surgery_output_db",-12);
    set(*p,PRISMVSTAudioProcessor::rootsRootGainId(4),12);
    p->prepareToPlay(48000,64);
    juce::AudioBuffer<float> block(2,4097);tone(block,0);
    juce::AudioBuffer<float> dry;dry.makeCopyOf(block);
    p->processBlock(block,midi);
    for(int c=0;c<2;++c)
        if(std::memcmp(block.getReadPointer(c),dry.getReadPointer(c),4097*sizeof(float))!=0){
            std::cerr<<"SURGERY bypass not exact\n";return false;
        }
    juce::AudioBuffer<float> empty(2,0);p->processBlock(empty,midi);
    block.clear();block.setSample(0,0,std::numeric_limits<float>::quiet_NaN());
    block.setSample(1,0,std::numeric_limits<float>::infinity());
    p->processBlock(block,midi);
    for(int c=0;c<2;++c)for(int i=0;i<block.getNumSamples();++i)
        if(!std::isfinite(block.getSample(c,i)))return false;

    // A/B is an instance-level audition buffer; preset files are durable.
    const auto gain=PRISMVSTAudioProcessor::rootsRootGainId(4);
    set(*p,gain,3);p->copyComparison();set(*p,gain,8);p->switchComparison();
    if(std::abs(p->apvts.getRawParameterValue(gain)->load()-3)>0.01f)return false;
    p->switchComparison();
    if(std::abs(p->apvts.getRawParameterValue(gain)->load()-8)>0.01f)return false;
    auto preset=juce::File::getCurrentWorkingDirectory().getChildFile("quality-roundtrip.surgerypreset");
    if(!p->savePreset(preset))return false;
    set(*p,gain,-3);if(!p->loadPreset(preset))return false;
    if(std::abs(p->apvts.getRawParameterValue(gain)->load()-8)>0.01f)return false;
    auto bad=juce::File::getCurrentWorkingDirectory().getChildFile("quality-invalid.xml");
    bad.replaceWithText("<PARAMETERS/>");
    if(p->loadPreset(bad)||std::abs(p->apvts.getRawParameterValue(gain)->load()-8)>0.01f)return false;
    std::cout<<"QUALITY: exact dry bypass, oversized/empty buffers, A/B and preset roundtrips\n";

    // SOLO must audition ONYX and compression, not silently bypass either.
    auto soloEnergy=[&](bool effects){
        prism::roots::v2::Engine engine(48000);
        auto a=engine.get(8);a.hz=500;a.comp=effects?prism::roots::v2::Compressor::fet4:prism::roots::v2::Compressor::off;
        a.thresholdDb=-42;engine.config(8,a);auto z=engine.get(9);z.hz=2000;engine.config(9,z);
        engine.setSoloRoot(4);double energy=0;
        juce::AudioBuffer<float> b(2,16384);tone(b,0);
        engine.process(b.getWritePointer(0),b.getWritePointer(1),16384);
        for(int i=8192;i<16384;++i)energy+=b.getSample(0,i)*b.getSample(0,i);
        return energy;
    };
    if(!(soloEnergy(true)<soloEnergy(false)*0.8)){
        std::cerr<<"SOLO did not audition compression\n";return false;
    }
    {
        prism::roots::v2::Engine engine(48000);
        auto config=engine.get(8);config.hz=500;config.tape=0.5;config.sparkle=0.5;
        engine.config(8,config);config=engine.get(9);config.hz=2000;engine.config(9,config);
        engine.setRootGain(4,12);engine.setRootMono(4,true);engine.setSoloRoot(4);
        juce::AudioBuffer<float> b(2,32768);tone(b,0);
        engine.process(b.getWritePointer(0),b.getWritePointer(1),32768);
        double side=0;
        for(int i=16384;i<32768;++i)side=juce::jmax(side,std::abs((double)b.getSample(0,i)-b.getSample(1,i)));
        if(side>1e-6){std::cerr<<"Gain/ONYX reintroduced SIDE after MONO\n";return false;}
    }
    // Disabling a CARVE must fade to dry; re-enabling after silence must not emit old ringing.
    surgery::CarveEngine carve;carve.prepare(48000);
    surgery::CarveConfig config;config.enabled=true;config.hz=900;config.depthDb=24;config.q=40;
    carve.configure(4,0,config);tone(block,0);carve.process(block.getWritePointer(0),block.getWritePointer(1),4097);
    config.enabled=false;carve.configure(4,0,config);
    for(int n=0;n<8;++n){block.clear();carve.process(block.getWritePointer(0),block.getWritePointer(1),4097);}
    config.enabled=true;carve.configure(4,0,config);block.clear();
    carve.process(block.getWritePointer(0),block.getWritePointer(1),4097);
    if(block.getMagnitude(0,4097)>1e-8f){std::cerr<<"Stale CARVE state replayed\n";return false;}
    std::cout<<"QUALITY: SOLO processes dynamics; CARVE switching clears stale ringing\n";

    auto analyzer=std::make_unique<PRISMVSTAudioProcessor>();
    set(*analyzer,"surgery_bypass",1); // Calibrate input, not the default active ROOT compressors.
    auto spectralPeak=[&](bool antiPhase){
        analyzer->prepareToPlay(48000,512);
        juce::AudioBuffer<float> b(2,512);
        for(int n=0;n<32;++n){
            for(int i=0;i<512;++i){
                const float x=(float)(0.2*std::sin((n*512+i)*750.0*juce::MathConstants<double>::twoPi/48000.0));
                b.setSample(0,i,x);b.setSample(1,i,antiPhase?-x:x);
            }
            analyzer->processBlock(b,midi);
        }
        std::array<float,PRISMVSTAudioProcessor::spectrumBins> spectrum;
        float maximum=-144;
        for(int attempt=0;attempt<100;++attempt){
            juce::Thread::sleep(10);analyzer->copySpectrum(spectrum);
            maximum=*std::max_element(spectrum.begin(),spectrum.end());
            if(maximum>-14.2f)break;
        }
        return maximum;
    };
    const float monoPeak=spectralPeak(false),antiPeak=spectralPeak(true);
    if(analyzer->getRootsMeter().correlation>-0.999)return false;
    if(std::abs(monoPeak+13.9794f)>0.3f||std::abs(antiPeak-monoPeak)>0.3f){
        std::cerr<<"Stereo analyzer calibration/phase failed: "<<monoPeak<<", "<<antiPeak<<"\n";return false;
    }
    std::array<float,PRISMVSTAudioProcessor::labBars> lab,peaks;
    analyzer->copyPrismLabBars(lab,peaks);
    if(std::abs(*std::max_element(lab.begin(),lab.end())+13.9794f)>0.5f){
        std::cerr<<"Visible analyzer Hann normalization incorrect\n";return false;
    }
    std::cout<<"QUALITY: calibrated stereo analyzer shows in-phase and anti-phase tones equally\n";

    set(*p,"surgery_bypass",0);set(*p,gain,0);p->prepareToPlay(48000,512);
    for(int root=0;root<7;++root){
        set(*p,PRISMVSTAudioProcessor::rootsRootControlId(root,"comp"),4);
        set(*p,PRISMVSTAudioProcessor::rootsRootControlId(root,"onyx_tape"),20);
        for(int slot=0;slot<4;++slot){
            set(*p,PRISMVSTAudioProcessor::carveId(root,slot,"on"),1);
            set(*p,PRISMVSTAudioProcessor::carveId(root,slot,"depth"),6);
            set(*p,PRISMVSTAudioProcessor::carveId(root,slot,"dynamic"),1);
        }
    }
    juce::AudioBuffer<float> stress(2,512);
    double elapsed=0;
    for(int n=0;n<120;++n){
        tone(stress,n*512);
        if(n%8==0){
            set(*p,PRISMVSTAudioProcessor::carveId(4,0,"q"),n%16==0?96.0f:0.3f);
            set(*p,PRISMVSTAudioProcessor::carveId(4,0,"hz"),n%16==0?520.0f:1800.0f);
        }
        startAllocationProbe();
        const auto start=std::chrono::steady_clock::now();
        p->processBlock(stress,midi);
        elapsed+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        if(finishAllocationProbe()!=0){std::cerr<<"Audio callback allocated C++ heap storage\n";return false;}
        for(int c=0;c<2;++c)for(int i=0;i<512;++i)
            if(!std::isfinite(stress.getSample(c,i)))return false;
    }
    std::cout<<"QUALITY: no C++ new allocations in 120 stressed audio callbacks; elapsed/audio ratio "
             <<elapsed/(120.0*512.0/48000.0)<<" (machine-specific wall time, not a DAW CPU guarantee)\n";
    if(!p->savePreset(preset))return false;
    auto restored=std::make_unique<PRISMVSTAudioProcessor>();
    if(!restored->loadPreset(preset))return false;
    for(auto* base:p->getParameters()){
        const auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(base);
        if(parameter==nullptr)continue;
        const auto* recalled=restored->apvts.getParameter(parameter->paramID);
        if(recalled==nullptr||std::abs(recalled->getValue()-parameter->getValue())>1e-6f){
            std::cerr<<"Preset lost parameter "<<parameter->paramID<<"\n";return false;
        }
    }
    auto oldState=p->apvts.copyState();
    for(int i=oldState.getNumChildren();--i>=0;){
        const auto id=oldState.getChild(i).getProperty("id").toString();
        if(id=="surgery_bypass"||id=="surgery_output_db")oldState.removeChild(i,nullptr);
    }
    auto legacy=juce::File::getCurrentWorkingDirectory().getChildFile("quality-legacy.surgerypreset");
    if(!oldState.createXml()->writeTo(legacy))return false;
    auto oldRecall=std::make_unique<PRISMVSTAudioProcessor>();
    set(*oldRecall,"surgery_output_db",-6);set(*oldRecall,"surgery_bypass",1);
    if(!oldRecall->loadPreset(legacy)||std::abs(oldRecall->apvts.getRawParameterValue("surgery_output_db")->load())>0.001f)
        return false;
    if(oldRecall->apvts.getRawParameterValue("surgery_bypass")->load()>0.5f)return false;
    auto poisoned=p->apvts.copyState().createXml();
    poisoned->getChildByAttribute("id",gain)->setAttribute("value","NaN");
    if(!poisoned->writeTo(bad)||p->loadPreset(bad))return false;
    std::cout<<"QUALITY: every host parameter survives preset recall; old states default to unity output\n";

    // Exercise all actual interface modes and themes without changing their architecture.
    set(*p,"surgery_bypass",0);set(*p,gain,0);p->prepareToPlay(48000,512);
    juce::Random previewNoise(44017);double lowNoise=0,midNoise=0;
    for(int n=0;n<12;++n){
        for(int i=0;i<block.getNumSamples();++i){
            const double white=previewNoise.nextFloat()*2-1;
            lowNoise+=.018*(white-lowNoise);midNoise+=.16*(white-midNoise);
            const auto sample=(float)(.7*lowNoise+.13*midNoise+.018*white
                +.06*std::sin((n*block.getNumSamples()+i)*juce::MathConstants<double>::twoPi*110/48000));
            block.setSample(0,i,sample);block.setSample(1,i,sample*.85f);
        }
        p->processBlock(block,midi);
    }
    juce::Thread::sleep(40); // bounded analyzer handoff for deterministic screenshot data
    set(*p,"aura_color",1);set(*p,"sonar_weather_mode",0);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());
    const char* modes[]={"ROOT","CARVE","ONYX","ANALYZE"};
    for(int theme=0;theme<1;++theme){
        for(int mode=0;mode<4;++mode){
            if(!click(*editor,modes[mode]))return false;
            const int width=theme==0?1120:840;
            editor->setSize(width,theme==0?755:610);
            auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());
            auto file=juce::File::getCurrentWorkingDirectory().getChildFile("surgery-ui-"+juce::String(theme)+"-"+modes[mode]+".png");
            juce::FileOutputStream stream(file);stream.setPosition(0);stream.truncate();
            if(!juce::PNGImageFormat().writeImageToStream(snapshot,stream))return false;
            if(theme==0&&mode==3){
                juce::TextButton* sonar=nullptr;
                for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))
                    if(button->getButtonText().startsWith("SONAR "))sonar=button;
                if(sonar==nullptr||!sonar->getToggleState())return false;
                sonar->setToggleState(false,juce::sendNotificationSync);
                juce::Thread::sleep(40);juce::Timer::callPendingTimersSynchronously();
                if(p->apvts.getRawParameterValue("aura_color")->load()>.5f)return false;
                auto off=editor->createComponentSnapshot(editor->getLocalBounds());
                int changed=0;
                for(int y=180;y<460;++y)for(int x=120;x<990;++x)
                    if(snapshot.getPixelAt(x,y)!=off.getPixelAt(x,y))++changed;
                if(changed<5000){std::cerr<<"SONAR ON has no visible heatmap: "<<changed<<" pixels\n";return false;}
                juce::FileOutputStream offFile(juce::File::getCurrentWorkingDirectory().getChildFile("surgery-ui-sonar-OFF.png"));
                offFile.setPosition(0);offFile.truncate();
                if(!juce::PNGImageFormat().writeImageToStream(off,offFile))return false;
                sonar->setToggleState(true,juce::sendNotificationSync);
                juce::Thread::sleep(40);juce::Timer::callPendingTimersSynchronously();
                if(p->apvts.getRawParameterValue("aura_color")->load()<.5f)return false;
                std::cout<<"QUALITY 1.0.3: SONAR button enables visible full-spectrum fill ("<<changed<<" pixels), OFF hides it\n";
            }
            if(theme==0&&mode==1){
                auto highDpi=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.0f);
                auto file2=juce::File::getCurrentWorkingDirectory().getChildFile("surgery-ui-CARVE-2x.png");
                juce::FileOutputStream stream2(file2);stream2.setPosition(0);stream2.truncate();
                if(!juce::PNGImageFormat().writeImageToStream(highDpi,stream2))return false;
            }
            if(theme==0){
                editor->setSize(840,610);
                auto minimum=editor->createComponentSnapshot(editor->getLocalBounds());
                auto minFile=juce::File::getCurrentWorkingDirectory().getChildFile("surgery-ui-rack-min-"+juce::String(modes[mode])+".png");
                juce::FileOutputStream minStream(minFile);minStream.setPosition(0);minStream.truncate();
                if(!juce::PNGImageFormat().writeImageToStream(minimum,minStream))return false;
                editor->setSize(width,755);
            }
        }
    }
    // Old host/state theme values must never switch away from the current rack.
    if(!click(*editor,"FREEZE"))return false;
    juce::Thread::sleep(40);juce::Timer::callPendingTimersSynchronously();
    auto rackReference=editor->createComponentSnapshot(editor->getLocalBounds());
    for(int legacy=0;legacy<4;++legacy){
        set(*p,"surgery_theme",(float)legacy);
        juce::Thread::sleep(40);juce::Timer::callPendingTimersSynchronously();
        auto recalled=editor->createComponentSnapshot(editor->getLocalBounds());
        for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))
            if(button->getButtonText().startsWith("THEME"))return false;
        for(int y=0;y<rackReference.getHeight();++y)for(int x=0;x<rackReference.getWidth();++x)
            if(rackReference.getPixelAt(x,y)!=recalled.getPixelAt(x,y)){std::cerr<<"Legacy theme changed the UI\n";return false;}
    }
    if(!verifyUI104(*p,*editor))return false;
    std::cout<<"QUALITY: all four modes rendered; legacy theme values preserve the sole ONYX UI\n";
    return true;
}

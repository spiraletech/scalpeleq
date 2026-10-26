#include "../Source/RootsEditor.h"
#include <chrono>
#include <fstream>
#include <iostream>

bool runScalpelValidation(){
    auto processor=std::make_unique<PRISMVSTAudioProcessor>();
    auto set=[&](const juce::String& id,float v){auto* p=processor->apvts.getParameter(id);if(!p)return false;p->setValueNotifyingHost(p->convertTo0to1(v));return true;};
    processor->prepareToPlay(48000,512);
    set("scalpel_xray",1);set("aura_color",1);
    auto save=[](const juce::Image& image,const juce::String& name){
        juce::FileOutputStream stream(juce::File::getCurrentWorkingDirectory().getChildFile(name));
        stream.setPosition(0);stream.truncate();return juce::PNGImageFormat().writeImageToStream(image,stream);
    };
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    RootsGraph* graph=nullptr;
    for(auto* child:editor->getChildren())if(auto* value=dynamic_cast<RootsGraph*>(child))graph=value;
    if(!graph)return false;
    auto mode=[&](const juce::String& name){for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child);button&&button->getButtonText()==name){button->onClick();return true;}return false;};
    if(!mode("BAND MODE"))return false;
    bool field=false,amount=false;
    for(auto* child:editor->getChildren()){
        if(auto* label=dynamic_cast<juce::Label*>(child))if(label->getText()=="FIELD")field=true;
        if(auto* slider=dynamic_cast<CompressionAmountSlider*>(child)){
            amount=true;
            const auto left=slider->proportionOfLengthToValue(0),right=slider->proportionOfLengthToValue(1);
            if(!(left==0&&right==-60))return false;
            slider->setValue(-35,juce::sendNotificationSync);
            if(std::abs(processor->apvts.getRawParameterValue(PRISMVSTAudioProcessor::rootsRootControlId(0,"threshold"))->load()+35)>.1)return false;
        }
    }
    if(!field||!amount)return false;
    const auto phaseId=PRISMVSTAudioProcessor::rootsRootControlId(0,"phase");
    if(!processor->apvts.getParameter(phaseId)->getName(128).contains("PHASE AUTO CORRECTION"))return false;
    for(float depth:{-90.f,90.f}){
        const auto id=PRISMVSTAudioProcessor::carveId(4,0,"depth");set(id,depth);
        juce::MemoryBlock state;processor->getStateInformation(state);
        auto recalled=std::make_unique<PRISMVSTAudioProcessor>();recalled->setStateInformation(state.getData(),(int)state.getSize());
        if(std::abs(recalled->apvts.getRawParameterValue(id)->load()-depth)>.011f)return false;
    }
    set(PRISMVSTAudioProcessor::carveId(4,0,"depth"),0);
    juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
    const auto fixture=juce::File::getCurrentWorkingDirectory().getChildFile("surgery-test-music.ogg");
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(fixture));
    if(!reader)return false;
    int64_t pos=reader->lengthInSamples/3;
    auto play=[&]{
        if(pos+512>=reader->lengthInSamples)pos=0;
        reader->read(&audio,0,512,pos,true,true);pos+=512;
        processor->processBlock(audio,midi);
    };
    for(int i=0;i<180;++i){play();if(i%4==3)juce::MessageManager::getInstance()->runDispatchLoopUntil(16);}
    juce::MessageManager::getInstance()->runDispatchLoopUntil(60);
    if(!graph->spectralMetrics().signalPresent){std::cerr<<"No measured fixture signal in native UI validation\n";return false;}
    graph->setFrozen(true);graph->setXray(true);
    std::ofstream timings("scalpel-render-timings.csv");timings<<"width,height,mode,render_ms\n";
    for(const auto size:{juce::Point<int>(1440,810),juce::Point<int>(3840,2160)}){
        editor->setSize(size.x,size.y);
        graph->setFrozen(false);
        for(int warm=0;warm<16;++warm){
            for(int block=0;block<4;++block)play();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
            editor->createComponentSnapshot(editor->getLocalBounds());
            if(juce::String(graph->gpuStatus())=="GPU LIVE"&&warm>3)break;
            if(juce::String(graph->gpuStatus())=="GPU DISABLED")break;
        }
        graph->setFrozen(true);
        const auto gpu=graph->gpuRasterSize();
        std::cout<<"SCALPEL GPU "<<graph->gpuStatus()<<" at "<<gpu.x<<'x'<<gpu.y<<" for editor "<<size.x<<'x'<<size.y<<'\n';
        if(juce::String(graph->gpuStatus())=="GPU WARMING")return false;
        if(juce::String(graph->gpuStatus())=="GPU LIVE"&&size.x==3840&&gpu.x<3000)return false;
        for(const auto* name:{"BAND MODE","CARVE","COLOR","ANALYZE"}){
            if(!mode(name))return false;
            const auto start=std::chrono::steady_clock::now();
            auto image=editor->createComponentSnapshot(editor->getLocalBounds());
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            timings<<size.x<<','<<size.y<<','<<name<<','<<ms<<'\n';
            if(image.getWidth()!=size.x||image.getHeight()!=size.y)return false;
            if(!save(image,"scalpel-"+juce::String(size.x)+"-"+name+".png"))return false;
        }
    }
    editor->setSize(1440,810);mode("COLOR");
    const auto frozenA=graph->createComponentSnapshot(graph->getLocalBounds());
    juce::Thread::sleep(35);juce::Timer::callPendingTimersSynchronously();
    const auto frozenB=graph->createComponentSnapshot(graph->getLocalBounds());
    for(int y=0;y<frozenA.getHeight();++y)for(int x=0;x<frozenA.getWidth();++x)
        if(frozenA.getPixelAt(x,y)!=frozenB.getPixelAt(x,y)){std::cerr<<"X-RAY freeze changed pixels\n";return false;}
    // Actual changing FFT frames from the music fixture; a preview sequence,
    // not a claim that full-editor rendering sustains 60 FPS.
    graph->setFrozen(false);
    for(int frame=0;frame<12;++frame){
        for(int j=0;j<8;++j){play();if(j%4==3)juce::MessageManager::getInstance()->runDispatchLoopUntil(16);}
        juce::Thread::sleep(25);juce::Timer::callPendingTimersSynchronously();
        if(!save(editor->createComponentSnapshot(editor->getLocalBounds()),"scalpel-live-"+juce::String(frame)+".png"))return false;
    }
    std::cout<<"SCALPEL: native 3840x2160 all modes, clockwise amount, FIELD, phase host name, bipolar recall, frozen pixels PASS\n";
    return true;
}

#pragma once
#include "../Source/RootsEditor.h"
#include <fstream>
inline bool verifySonarRecovery(){
    auto processor=std::make_unique<PRISMVSTAudioProcessor>();processor->prepareToPlay(48000,512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());editor->setSize(1120,755);
    RootsGraph* graph=nullptr;for(auto* child:editor->getChildren())if(auto* g=dynamic_cast<RootsGraph*>(child))graph=g;
    if(!graph)return false;
    auto set=[&](const char* id,float value){auto* p=processor->apvts.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(value));};
    auto pump=[](int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);};
    auto save=[](const juce::Image& image,const juce::String& name){juce::FileOutputStream out(juce::File::getCurrentWorkingDirectory().getChildFile(name));out.setPosition(0);out.truncate();return juce::PNGImageFormat().writeImageToStream(image,out);};
    auto mode=[&](const char* text){for(auto* child:editor->getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child))if(b->getButtonText()==text){b->onClick();return true;}return false;};
    set("aura_memory",.05f);set("aura_color",1);
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File::getCurrentWorkingDirectory().getChildFile("surgery-test-music.ogg")));
    if(!reader||reader->lengthInSamples<100)return false;
    juce::AudioBuffer<float> music(2,(int)reader->lengthInSamples);reader->read(&music,0,music.getNumSamples(),0,true,true);
    juce::AudioBuffer<float> block(2,512);juce::MidiBuffer midi;juce::Random random(1015);double pink=0,pink2=0,pink3=0;long long position=0;
    auto feed=[&](int signal,float amplitude,int blocks){
        for(int n=0;n<blocks;++n){
            for(int i=0;i<512;++i,++position){
                const double t=position/48000.0;double v=0;
                if(signal==1||signal==4)v=amplitude*std::sin(juce::MathConstants<double>::twoPi*(signal==1?1000:4000)*t);
                if(signal==2){double white=random.nextFloat()*2-1;pink=.99765*pink+white*.099046;pink2=.963*pink2+white*.296516;pink3=.57*pink3+white*1.05269;v=(pink+pink2+pink3+white*.1848)*amplitude*.22;}
                for(int ch=0;ch<2;++ch){
                    double sample=v;
                    if(signal==3){const double source=position*reader->sampleRate/48000.0;const int a=(int)std::fmod(source,music.getNumSamples());const int b=(a+1)%music.getNumSamples();const double u=source-std::floor(source);sample=amplitude*((1-u)*music.getSample(ch,a)+u*music.getSample(ch,b));}
                    block.setSample(ch,i,(float)sample);
                }
            }
            processor->processBlock(block,midi);
            for(int ch=0;ch<2;++ch)for(int i=0;i<512;++i)if(!std::isfinite(block.getSample(ch,i)))return false;
            if(n%4==3)pump(12);
        }
        pump(60);return true;
    };
    editor->addToDesktop(0);editor->setTopLeftPosition(30,30);editor->setVisible(true);
    std::ofstream evidence("surgery-sonar-recovery.csv");evidence<<"signal,tab,peak_hz,peak_db,heat_pixels_ge5,mean_contrast\n";
    std::ofstream refresh("surgery-sonar-live-refresh.csv");refresh<<"signal,native_paints,elapsed_ms,paint_callbacks_per_second\n";
    float firstPeak=0;
    for(int signal:{1,2,3,4}){
        graph->setFrozen(false);const auto liveStart=std::chrono::steady_clock::now();const auto paintsBefore=graph->paintedFrames();if(!feed(signal,signal==3?.8f:.3f,64))return false;
        if(graph->paintedFrames()<=paintsBefore+2){std::cerr<<"Recovery: native window did not animate\n";return false;}
        const double liveMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-liveStart).count();
        refresh<<signal<<','<<graph->paintedFrames()-paintsBefore<<','<<liveMs<<','<<(graph->paintedFrames()-paintsBefore)*1000/liveMs<<'\n';
        const auto metrics=graph->spectralMetrics();
        if(!metrics.signalPresent||metrics.loudestDb<=-85){std::cerr<<"Recovery: live pipeline lacks signal "<<signal<<"\n";return false;}
        if(signal==1){firstPeak=metrics.loudestHz;if(std::abs(firstPeak-1000)>120)return false;}
        if(signal==4&&std::abs(metrics.loudestHz-4000)>400)return false;
        for(const char* tab:{"ROOT","CARVE","ONYX","ANALYZE"}){
            if(!mode(tab))return false;pump(25);graph->setFrozen(true);
            set("aura_color",1);auto on=graph->createComponentSnapshot(graph->getLocalBounds());
            set("aura_color",0);auto off=graph->createComponentSnapshot(graph->getLocalBounds());
            int visible=0;double contrast=0;int count=0;
            for(int y=70;y<graph->getHeight()-60;++y)for(int x=65;x<graph->getWidth()-65;++x){
                auto a=on.getPixelAt(x,y),b=off.getPixelAt(x,y);
                const int difference=std::max({std::abs(a.getRed()-b.getRed()),std::abs(a.getGreen()-b.getGreen()),std::abs(a.getBlue()-b.getBlue())});
                if(difference>=5)++visible;contrast+=difference;++count;
            }
            evidence<<signal<<','<<tab<<','<<metrics.loudestHz<<','<<metrics.loudestDb<<','<<visible<<','<<contrast/count<<'\n';
            if(signal==2&&(visible<10000||contrast/count<2)){std::cerr<<"Recovery: pink-noise heat is invisible\n";return false;}
            if(visible<80){std::cerr<<"Recovery: no visible heat for "<<signal<<' '<<tab<<"\n";return false;}
            set("aura_color",1);graph->setFrozen(false);pump(25);
            if(!save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-recovery-signal"+juce::String(signal)+"-"+tab+".png"))return false;
        }
        // Selecting another ROOT changes presentation but never FFT buffers.
        std::array<float,PRISMVSTAudioProcessor::labBars> before{},pk{},after{},ap{};
        processor->copyPrismLabBars(before,pk);graph->selectNode(8);processor->copyPrismLabBars(after,ap);
        if(before!=after||pk!=ap)return false;graph->selectNode(0);
    }
    graph->setFrozen(false);if(!feed(4,.03f,80))return false;const float quiet=graph->spectralMetrics().loudestDb;
    if(!feed(4,.3f,40)||graph->spectralMetrics().loudestDb<quiet+12)return false;
    if(!feed(0,0,180))return false;graph->setFrozen(true);
    const auto silence=graph->spectralMetrics();if(silence.loudestDb>-115)return false;
    set("aura_color",1);auto silentOn=graph->createComponentSnapshot(graph->getLocalBounds());
    set("aura_color",0);auto silentOff=graph->createComponentSnapshot(graph->getLocalBounds());
    int silentDifference=0;for(int y=70;y<graph->getHeight()-60;++y)for(int x=65;x<graph->getWidth()-65;++x)
        if(silentOn.getPixelAt(x,y)!=silentOff.getPixelAt(x,y))++silentDifference;
    if(silentDifference>100){std::cerr<<"Recovery: fabricated silence heat "<<silentDifference<<'\n';return false;}
    if(!save(silentOn,"surgery-ui-recovery-silence.png"))return false;
    editor->removeFromDesktop();
    // Display controls are audio-null, even while animated/automated.
    auto reference=std::make_unique<PRISMVSTAudioProcessor>();auto visual=std::make_unique<PRISMVSTAudioProcessor>();
    reference->prepareToPlay(48000,512);visual->prepareToPlay(48000,512);juce::AudioBuffer<float> a(2,512),b(2,512);
    for(int n=0;n<80;++n){
        for(const auto* id:{"aura_color","aura_memory","analyzer_depth","analyzer_slope"}){
            auto* parameter=visual->apvts.getParameter(id);parameter->setValueNotifyingHost((n%17)/16.f);
        }
        for(int ch=0;ch<2;++ch)for(int i=0;i<512;++i){const float v=(random.nextFloat()*2-1)*.12f;a.setSample(ch,i,v);b.setSample(ch,i,v);}
        reference->processBlock(a,midi);visual->processBlock(b,midi);
        for(int ch=0;ch<2;++ch)for(int i=0;i<512;++i)if(a.getSample(ch,i)!=b.getSample(ch,i))return false;
    }
    std::cout<<"RECOVERY: live-before-audio editor, 1k/4k sine, pink noise, recorded singing, four tabs, silence, ROOT measurement invariance and bit-exact visualization/audio null passed\n";
    return true;
}

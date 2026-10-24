#pragma once
#include "../Source/RootsEditor.h"
#include <chrono>
#include <fstream>
void startAllocationProbe() noexcept;
std::size_t finishAllocationProbe() noexcept;
inline bool verifyWeatherUI(){
    using namespace surgery::weather;
    auto fail=[](const char* text){std::cerr<<"WEATHER UI FAIL: "<<text<<'\n';return false;};
    auto set=[](PRISMVSTAudioProcessor& p,const char* id,float value){auto* parameter=p.apvts.getParameter(id);if(!parameter)throw std::runtime_error(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));};
    auto pump=[](int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);};
    auto save=[](const juce::Image& image,const juce::String& name){juce::FileOutputStream out(juce::File::getCurrentWorkingDirectory().getChildFile(name));out.setPosition(0);out.truncate();return juce::PNGImageFormat().writeImageToStream(image,out);};
    juce::MidiBuffer midi;juce::Random random(2106);
    // Core samples must remain bit-exact with OFF/standard/weather at every rate.
    std::ofstream audioReport("surgery-weather-audio-null.csv");audioReport<<"rate,mode,max_sample_difference,audio_allocations,lab_difference\n";
    for(double rate:{44100.,48000.,96000.,192000.})for(int mode=0;mode<8;++mode){
        auto reference=std::make_unique<PRISMVSTAudioProcessor>(),visual=std::make_unique<PRISMVSTAudioProcessor>();
        reference->prepareToPlay(rate,512);visual->prepareToPlay(rate,512);
        std::unique_ptr<juce::AudioProcessorEditor> weatherEditor(visual->createEditor());
        set(*visual,"sonar_weather_mode",0);set(*visual,"sonar_visual_style",(float)mode);set(*visual,"juke_mode",(float)(mode%6));set(*visual,"aura_color",mode==0?0.f:1.f);set(*visual,"sonar_reduced_motion",mode%2);
        juce::AudioBuffer<float> a(2,512),b(2,512);double difference=0;std::size_t allocations=0;
        for(int block=0;block<40;++block){
            for(int ch=0;ch<2;++ch)for(int i=0;i<512;++i){const float v=(float)(.12*std::sin((block*512+i)*juce::MathConstants<double>::twoPi*1000/rate))+(random.nextFloat()*2-1)*.02f;a.setSample(ch,i,v);b.setSample(ch,i,v);}
            startAllocationProbe();reference->processBlock(a,midi);visual->processBlock(b,midi);allocations+=finishAllocationProbe();
            for(int ch=0;ch<2;++ch)for(int i=0;i<512;++i)difference=std::max(difference,(double)std::abs(a.getSample(ch,i)-b.getSample(ch,i)));
        }
        pump(160);std::array<float,PRISMVSTAudioProcessor::labBars> labA{},ap{},labB{},bp{};
        reference->copyPrismLabBars(labA,ap);visual->copyPrismLabBars(labB,bp);double labDifference=0;for(size_t i=0;i<labA.size();++i)labDifference=std::max(labDifference,(double)std::abs(labA[i]-labB[i]));
        audioReport<<rate<<','<<mode<<','<<difference<<','<<allocations<<','<<labDifference<<'\n';
        if(difference!=0||allocations!=0||labDifference>.001)return fail("visual mode modified audio/FFT or allocated in callback");
        if(visual->getLatencySamples()!=reference->getLatencySamples())return fail("visual latency");
    }
    auto p=std::make_unique<PRISMVSTAudioProcessor>();p->prepareToPlay(48000,512);
    if(p->apvts.getRawParameterValue("sonar_weather_mode")->load()!=0||p->apvts.getRawParameterValue("sonar_reduced_motion")->load()!=1)return fail("optional/default settings");
    const auto* legacyChoice=dynamic_cast<juce::AudioParameterChoice*>(p->apvts.getParameter("sonar_weather_mode"));
    if(!legacyChoice||legacyChoice->choices.size()!=2||legacyChoice->convertTo0to1(1)!=1||p->apvts.getRawParameterValue("sonar_visual_style")->load()!=0||p->apvts.getRawParameterValue("juke_mode")->load()!=0)return fail("legacy normalized choice / fresh defaults");
    std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());editor->setSize(1120,755);
    RootsGraph* graph=nullptr;for(auto* child:editor->getChildren())if(auto* g=dynamic_cast<RootsGraph*>(child))graph=g;if(!graph)return false;
    editor->addToDesktop(0);editor->setTopLeftPosition(20,20);editor->setVisible(true);
    juce::AudioFormatManager musicFormats;musicFormats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> musicReader(musicFormats.createReaderFor(juce::File::getCurrentWorkingDirectory().getChildFile("surgery-test-music.ogg")));
    if(!musicReader)return fail("recorded music fixture");
    juce::AudioBuffer<float> music(2,(int)musicReader->lengthInSamples);musicReader->read(&music,0,music.getNumSamples(),0,true,true);
    bool lightningCaptured=false;
    juce::AudioBuffer<float> block(2,512);long long sample=0;double slow=0;
    auto feed=[&](int kind,int blocks){
        for(int n=0;n<blocks;++n){for(int i=0;i<512;++i,++sample){const double t=sample/48000.;double value=0,right=0;
            if(kind==0)value=.25*std::sin(juce::MathConstants<double>::twoPi*80*t)+.08*std::sin(juce::MathConstants<double>::twoPi*160*t);
            if(kind==1)value=.20*std::sin(juce::MathConstants<double>::twoPi*8000*t);
            if(kind==2||kind==3)value=.16*std::sin(juce::MathConstants<double>::twoPi*440*t);
            if(kind==4){const double noise=random.nextFloat()*2-1;slow+=.035*(noise-slow);value=.6*slow+.1*noise;value*=sample%12000<1500?1:.18;}
            right=kind==2?.16*std::sin(juce::MathConstants<double>::twoPi*660*t):kind==3?-value:value;
            if(kind==5){const double position=sample*musicReader->sampleRate/48000.;const int a=(int)std::fmod(position,music.getNumSamples()),b=(a+1)%music.getNumSamples();const double fraction=position-std::floor(position);value=.7*((1-fraction)*music.getSample(0,a)+fraction*music.getSample(0,b));right=.7*((1-fraction)*music.getSample(1,a)+fraction*music.getSample(1,b));}
            block.setSample(0,i,(float)value);block.setSample(1,i,(float)right);
        }p->processBlock(block,midi);if(n%4==3){pump(16);if(kind==5&&n%8==7)save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-vessel-motion-"+juce::String(n/8).paddedLeft('0',2)+".png");if(kind==4&&!lightningCaptured&&graph->weatherMeaning().flash>.01f){lightningCaptured=save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-weather-onset.png");}}}pump(90);
    };
    feed(0,48);if(!graph->spectralMetrics().signalPresent||!graph->weatherActive())return fail("Standard+ default regression");
    if(!save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-weather-standard.png"))return false;
    set(*p,"sonar_weather_mode",1);set(*p,"aura_memory",.18f);
    feed(0,96);if(!graph->weatherActive()||graph->weatherMeaning().bass<.15||graph->weatherMeaning().flash!=0)return fail("bass/reduced weather response");
    const Snapshot bass=graph->weatherMeasured();
    if(!save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-weather-bass.png"))return false;
    feed(1,160);const Snapshot treble=graph->weatherMeasured();
    if(treble.centroid<bass.centroid+.2f||treble.bass>=bass.bass)return fail("centroid/bass acoustic response");
    if(!save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-weather-treble.png"))return false;
    feed(3,100);if(graph->weatherMeasured().correlation>-.95||graph->weatherMeasured().width<.95)return fail("antiphase warning/width");
    if(!save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-weather-antiphase.png"))return false;
    feed(2,96);if(std::abs(graph->weatherMeasured().correlation)>.15||std::abs(graph->weatherMeasured().width-.5)>.1)return fail("width measurement");
    set(*p,"sonar_reduced_motion",0);feed(4,160);
    auto mode=[&](const char* text){for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()==text){button->onClick();return true;}return false;};
    for(const char* tab:{"ROOT","CARVE","ONYX","ANALYZE"}){
        if(!mode(tab))return false;feed(4,32);
        for(float scale:{1.f,1.25f,1.5f})if(!save(editor->createComponentSnapshot(editor->getLocalBounds(),true,scale),"surgery-ui-weather-"+juce::String(tab)+"-"+juce::String((int)(scale*100))+".png"))return false;
    }
    feed(5,128);if(!save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-weather-music.png"))return false;
    graph->setFrozen(true);
    const auto heldMeasurement=graph->weatherMeasured();const auto heldMeaning=graph->weatherMeaning();
    const auto heldField=graph->weatherField();const auto heldSpectrum=graph->spectralMetrics();
    // Compare through a deterministic software target; native GPU snapshots
    // may round isolated interpolation pixels differently on repeated draws.
    // The real native editor/scales are captured independently above.
    auto heldRender=[&](){juce::Image image(juce::Image::ARGB,graph->getWidth(),graph->getHeight(),true,juce::SoftwareImageType{});juce::Graphics g(image);graph->paint(g);return image;};
    auto frozen=heldRender();feed(1,48);auto unchanged=heldRender();
    if(std::memcmp(&heldMeasurement,&graph->weatherMeasured(),sizeof(heldMeasurement))||std::memcmp(&heldMeaning,&graph->weatherMeaning(),sizeof(heldMeaning))||std::memcmp(&heldField,&graph->weatherField(),sizeof(heldField)))return fail("freeze advanced weather state");
    if(heldSpectrum.approximateCentroidHz!=graph->spectralMetrics().approximateCentroidHz)return fail("freeze advanced spectrum");
    for(int y=0;y<frozen.getHeight();++y)for(int x=0;x<frozen.getWidth();++x)if(frozen.getPixelAt(x,y)!=unchanged.getPixelAt(x,y)){save(frozen,"surgery-ui-weather-freeze-before.png");save(unchanged,"surgery-ui-weather-freeze-after.png");return fail("freeze changed software-rendered scene");}
    set(*p,"aura_color",0);pump(40);if(graph->weatherActive())return fail("OFF ignored while frozen");
    graph->setFrozen(false);set(*p,"aura_color",0);feed(4,24);if(graph->weatherActive()||!graph->spectralMetrics().signalPresent)return fail("visualization OFF");
    if(!save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-weather-OFF.png"))return false;
    set(*p,"aura_color",1);set(*p,"sonar_weather_mode",0);feed(4,24);if(!graph->weatherActive())return fail("legacy Standard value must render Standard+");
    set(*p,"sonar_weather_mode",1);set(*p,"sonar_reduced_motion",0);
    juce::MemoryBlock state;p->getStateInformation(state);auto restored=std::make_unique<PRISMVSTAudioProcessor>();restored->setStateInformation(state.getData(),(int)state.getSize());
    if(restored->apvts.getRawParameterValue("sonar_weather_mode")->load()!=1||restored->apvts.getRawParameterValue("sonar_reduced_motion")->load()!=0)return fail("mode state recall");
    auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),(int)state.getSize());
    auto oldVisual=std::make_unique<juce::XmlElement>(*xml);
    for(const char* id:{"sonar_visual_style","juke_mode"})if(auto* child=oldVisual->getChildByAttribute("id",id))oldVisual->removeChildElement(child,true);
    juce::MemoryBlock oldVisualState;juce::AudioProcessor::copyXmlToBinary(*oldVisual,oldVisualState);restored->setStateInformation(oldVisualState.getData(),(int)oldVisualState.getSize());
    if(restored->apvts.getRawParameterValue("sonar_weather_mode")->load()!=1||restored->apvts.getRawParameterValue("juke_mode")->load()!=2)return fail("1.0.10 Storm session migration");
    for(const char* id:{"sonar_weather_mode","sonar_reduced_motion","sonar_visual_style","juke_mode"})if(auto* child=xml->getChildByAttribute("id",id))xml->removeChildElement(child,true);
    juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*xml,legacy);restored->setStateInformation(legacy.getData(),(int)legacy.getSize());
    if(restored->apvts.getRawParameterValue("sonar_weather_mode")->load()!=0||restored->apvts.getRawParameterValue("sonar_reduced_motion")->load()!=1||restored->apvts.getRawParameterValue("juke_mode")->load()!=5)return fail("old session changed default mode");
    set(*p,"sonar_weather_mode",1);set(*p,"aura_color",1);feed(6,80);pump(4000);
    if(graph->weatherMeaning().level>.001f)return fail("live silence creates weather energy");
    if(!save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-weather-silence.png"))return false;
    // All visible choices are automatable and recalled independently.
    if(!mode("ANALYZE"))return fail("Analyze selector");
    juce::ComboBox *sonarBox=nullptr,*jukeBox=nullptr;
    for(auto* child:editor->getChildren())if(auto* box=dynamic_cast<juce::ComboBox*>(child)){
        if(box->getNumItems()==1&&box->getItemText(0)=="Standard+")sonarBox=box;
        if(box->getNumItems()==6&&box->getItemText(0)=="Real Jukebox")jukeBox=box;
    }
    if(!sonarBox||!jukeBox||sonarBox->getBounds().intersects(jukeBox->getBounds()))return fail("independent dropdown layout");
    set(*p,"sonar_weather_mode",1);set(*p,"sonar_visual_style",6);pump(40);
    if(p->apvts.getRawParameterValue("sonar_weather_mode")->load()!=1)return fail("host style update rewrites legacy automation");
    set(*p,"sonar_visual_style",0);pump(40);if(sonarBox->getSelectedId()!=8)return fail("legacy values must display Standard+");
    if(sonarBox->getNumItems()!=1)return fail("removed modes remain visible");
    set(*p,"sonar_weather_mode",0);set(*p,"sonar_visual_style",0);set(*p,"juke_mode",0);set(*p,"aura_color",0);feed(4,60);
    if(graph->weatherActive()||graph->weatherMeaning().level<.01f)return fail("JUKE cannot run independently of SONAR");
    for(int j=0;j<6;++j){set(*p,"juke_mode",(float)j);feed(4,24);save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-111-juke-"+juce::String(j)+".png");}
    set(*p,"juke_mode",5);set(*p,"aura_color",1);
    std::ofstream modesReport("surgery-visual-modes.csv");modesReport<<"style,active,level\n";
    for(int v=0;v<8;++v){set(*p,"sonar_visual_style",(float)v);feed(4,48);
        if(!graph->weatherActive()||sonarBox->getSelectedId()!=8)return fail("legacy automation escapes Standard+");
        modesReport<<v<<','<<graph->weatherActive()<<','<<graph->weatherMeaning().level<<'\n';
        save(editor->createComponentSnapshot(editor->getLocalBounds()),"surgery-ui-112-legacy-alias-"+juce::String(v)+".png");
    }
    set(*p,"sonar_visual_style",6);set(*p,"juke_mode",3);p->getStateInformation(state);restored->setStateInformation(state.getData(),(int)state.getSize());
    if(restored->apvts.getRawParameterValue("sonar_visual_style")->load()!=6||restored->apvts.getRawParameterValue("juke_mode")->load()!=3)return fail("expanded mode recall");
    // Re-run bit-exact audio and allocation checks across every new mode.
    editor->removeFromDesktop();
    // Benchmark update/simulation/upload + graph drawing, including two independent fields.
    restored->prepareToPlay(48000,512);std::unique_ptr<juce::AudioProcessorEditor> editor2(restored->createEditor());RootsGraph* graph2=nullptr;
    for(auto* child:editor2->getChildren())if(auto* g=dynamic_cast<RootsGraph*>(child))graph2=g;if(!graph2)return false;
    auto cpuMs=[](){
#if JUCE_WINDOWS
        FILETIME a,b,k,u;if(GetThreadTimes(GetCurrentThread(),&a,&b,&k,&u)){ULARGE_INTEGER kernel,user;kernel.LowPart=k.dwLowDateTime;kernel.HighPart=k.dwHighDateTime;user.LowPart=u.dwLowDateTime;user.HighPart=u.dwHighDateTime;return (kernel.QuadPart+user.QuadPart)/10000.;}
#endif
        return 0.;
    };
    std::ofstream timing("surgery-weather-performance.csv");timing<<"instances,scale,median_ms,p95_ms,max_ms,mean_ms,thread_cpu_ms_per_frame\n";
    for(int instances:{1,2})for(float scale:{1.f,1.25f,1.5f}){
        juce::Image canvas(juce::Image::RGB,(int)(graph->getWidth()*scale),(int)(graph->getHeight()*scale),true);std::vector<double> samples;double cpu=0;
        for(int frame=0;frame<80;++frame){
            Snapshot input=(frame/16)%2?bass:treble;input.seconds=frame/60.;
            const auto start=std::chrono::steady_clock::now();const double cpuStart=cpuMs();
            for(int instance=0;instance<instances;++instance){auto* current=instance?graph2:graph;current->updateWeather(&input,1./60.,true,false,.18);juce::Graphics g(canvas);g.addTransform(juce::AffineTransform::scale(scale));current->paint(g);}
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(frame>=10){samples.push_back(ms);cpu+=cpuMs()-cpuStart;}
        }
        std::sort(samples.begin(),samples.end());double mean=0;for(auto x:samples)mean+=x;mean/=samples.size();
        timing<<instances<<','<<scale<<','<<samples[samples.size()/2]<<','<<samples[samples.size()*95/100]<<','<<samples.back()<<','<<mean<<','<<cpu/samples.size()<<'\n';
    }
    std::cout<<"WEATHER UI: 4 rates x eight styles/six JUKE choices cycled, bit-exact audio/FFT, no callback allocations, live acoustic response, stereo warning, four tabs/scales, freeze, old/new recall and two-instance weather benchmark passed\n";
    return true;
}


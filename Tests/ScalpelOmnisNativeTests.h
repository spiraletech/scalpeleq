#pragma once
#include "../Source/RootsEditor.h"
#include <fstream>
#include <iostream>
#include <ctime>

inline bool verifyScalpelOmnisNative(){
    const auto pump=[](int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);};
    const auto save=[](const juce::Image& im,const juce::String& name){juce::FileOutputStream f(juce::File::getCurrentWorkingDirectory().getChildFile(name));f.setPosition(0);f.truncate();return juce::PNGImageFormat{}.writeImageToStream(im,f);};
    auto p=std::make_unique<PRISMVSTAudioProcessor>();p->prepareToPlay(48000,512);
    auto set=[&](const juce::String& id,float value){auto* v=p->apvts.getParameter(id);if(!v)return false;v->setValueNotifyingHost(v->convertTo0to1(value));return true;};
    const auto now=juce::Time::getMillisecondCounterHiRes();
    std::unique_ptr<juce::AudioProcessorEditor> e(p->createEditor());e->setSize(1344,786);e->addToDesktop(0);e->setVisible(true);pump(60);
    std::ofstream log("omnis-native-evidence.txt");
    log<<"Platform="<<juce::SystemStats::getOperatingSystemName()<<"\nFont="<<scalpel::omnis::face()<<"\nHeaderFont="<<scalpel::omnis::face(true)<<"\nEditorInitAndFirstPumpMs="<<juce::Time::getMillisecondCounterHiRes()-now<<'\n';
    auto button=[&](const juce::String& label)->juce::Button*{for(auto* c:e->getChildren())if(auto* b=dynamic_cast<juce::Button*>(c);b&&b->getButtonText()==label)return b;return nullptr;};
    auto click=[&](const juce::String& label){auto* b=button(label);if(!b||!b->onClick)return false;if(b->getClickingTogglesState())b->setToggleState(!b->getToggleState(),juce::dontSendNotification);b->onClick();return true;};
    if(!button("GRAPH POWER"))return false;
    const auto powerCentre=button("GRAPH POWER")->localPointToGlobal(button("GRAPH POWER")->getLocalBounds().getCentre());
    if(e->getComponentAt(e->getLocalPoint(nullptr,powerCentre))!=button("GRAPH POWER"))return false;
    const auto benchmark=[&](const char* stage,bool audio,bool motion){
        const auto wall=juce::Time::getMillisecondCounterHiRes();const auto cpu=std::clock();int frames=0;
        juce::AudioBuffer<float> buffer(2,512);juce::MidiBuffer midi;
        while(juce::Time::getMillisecondCounterHiRes()-wall<1000){
            if(audio){for(int c=0;c<2;++c)for(int i=0;i<512;++i)buffer.setSample(c,i,.1f*std::sin(.043f*(frames*512+i)));p->processBlock(buffer,midi);}
            if(motion&&frames%4==0)click(frames%8==0?"CARVE":"ANALYZE");
            pump(16);++frames;
        }
        const double wallMs=juce::Time::getMillisecondCounterHiRes()-wall;
        const double cpuMs=1000.*(std::clock()-cpu)/CLOCKS_PER_SEC;
        log<<"MEASURE "<<stage<<" wallMs="<<wallMs<<" processCpuMs="<<cpuMs<<" cpuOneCorePercent="<<cpuMs/wallMs*100.<<" iterations="<<frames<<'\n';
    };
    benchmark("idle",false,false);benchmark("active-analyzer",true,false);
    set("sonar_reduced_motion",0);benchmark("transitions-with-analyzer",true,true);
    set("sonar_reduced_motion",1);click("BAND MODE");pump(40);
    // Live APVTS restoration/automation feeds the existing attachments.
    for(const auto* mode:{"BAND MODE","CARVE","ANALYZE"}){
        if(!click(mode))return false;pump(40);
        if(!save(e->createComponentSnapshot(e->getLocalBounds()),"omnis-"+juce::String(mode).replaceCharacter(' ','-')+"-1344x786.png"))return false;
        for(auto* child:e->getChildren())if(auto* s=dynamic_cast<juce::Slider*>(child);s&&s->isVisible()){
            const auto r=s->getBounds();if(std::abs(r.getY()+scalpel::omnis::controls::localRotaryY-scalpel::omnis::controls::rotaryCentreY)>.001f||r.getWidth()!=168)return false;
            const auto before=s->getValue();s->setValue(s->getMinimum(),juce::sendNotificationSync);
            if(!save(s->createComponentSnapshot(s->getLocalBounds()),"omnis-rotary-"+juce::String(r.getX())+"-"+juce::String(mode).replaceCharacter(' ','-')+"-min.png"))return false;
            s->setValue(s->getMaximum(),juce::sendNotificationSync);
            if(!save(s->createComponentSnapshot(s->getLocalBounds()),"omnis-rotary-"+juce::String(r.getX())+"-"+juce::String(mode).replaceCharacter(' ','-')+"-max.png"))return false;
            s->setValue(before,juce::sendNotificationSync);
        }
    }
    std::vector<float> parameters;for(auto* v:p->getParameters())parameters.push_back(v->getValue());
    if(!click("GRAPH POWER"))return false;pump(35);
    if(!save(e->createComponentSnapshot(e->getLocalBounds()),"omnis-graph-standby.png"))return false;
    if(!click("GRAPH POWER"))return false;pump(35);
    if(!set("sonar_reduced_motion",0))return false;
    // Actual time-based native frames, not generated animation illustrations.
    if(!click("GRAPH POWER"))return false;pump(90);
    if(!save(e->createComponentSnapshot(e->getLocalBounds()),"omnis-CRT-collapse.png"))return false;
    if(!click("GRAPH POWER"))return false;pump(80);
    if(!save(e->createComponentSnapshot(e->getLocalBounds()),"omnis-CRT-ignition.png"))return false;
    pump(300);if(!click("BAND MODE"))return false;pump(100);
    if(!save(e->createComponentSnapshot(e->getLocalBounds()),"omnis-MONOLITH-collapse.png"))return false;
    for(int i=0;i<100;++i){if(!click(i%2?"CARVE":"ANALYZE"))return false;pump(1);}pump(700);
    if(!button("CARVE")->getToggleState()||button("ANALYZE")->getToggleState())return false;
    if(!save(e->createComponentSnapshot(e->getLocalBounds()),"omnis-MONOLITH-resolved.png"))return false;
    for(size_t i=0;i<parameters.size();++i){auto* v=p->getParameters()[(int)i];if(v==p->apvts.getParameter("sonar_reduced_motion"))continue;if(v->getValue()!=parameters[i])return false;}
    log<<"PASS graph power and 100 interrupted transitions leave all audio parameters unchanged\n";
    // Repeated editor ownership/lifecycle with active animations.
    e.reset();for(int i=0;i<20;++i){e.reset(p->createEditor());
        if(!click("CARVE")||!click("GRAPH POWER"))return false;
        pump(1);e.reset();}
    log<<"PASS 20 editor lifecycle iterations\n";
    std::ofstream raw("omnis-audio.f32",std::ios::binary);
    // Reproducible signal fixture used for a separate baseline binary comparison.
    juce::MidiBuffer midi;juce::AudioBuffer<float> audio(2,512);
    for(int block=0;block<40;++block){for(int c=0;c<2;++c)for(int i=0;i<512;++i)audio.setSample(c,i,.1f*std::sin(.043f*(block*512+i)));p->processBlock(audio,midi);for(int c=0;c<2;++c)raw.write((char*)audio.getReadPointer(c),512*sizeof(float));}
    log<<"Signal=stereo .1*sin(.043*n), 48000 Hz, 40x512 frames; separate baseline comparison required\n";
    std::cout<<"PASS OMNIS native presentation, immediate native attachments, CRT, 100 mode requests, 20 lifecycles\n";return true;
}

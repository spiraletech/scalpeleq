#pragma once
#include "../Source/RootsEditor.h"
#include <iostream>
// ET-MOTION-016 supersedes the 1.0.8.6 atlas, red navigation, mismatched
// dial-ring and transparent-cap geometry contracts. Native processing and
// C01's complete regression remain independent gates, not waived here.
inline bool verifyScalpelFinish(){
    const auto fail=[](int line){std::cerr<<"OMNIS UI contract failed at "<<line<<'\n';return false;};
    auto p=std::make_unique<PRISMVSTAudioProcessor>();p->prepareToPlay(48000,512);
    std::unique_ptr<juce::AudioProcessorEditor> e(p->createEditor());
    const auto button=[&](const juce::String& name)->juce::Button*{for(auto* c:e->getChildren())if(auto* b=dynamic_cast<juce::Button*>(c);b&&b->getButtonText()==name&&b->isVisible())return b;return nullptr;};
    const auto choose=[&](const char* name){auto* b=button(name);if(!b||!b->onClick)return false;b->onClick();juce::Timer::callPendingTimersSynchronously();return true;};
    const auto snapshot=[&]{std::vector<float> values;for(auto* v:p->getParameters())values.push_back(v->getValue());return values;};
    const auto baseline=snapshot();
    for(const auto size:std::array<juce::Point<int>,3>{{{896,504},{1344,756},{1792,1008}}}){
        e->setSize(size.x,size.y);
        for(const auto* page:{"BAND MODE","CARVE","ANALYZE"}){
            if(!choose(page))return fail(__LINE__);
            int rotaries=0,selectedTabs=0;
            for(auto* c:e->getChildren()){
                if(auto* b=dynamic_cast<RootSelectorButton*>(c)){
                    const int index=scalpel::finish::bandKeyIndex(b->getButtonText());
                    if(index<0||b->getBounds()!=scalpel::reference::band(index))return fail(__LINE__);
                    const auto t=b->getTransform();const auto l=scalpel::omnis::Layout(size.x,size.y);
                    if(std::abs(t.mat00-l.scale*1280.f/1120.f)>.0001f)return fail(__LINE__);
                }
                if(auto* b=dynamic_cast<juce::Button*>(c);b&&scalpel::omnis::navigation(b->getButtonText())){
                    selectedTabs+=b->getToggleState();if(b->getWidth()!=405||b->getHeight()!=54||b->getY()!=477)return fail(__LINE__);
                }
                if(auto* s=dynamic_cast<juce::Slider*>(c);s&&s->isVisible()){
                    ++rotaries;if(s->getY()+65!=619||s->getWidth()!=168)return fail(__LINE__);
                    auto im=s->createComponentSnapshot(s->getLocalBounds());if(!im.isValid())return fail(__LINE__);
                }
                if(auto* b=dynamic_cast<juce::Button*>(c);b&&b->isVisible()&&b->getY()>=538){
                    if(b->getWidth()!=146||b->getHeight()!=46)return fail(__LINE__);
                    auto im=b->createComponentSnapshot(b->getLocalBounds());
                    for(int y=0;y<im.getHeight();++y)for(int x=0;x<im.getWidth();++x){auto col=im.getPixelAt(x,y);if(col.getRed()>col.getGreen()+18||col.getGreen()>col.getRed()+18)return fail(__LINE__);}
                }
            }
            if(selectedTabs!=1||rotaries!=(juce::String(page)=="BAND MODE"?2:juce::String(page)=="CARVE"?3:1))return fail(__LINE__);
            if(e->getWidth()!=size.x||e->getHeight()!=size.y)return fail(__LINE__);
            if(!e->createComponentSnapshot(e->getLocalBounds()).isValid())return fail(__LINE__);
        }
    }
    if(snapshot()!=baseline)return fail(__LINE__);
    for(const auto* page:{"BAND MODE","CARVE"}){
        if(!choose(page)||!choose("ON"))return fail(__LINE__);
        if(!button("ON")->getToggleState()||button("OFF")->getToggleState())return fail(__LINE__);
        if(!choose("OFF")||button("ON")->getToggleState()||!button("OFF")->getToggleState())return fail(__LINE__);
    }
    for(const auto* name:{"STEREO","MID","SIDE"}){
        if(!choose(name))return fail(__LINE__);int selected=0;for(const auto* n:{"STEREO","MID","SIDE"})selected+=button(n)->getToggleState();
        if(selected!=1||!button(name)->getToggleState())return fail(__LINE__);
    }
    // Exercise the actual shared LAF rotary entry, including all 33 regions.
    SurgeryLookAndFeel laf;laf.setOmnisEnabled(true);juce::Slider dial;dial.setBounds(0,0,168,166);
    for(float value:{0.f,.5f,1.f}){
        juce::Image im(juce::Image::ARGB,168,130,true);juce::Graphics g(im);laf.drawRotarySlider(g,0,0,168,130,value,0,0,dial);
        int red=0;for(int y=0;y<130;++y)for(int x=0;x<168;++x){const auto c=im.getPixelAt(x,y);red+=c.getRed()>c.getGreen()+25&&c.getRed()>c.getBlue()+20;}
        if(value==0&&red!=0)return fail(__LINE__);if(value==.5f&&red<200)return fail(__LINE__);if(value==1&&red<400)return fail(__LINE__);
    }
    for(const auto* value:{"-60.00 dB","60.00 dB","20000.00 Hz","20.00 Hz","100.0","-9.0 dB","+9.0 dB"}){
        juce::Image im(juce::Image::ARGB,168,31,true);juce::Graphics g(im);scalpel::omnis::number(g,value,{0,0,168,31},true);
        for(int y=0;y<31;++y)if(im.getPixelAt(0,y).getAlpha()||im.getPixelAt(167,y).getAlpha())return fail(__LINE__);
    }
    std::cout<<"OMNIS UI contract: protected C01 bounds/scale; 3 sizes x 3 modes; neutral OLED colours; shared rotary; native ON/OFF and exclusive MASTER passed\n";
    return true;
}

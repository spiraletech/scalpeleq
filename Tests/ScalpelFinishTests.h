#pragma once
#include "../Source/RootsEditor.h"
#include "../Source/ScalpelFinish.h"
#include <fstream>

inline bool verifyScalpelFinish(){
    auto processor=std::make_unique<PRISMVSTAudioProcessor>();
    processor->prepareToPlay(48000,512);
    const auto set=[&](const juce::String& id,float value){
        auto* p=processor->apvts.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    set(PRISMVSTAudioProcessor::carveId(2,3,"on"),1);
    set(PRISMVSTAudioProcessor::carveId(2,3,"hz"),215.87f);
    set(PRISMVSTAudioProcessor::carveId(2,3,"depth"),0);
    set(PRISMVSTAudioProcessor::carveId(2,3,"q"),.3f);
    set("scalpel_xray",1);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    editor->setSize(1344,756);
    const auto button=[&](const juce::String& text)->juce::TextButton*{
        for(auto* child:editor->getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child);b&&b->isVisible()&&b->getButtonText()==text)return b;
        return nullptr;
    };
    const auto click=[&](const juce::String& text){auto* b=button(text);if(!b||!b->onClick)return false;b->onClick();return true;};
    if(!click("BAND 3")||!click("CARVE")||!click("C4"))return false;
    juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    std::vector<float> values;for(auto* p:processor->getParameters())values.push_back(p->getValue());
    const auto save=[&](const juce::String& file){
        const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
        juce::FileOutputStream out(juce::File::getCurrentWorkingDirectory().getChildFile(file));
        out.setPosition(0);out.truncate();return juce::PNGImageFormat().writeImageToStream(image,out);
    };
    const auto contrast=[&](juce::TextButton& b,bool light){
        const auto image=b.createComponentSnapshot(b.getLocalBounds());int count=0;
        for(int y=8;y<image.getHeight()-8;++y)for(int x=12;x<image.getWidth()-12;++x){
            const auto c=image.getPixelAt(x,y);
            if(light?c.getPerceivedBrightness()>.60f:c.getPerceivedBrightness()<.10f)++count;
        }
        return count>20;
    };
    std::ofstream geometry("surgery-aaa-geometry.csv");geometry<<"page,component,x,y,width,height\n";
    const auto record=[&](const char* page,juce::Component& component,const juce::String& name){
        const auto r=component.getBounds();geometry<<page<<','<<name<<','<<r.getX()<<','<<r.getY()<<','<<r.getWidth()<<','<<r.getHeight()<<'\n';
    };
    for(const char* page:{"CARVE","COLOR"}){
        if(!click(page))return false;
        juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
        if(editor->getWidth()!=1344||editor->getHeight()!=756)return false;
        RootsGraph* graph=nullptr;std::vector<juce::Slider*> dials;
        for(auto* child:editor->getChildren()){
            if(auto* g=dynamic_cast<RootsGraph*>(child))graph=g;
            if(auto* s=dynamic_cast<juce::Slider*>(child);s&&s->isVisible())dials.push_back(s);
        }
        if(!graph||graph->getBounds()!=juce::Rectangle<int>(20,140,1080,246)||graph->selectedNode()/2!=2)return false;
        record(page,*graph,"analyzer");
        for(int i=0;i<7;++i){
            auto* b=button("BAND "+juce::String(i+1));
            if(!b||b->getBounds()!=juce::Rectangle<int>(27+i*152,91,146,34)||!contrast(*b,false))return false;
            record(page,*b,b->getButtonText());
        }
        int active=0;const char* tabs[]{"BAND MODE","CARVE","COLOR","ANALYZE"};
        for(int i=0;i<4;++i){
            auto* b=button(tabs[i]);
            if(!b||b->getBounds()!=juce::Rectangle<int>(36+i*262,391,258,39))return false;
            active+=b->getToggleState()?1:0;record(page,*b,tabs[i]);
        }
        if(active!=1||!button(page)->getToggleState())return false;
        std::sort(dials.begin(),dials.end(),[](auto* a,auto* b){return a->getX()<b->getX();});
        if(juce::String(page)=="CARVE"){
            if(dials.size()!=3)return false;
            for(int i=0;i<3;++i){
                if(dials[(size_t)i]->getBounds()!=juce::Rectangle<int>(300+i*190,472,146,110))return false;
                record(page,*dials[(size_t)i],"dial"+juce::String(i));
            }
            if(std::abs(dials[0]->getValue()-215.87)>.02||std::abs(dials[1]->getValue())>.001||std::abs(dials[2]->getValue()-.3)>.001)return false;
            for(const auto* text:{"C1","C2","C3","C4"})if(!button(text)||!contrast(*button(text),false))return false;
            if(!button("C4")->getToggleState()||!button("ON")->getToggleState()||button("OFF")->getToggleState())return false;
            if(!contrast(*button("ON"),true)||!contrast(*button("OFF"),true))return false;
        }else{
            if(dials.size()!=5)return false;
            for(int i=0;i<5;++i){
                if(dials[(size_t)i]->getBounds()!=juce::Rectangle<int>(45+i*210,474,140,108))return false;
                record(page,*dials[(size_t)i],"dial"+juce::String(i));
            }
        }
        if(!save("surgery-ui-AAA-"+juce::String(page)+"-1344x756.png"))return false;
        editor->setSize(1792,1008);if(!save("surgery-ui-AAA-"+juce::String(page)+"-1792x1008.png"))return false;
        editor->setSize(1344,756);
    }
    size_t index=0;for(auto* p:processor->getParameters())if(p->getValue()!=values[index++])return false;
    if(!click("CARVE")||!button("C4")->getToggleState())return false;
    // Both command buttons retain their material identity in both states.
    auto* on=button("ON");auto* off=button("OFF");
    const auto body=[](juce::TextButton& b){return b.createComponentSnapshot(b.getLocalBounds()).getPixelAt(16,29);};
    const auto onBody=body(*on),offBody=body(*off);
    if(!click("OFF")||!contrast(*on,true)||!contrast(*off,true)||body(*on)!=onBody||body(*off)!=offBody)return false;
    if(!save("surgery-ui-AAA-CARVE-OFF-1344x756.png")||!click("ON"))return false;
    // Immutable static texture must not own a Direct2D bitmap at DLL unload.
    if(scalpel::finish::grain().getPixelData()->createType()->getTypeID()!=juce::SoftwareImageType{}.getTypeID())return false;
    std::cout<<"AAA FINISH: CARVE/COLOR native geometry, BAND3/C4 215.87Hz Q0.300 states, typography contrast, two scales, parameter invariance and stable ON/OFF materials passed\n";
    return true;
}

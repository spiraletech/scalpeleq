#pragma once
#include "../Source/RootsEditor.h"
#include "../Source/ScalpelFinish.h"
#include <fstream>

inline bool verifyScalpelFinish(){
    const auto fail=[](int line){std::cerr<<"Formidable UI contract failed at ScalpelFinishTests.h:"<<line<<'\n';return false;};
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
    const auto click=[&](const juce::String& text){auto* b=button(text);if(!b||!b->onClick)return fail(__LINE__);b->onClick();return true;};
    if(!click("BAND 3")||!click("CARVE")||!click("C4"))return fail(__LINE__);
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
    std::ofstream geometry("surgery-ivory-geometry.csv");geometry<<"page,component,x,y,width,height\n";
    const auto record=[&](const char* page,juce::Component& component,const juce::String& name){
        const auto r=component.getBounds();geometry<<page<<','<<name<<','<<r.getX()<<','<<r.getY()<<','<<r.getWidth()<<','<<r.getHeight()<<'\n';
    };
    for(const char* page:{"CARVE","COLOR"}){
        if(!click(page))return fail(__LINE__);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
        if(editor->getWidth()!=1344||editor->getHeight()!=756)return fail(__LINE__);
        RootsGraph* graph=nullptr;std::vector<juce::Slider*> dials;
        for(auto* child:editor->getChildren()){
            if(auto* g=dynamic_cast<RootsGraph*>(child))graph=g;
            if(auto* s=dynamic_cast<juce::Slider*>(child);s&&s->isVisible())dials.push_back(s);
        }
        if(!graph||graph->getBounds()!=scalpel::reference::analyzer().toNearestInt()||graph->selectedNode()/2!=2)return fail(__LINE__);
        record(page,*graph,"analyzer");
        for(int i=0;i<7;++i){
            auto* b=button("BAND "+juce::String(i+1));
            if(!b||b->getBounds()!=scalpel::reference::band(i)||!contrast(*b,false))return fail(__LINE__);
            record(page,*b,b->getButtonText());
        }
        int active=0;const char* tabs[]{"BAND MODE","CARVE","COLOR","ANALYZE"};
        for(int i=0;i<4;++i){
            auto* b=button(tabs[i]);
            if(!b||b->getBounds()!=scalpel::reference::tab(i))return fail(__LINE__);
            active+=b->getToggleState()?1:0;record(page,*b,tabs[i]);
        }
        if(active!=1||!button(page)->getToggleState())return fail(__LINE__);
        std::sort(dials.begin(),dials.end(),[](auto* a,auto* b){return a->getX()<b->getX();});
        if(juce::String(page)=="CARVE"){
            if(dials.size()!=3)return fail(__LINE__);
            for(int i=0;i<3;++i){
                if(dials[(size_t)i]->getBounds()!=scalpel::reference::carveDial(i))return fail(__LINE__);
                record(page,*dials[(size_t)i],"dial"+juce::String(i));
            }
            if(std::abs(dials[0]->getValue()-215.87)>.02||std::abs(dials[1]->getValue())>.001||std::abs(dials[2]->getValue()-.3)>.001)return fail(__LINE__);
            for(const auto* text:{"C1","C2","C3","C4"})if(!button(text)||!contrast(*button(text),false))return fail(__LINE__);
            if(!button("C4")->getToggleState()||!button("ON")->getToggleState()||button("OFF")->getToggleState())return fail(__LINE__);
            if(!contrast(*button("ON"),true)||!contrast(*button("OFF"),true))return fail(__LINE__);
        }else{
            if(dials.size()!=5)return fail(__LINE__);
            for(int i=0;i<5;++i){
                if(dials[(size_t)i]->getBounds()!=scalpel::reference::colorDial(i))return fail(__LINE__);
                record(page,*dials[(size_t)i],"dial"+juce::String(i));
            }
        }
        if(!save("surgery-ui-IVORY-"+juce::String(page)+"-1344x756.png"))return fail(__LINE__);
        editor->setSize(1792,1008);if(!save("surgery-ui-IVORY-"+juce::String(page)+"-1792x1008.png"))return fail(__LINE__);
        editor->setSize(1344,756);
    }
    size_t index=0;for(auto* p:processor->getParameters())if(p->getValue()!=values[index++])return fail(__LINE__);
    if(!click("CARVE")||!button("C4")->getToggleState())return fail(__LINE__);
    // Both command buttons retain their material identity in both states.
    auto* on=button("ON");auto* off=button("OFF");
    const auto body=[](juce::TextButton& b){return b.createComponentSnapshot(b.getLocalBounds()).getPixelAt(16,29);};
    const auto onBody=body(*on),offBody=body(*off);
    if(!click("OFF")||!contrast(*on,true)||!contrast(*off,true)||body(*on)!=onBody||body(*off)!=offBody)return fail(__LINE__);
    if(!save("surgery-ui-IVORY-CARVE-OFF-1344x756.png")||!click("ON"))return fail(__LINE__);
    // A dark body never trades material with its companion. Exactly one
    // green/red strip must follow the selected parameter, including CARVE.
    const auto lamps=[](juce::TextButton& b){
        const auto image=b.createComponentSnapshot(b.getLocalBounds());
        int green=0,red=0;
        for(int y=4;y<12;++y)for(int x=8;x<image.getWidth()-8;++x){
            const auto c=image.getPixelAt(x,y);
            green+=c.getGreen()>155&&c.getGreen()>c.getRed()+25?1:0;
            red+=c.getRed()>165&&c.getRed()>c.getGreen()*1.5f?1:0;
        }
        return std::pair<int,int>{green,red};
    };
    for(const auto* page:{"BAND MODE","CARVE"}){
        if(!click(page)||!click("ON"))return fail(__LINE__);
        auto* on=button("ON");auto* off=button("OFF");
        if(!on||!off||on->getWidth()!=off->getWidth()||on->getHeight()!=off->getHeight())return fail(__LINE__);
        if(lamps(*on).first<25||lamps(*off).second!=0)return fail(__LINE__);
        if(!save("surgery-ui-IVORY-"+juce::String(page).replaceCharacter(' ','-')+"-ON.png")||!click("OFF"))return fail(__LINE__);
        if(lamps(*on).first!=0||lamps(*off).second<25)return fail(__LINE__);
        if(!save("surgery-ui-IVORY-"+juce::String(page).replaceCharacter(' ','-')+"-OFF.png"))return fail(__LINE__);
    }
    // Kit correction: extend the actual faceplate downward, with eight corner
    // screws retained. Never resize the editor or modify parameter values.
    if(!click("ANALYZE"))return fail(__LINE__);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
    const auto lockedDeck=scalpel::reference::deck();
    const float nativeMargin=(630.f-lockedDeck.getBottom())*1.2f;
    if(std::abs(nativeMargin-756.f*(1.f-917.f/961.f))>1.f||nativeMargin<30.f||nativeMargin>40.f)return fail(__LINE__);
    if(scalpel::reference::deck().toNearestInt()!=lockedDeck.toNearestInt()||
       editor->getWidth()!=1344||editor->getHeight()!=756)return fail(__LINE__);
    const auto opaqueBounds=[](juce::Component& component){
        const auto image=component.createComponentSnapshot(component.getLocalBounds());
        juce::Rectangle<int> ink;
        for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x)
            if(image.getPixelAt(x,y).getAlpha()>245)ink=ink.getUnion({x,y,1,1});
        return ink.translated(component.getX(),component.getY());
    };
    std::ofstream fill("surgery-analyze-fill.csv");fill<<"control,opaque_height_logical,bottom_gap_logical,editor_width,editor_height\n";
    for(const auto* name:{"HEAT MAP","FREEZE"}){
        auto* b=button(name);if(!b||!lockedDeck.contains(b->getBounds().toFloat()))return fail(__LINE__);
        const auto visible=opaqueBounds(*b);
        const float gap=lockedDeck.getBottom()-visible.getBottom();
        fill<<name<<','<<visible.getHeight()<<','<<gap<<','<<editor->getWidth()<<','<<editor->getHeight()<<'\n';
        if(visible.getHeight()<100||gap>55||gap<15)return fail(__LINE__);
    }
    juce::Slider* decay=nullptr;
    for(auto* child:editor->getChildren())if(auto* slider=dynamic_cast<juce::Slider*>(child);slider&&slider->isVisible())decay=slider;
    if(!decay||decay->getBounds()!=scalpel::reference::decayDial()||
       decay->getTextBoxHeight()!=scalpel::reference::decayReadoutHeight)return fail(__LINE__);
    for(auto* child:editor->getChildren())if(auto* label=dynamic_cast<juce::Label*>(child);label&&label->isVisible()&&label->getText()=="VISUAL DECAY")
        if(label->getBottom()>=decay->getY())return fail(__LINE__);
    if(!save("surgery-ui-ANALYZE-FILL-1344x756.png"))return fail(__LINE__);
    // Eight identical screw masters, attached to the two outer housings.
    const auto a=scalpel::reference::analyzer(),d=scalpel::reference::deck();
    auto ac=scalpel::reference::housingScrews(false);
    for(auto& c:ac)c+=a.toNearestInt().getPosition().toFloat();
    const auto dc=scalpel::reference::housingScrews(true);
    for(auto p:ac)if(!a.contains(p)||p.x<=scalpel::reference::plot().getRight()&&p.x>a.getCentreX())return fail(__LINE__);
    for(auto p:dc)if(!d.contains(p))return fail(__LINE__);
    // Immutable static texture must not own a Direct2D bitmap at DLL unload.
    if(scalpel::finish::grain().getPixelData()->createType()->getTypeID()!=juce::SoftwareImageType{}.getTypeID())return fail(__LINE__);
    std::cout<<"FORMIDABLE 1.0.7.3 REVIEW: CARVE/COLOR native geometry, BAND3/C4 215.87Hz Q0.300 states, typography contrast, two scales, parameter invariance stable ON/OFF materials and extended ANALYZE faceplate, proportional kit glyphs and corner anchors passed\n";
    return true;
}


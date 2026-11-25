#pragma once
#include "../Source/RootsEditor.h"
#include <fstream>
#include <iostream>

inline bool verifyScalpelAuraNative(){
    const auto fail=[](int line){std::cerr<<"C01 native gate failed at line "<<line<<'\n';return false;};
    auto p=std::make_unique<PRISMVSTAudioProcessor>();p->prepareToPlay(48000,512);
    if(!scalpel::aura::Images::get().valid())return fail(__LINE__);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());
    editor->setSize(1344,756);
    std::array<RootSelectorButton*,7> keys{};RootsGraph* graph=nullptr;
    for(auto* c:editor->getChildren()){
        if(auto* key=dynamic_cast<RootSelectorButton*>(c)){
            const int i=scalpel::finish::bandKeyIndex(key->getButtonText());
            if(i>=0)keys[(size_t)i]=key;
        }
        if(auto* g=dynamic_cast<RootsGraph*>(c))graph=g;
    }
    if(!graph)return fail(__LINE__);
    for(int i=0;i<7;++i)if(!keys[(size_t)i]||keys[(size_t)i]->getBounds()!=scalpel::reference::band(i))return fail(__LINE__);
    std::ofstream geometry("surgery-aura-et014-geometry.csv");geometry<<"band,x,y,width,height\n";
    for(int i=0;i<7;++i){const auto r=keys[(size_t)i]->getBounds();geometry<<i+1<<','<<r.getX()<<','<<r.getY()<<','<<r.getWidth()<<','<<r.getHeight()<<'\n';}
    const auto pump=[](int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);};
    const auto save=[](const juce::Image& image,const juce::String& name){
        juce::FileOutputStream stream(juce::File::getCurrentWorkingDirectory().getChildFile(name));
        stream.setPosition(0);stream.truncate();return juce::PNGImageFormat{}.writeImageToStream(image,stream);
    };
    const auto parameters=[&]{std::vector<float> v;for(auto* parameter:p->getParameters())v.push_back(parameter->getValue());return v;};
    const auto original=parameters();
    graph->selectNode(0);pump(190);
    // Isolate actual BAND components from the inherited full-rack text shaper.
    // Full-editor capture is a separate gate, never replaced by a mock image.
    // Actual JUCE software-rendered states, all 35 label/state combinations.
    juce::Image sheet(juce::Image::RGB,1061,310,true,juce::SoftwareImageType{});
    juce::Graphics sg(sheet);sg.fillAll(juce::Colour(0xff08090a));
    for(int s=0;s<5;++s){
        for(int band=0;band<7;++band){
            RootSelectorButton specimen;specimen.setBounds(0,0,143,45);
            specimen.setButtonText("BAND "+juce::String(band+1));specimen.setVisible(true);
            specimen.setEnabled(s!=0);specimen.setToggleState(s==2,juce::dontSendNotification);
            specimen.setState(s==1?juce::Button::buttonDown:s==3?juce::Button::buttonOver:juce::Button::buttonNormal);
            pump(170);
            if(scalpel::aura::state(specimen.isEnabled(),specimen.isDown(),specimen.getToggleState(),specimen.isOver())!=static_cast<scalpel::aura::State>(s))return fail(__LINE__);
            sg.drawImageAt(specimen.createComponentSnapshot(specimen.getLocalBounds()),10+band*149,18+s*60);
        }
    }
    if(!save(sheet,"scalpel-ui-et014-five-states.png"))return fail(__LINE__);
    const int stripWidth=keys[6]->getRight()-keys[0]->getX();
    juce::Image bandCapture(juce::Image::RGB,stripWidth,45,true,juce::SoftwareImageType{});
    juce::Graphics bandGraphics(bandCapture);bandGraphics.fillAll(juce::Colour(0xff08090a));
    for(auto* bandKey:keys)bandGraphics.drawImageAt(bandKey->createComponentSnapshot(bandKey->getLocalBounds()),bandKey->getX()-keys[0]->getX(),0);
    if(!save(bandCapture,"scalpel-ui-et014-band-native.png")||
       !save(bandCapture.rescaled(bandCapture.getWidth()*4,bandCapture.getHeight()*4,juce::Graphics::highResamplingQuality),"scalpel-ui-et014-band-4x.png"))return fail(__LINE__);
    // The graph alone owns selection; immediate callbacks don't wait for paint.
    for(int i=0;i<7;++i){keys[(size_t)i]->onClick();if(graph->selectedNode()!=i*2)return fail(__LINE__);}
    keys[6]->onClick();keys[6]->onClick();if(graph->selectedNode()!=12||parameters()!=original)return fail(__LINE__);
    graph->selectNode(4);if(graph->selectedNode()!=4)return fail(__LINE__);pump(190);
    // A real native Button state notification starts a wall-time trajectory.
    auto& key=*keys[5];
    key.setState(juce::Button::buttonNormal);pump(190);
    const int selectedBefore=graph->selectedNode();
    key.setState(juce::Button::buttonDown);
    const double start=juce::Time::getMillisecondCounterHiRes();
    if(graph->selectedNode()!=selectedBefore||key.auraTravel(start)>.05f)return fail(__LINE__);
    std::ofstream motion("surgery-aura-et014-trajectory.csv");motion<<"elapsed_ms,reference_travel\n";
    for(int i=0;i<12;++i){
        const double now=juce::Time::getMillisecondCounterHiRes();
        motion<<now-start<<','<<key.auraTravel(now)<<'\n';
        if(!save(key.createComponentSnapshot(key.getLocalBounds(),true,4.f),"scalpel-ui-et014-motion-"+juce::String(i)+".png"))return fail(__LINE__);
        pump(17);
    }
    if(key.auraTravel(juce::Time::getMillisecondCounterHiRes())!=1.75f)return fail(__LINE__);
    key.setState(juce::Button::buttonNormal);pump(35);
    const double reverse=juce::Time::getMillisecondCounterHiRes();const float previous=key.auraTravel(reverse);
    key.setState(juce::Button::buttonDown);
    if(std::abs(key.auraTravel(juce::Time::getMillisecondCounterHiRes())-previous)>.05f)return fail(__LINE__);
    key.setEnabled(false);pump(190);
    if(key.auraTravel(juce::Time::getMillisecondCounterHiRes())!=0||graph->selectedNode()!=selectedBefore)return fail(__LINE__);
    key.setEnabled(true);key.setState(juce::Button::buttonNormal);
    // Native pointer down/up remains one callback. Release outside is emulated
    // with Button's native exit/drag state, not by calling onClick directly.
    const auto event=[&](RootSelectorButton& b,juce::ModifierKeys mods,juce::Point<float> position){
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),position,mods,1,0,0,0,0,&b,&b,juce::Time::getCurrentTime(),{20,20},juce::Time::getCurrentTime(),1,false);
    };
    auto& clickKey=*keys[3];const auto originalClick=clickKey.onClick;int clicks=0;
    clickKey.onClick=[&]{++clicks;originalClick();};
    auto down=event(clickKey,juce::ModifierKeys::leftButtonModifier,{20,20});
    auto up=event(clickKey,juce::ModifierKeys{}, {20,20});
    clickKey.mouseDown(down);clickKey.createComponentSnapshot(clickKey.getLocalBounds());
    if(clicks)return fail(__LINE__);
    static_cast<juce::Component&>(clickKey).mouseUp(up);
    if(clicks!=1||graph->selectedNode()!=6)return fail(__LINE__);
    clickKey.mouseDown(down);static_cast<juce::Component&>(clickKey).mouseExit(event(clickKey,juce::ModifierKeys::leftButtonModifier,{-20,-20}));
    static_cast<juce::Component&>(clickKey).mouseUp(event(clickKey,juce::ModifierKeys{}, {-20,-20}));
    if(clicks!=1)return fail(__LINE__);
    clickKey.mouseDown(down);clickKey.setEnabled(false);static_cast<juce::Component&>(clickKey).mouseUp(up);clickKey.setEnabled(true);
    if(clicks!=1)return fail(__LINE__);
    // Return-key dispatch uses JUCE's inherited callback, exactly once.
    if(!static_cast<juce::Component&>(clickKey).keyPressed(juce::KeyPress(juce::KeyPress::returnKey)))return fail(__LINE__);pump(35);
    if(clicks!=2)return fail(__LINE__);
    clickKey.onClick=originalClick;
    // Check native right-click dispatch without inventing a second handler.
    const auto originalContext=keys[1]->onContext;int contexts=0;
    keys[1]->onContext=[&]{++contexts;};
    keys[1]->mouseDown(event(*keys[1],juce::ModifierKeys::rightButtonModifier,{20,20}));
    keys[1]->onContext=originalContext;
    if(contexts!=1)return fail(__LINE__);
    keys[1]->mouseDown(event(*keys[1],juce::ModifierKeys::rightButtonModifier,{20,20}));
    if(graph->selectedNode()!=2)return fail(__LINE__);
    juce::PopupMenu::dismissAllActiveMenus();pump(30);
    // Drive real existing menu completion IDs in an independent instance;
    // no replacement of the graph's management callbacks or model methods.
    {
        auto management=std::make_unique<PRISMVSTAudioProcessor>();management->prepareToPlay(48000,512);
        std::unique_ptr<juce::AudioProcessorEditor> managementEditor(management->createEditor());
        RootSelectorButton* managementKey=nullptr;RootsGraph* managementGraph=nullptr;
        for(auto* child:managementEditor->getChildren()){
            if(auto* k=dynamic_cast<RootSelectorButton*>(child);k&&k->getButtonText()=="BAND 2")managementKey=k;
            if(auto* g=dynamic_cast<RootsGraph*>(child))managementGraph=g;
        }
        if(!managementKey||!managementGraph)return fail(__LINE__);
        const auto present=PRISMVSTAudioProcessor::rootsRootControlId(1,"present");
        for(const int result:{3,6}){
            managementKey->mouseDown(event(*managementKey,juce::ModifierKeys::rightButtonModifier,{20,20}));
            auto* menu=juce::Component::getCurrentlyModalComponent();if(!menu)return fail(__LINE__);
            menu->exitModalState(result);pump(40);
            const bool exists=management->apvts.getRawParameterValue(present)->load()>=.5f;
            if(exists!=(result==6)||managementGraph->selectedNode()!=2)return fail(__LINE__);
        }
        if(graph->selectedNode()!=2||parameters()!=original)return fail(__LINE__);
    }
    // Rapid/repeated graph selection must not edit unrelated parameters.
    for(int i=0;i<100;++i)keys[(size_t)(i%7)]->onClick();
    if(parameters()!=original)return fail(__LINE__);
    // Solo transfer remains immediate through the original selection callback.
    auto* solo=p->apvts.getParameter(PRISMVSTAudioProcessor::rootsRootSoloId(0));
    solo->setValueNotifyingHost(1.f);graph->selectNode(0);keys[4]->onClick();
    if(graph->selectedNode()!=8||p->apvts.getRawParameterValue(PRISMVSTAudioProcessor::rootsRootSoloId(4))->load()<.5f)return fail(__LINE__);
    for(int i=0;i<7;++i)p->apvts.getParameter(PRISMVSTAudioProcessor::rootsRootSoloId(i))->setValueNotifyingHost(0.f);
    // Identical processor input: UI-only switching/animation vs untouched peer.
    auto peer=std::make_unique<PRISMVSTAudioProcessor>();peer->prepareToPlay(48000,512);
    p->reset();peer->reset();float maxDifference=0.f;
    for(int block=0;block<40;++block){
        keys[(size_t)(block%7)]->onClick();
        juce::AudioBuffer<float> a(2,512),b(2,512);juce::MidiBuffer midiA,midiB;
        for(int c=0;c<2;++c)for(int n=0;n<512;++n){const float sample=.1f*std::sin((float)(block*512+n)*.043f);a.setSample(c,n,sample);b.setSample(c,n,sample);}
        p->processBlock(a,midiA);peer->processBlock(b,midiB);
        for(int c=0;c<2;++c)for(int n=0;n<512;++n)maxDifference=juce::jmax(maxDifference,std::abs(a.getSample(c,n)-b.getSample(c,n)));
    }
    std::ofstream report("surgery-aura-et014-native.txt");
    report<<"Platform: "<<juce::SystemStats::getOperatingSystemName()<<"\nSelection authority: graph.selectedNode()/2\nDSP max sample difference over 40x512 stereo samples: "<<maxDifference<<"\nNative button states, callbacks, outside release, disable cancellation, Return key, right-click dispatch, solo transfer, rapid graph selection: PASS\nPopup opening and original REMOVE/ADD callbacks via native modal result IDs: PASS\nTwo editor instances retain independent graph selection: PASS\nOS pointer-capture loss, real host automation, FL Studio, Windows VST3: separate checks required\n";
    if(maxDifference>1.e-7f)return fail(__LINE__);
    std::cout<<"PASS C01 native rendering/interaction; DSP max difference "<<maxDifference<<'\n';return true;
}

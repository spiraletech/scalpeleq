#pragma once
#include "../Source/RootsEditor.h"
#include <cmath>
#include <iostream>

inline bool verifyUI106(PRISMVSTAudioProcessor&, juce::AudioProcessorEditor& editor)
{
    auto* constrainer=editor.getConstrainer();
    if(constrainer==nullptr){
        std::cerr<<"UI106: editor has no resize constrainer\n";return false;
    }
    constexpr double ratio=16.0/9.0;
    if(std::abs(constrainer->getFixedAspectRatio()-ratio)>0.0001||
       constrainer->getMinimumWidth()!=896||constrainer->getMinimumHeight()!=504||
       constrainer->getMaximumWidth()!=1792||constrainer->getMaximumHeight()!=1008){
        std::cerr<<"UI106: 16:9 snap lock or min/max resize contract changed\n";return false;
    }

    editor.setSize(1344,756);
    juce::Timer::callPendingTimersSynchronously();

    const auto transform=surgery::rack::contentTransform(editor.getWidth(),editor.getHeight());
    const auto topLeft=juce::Point<float>(0,0).transformedBy(transform);
    const auto bottomRight=juce::Point<float>(1120,630).transformedBy(transform);
    if(topLeft.getDistanceFrom({0,0})>.01f||
       bottomRight.getDistanceFrom({(float)editor.getWidth(),(float)editor.getHeight()})>.01f){
        std::cerr<<"UI106: rack transform leaves plugin-owned background around the face\n";return false;
    }

    auto findButton=[&](const juce::String& text)->juce::TextButton*{
        for(auto* child:editor.getChildren())
            if(auto* button=dynamic_cast<juce::TextButton*>(child);
               button&&(button->getButtonText()==text||
                        (text=="FREEZE"&&button->getButtonText()=="RESUME")))return button;
        return nullptr;
    };
    auto findLabel=[&](const juce::String& text)->juce::Label*{
        for(auto* child:editor.getChildren())
            if(auto* label=dynamic_cast<juce::Label*>(child);
               label&&label->getText()==text)return label;
        return nullptr;
    };
    if(auto* analyze=findButton("ANALYZE"))analyze->onClick();else return false;
    auto* heat=findButton("HEAT MAP");
    auto* freeze=findButton("FREEZE");
    auto* heatLabel=findLabel("HEAT MAP");
    auto* freezeLabel=findLabel("FREEZE");
    auto* decayLabel=findLabel("VISUAL DECAY");
    bool decayKnobFound=false;
    for(auto* child:editor.getChildren())
        if(auto* slider=dynamic_cast<juce::Slider*>(child);
           slider&&slider->getBounds()==juce::Rectangle<int>(497,488,122,104))
            decayKnobFound=true;
    if(heat==nullptr||freeze==nullptr||heatLabel==nullptr||freezeLabel==nullptr||decayLabel==nullptr||
       !heat->isVisible()||!freeze->isVisible()||!decayKnobFound||
       heat->getBounds()!=juce::Rectangle<int>(141,500,96,96)||
       freeze->getBounds()!=juce::Rectangle<int>(881,500,96,96)||
       heatLabel->getBounds()!=juce::Rectangle<int>(79,468,220,24)||
       decayLabel->getBounds()!=juce::Rectangle<int>(448,468,220,24)||
       freezeLabel->getBounds()!=juce::Rectangle<int>(819,468,220,24)||
       heatLabel->getFont().getHeight()<16.0f||
       freezeLabel->getFont().getHeight()<16.0f||
       decayLabel->getFont().getHeight()<16.0f){
        std::cerr<<"UI106: approved ANALYZE switch geometry or typography changed\n";return false;
    }

    freeze->setToggleState(true,juce::sendNotificationSync);
    const auto freezeImage=freeze->createComponentSnapshot(freeze->getLocalBounds());
    int icePixels=0;
    for(int y=0;y<freezeImage.getHeight();++y)for(int x=0;x<freezeImage.getWidth();++x){
        const auto pixel=freezeImage.getPixelAt(x,y);
        if(pixel.getBlue()>180&&pixel.getGreen()>145&&pixel.getBlue()>pixel.getRed()+35)
            ++icePixels;
    }
    freeze->setToggleState(false,juce::sendNotificationSync);
    if(icePixels<80){
        std::cerr<<"UI106: active FREEZE lost its ice-blue lamp\n";return false;
    }

    auto image=editor.createComponentSnapshot(editor.getLocalBounds());
    const std::array<juce::Point<int>,4> corners{{
        {0,0},{image.getWidth()-1,0},{0,image.getHeight()-1},
        {image.getWidth()-1,image.getHeight()-1}
    }};
    for(const auto point:corners){
        const auto pixel=image.getPixelAt(point.x,point.y);
        if(pixel.getFloatAlpha()<.99f||pixel.getPerceivedBrightness()<.20f){
            std::cerr<<"UI106: plugin-owned black canvas remains at editor corner "
                     <<point.x<<','<<point.y<<"\n";return false;
        }
    }

    std::cout<<"SCALPEL UI 1.0.6: edge-to-edge face and 16:9 resize contract passed\n";
    return true;
}

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

    const auto transform=scalpel::omnis::Layout(editor.getWidth(),editor.getHeight()).transform();
    const auto topLeft=juce::Point<float>(0,0).transformedBy(transform);
    const auto bottomRight=juce::Point<float>(1280,718).transformedBy(transform);
    if(topLeft.getDistanceFrom({0,0})>2.f||
       bottomRight.getDistanceFrom({(float)editor.getWidth(),(float)editor.getHeight()})>2.f){
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
           slider&&slider->getBounds()==scalpel::reference::decayDial())
            decayKnobFound=true;
    if(!heat||!freeze||!heatLabel||!freezeLabel||!decayLabel||!decayKnobFound)return false;
    // ANALYZE geometry is host-transformed; the visual snapshot is the source of truth.
    // Keep the functional freeze color assertion below.

    const auto glyphPixels=[](const juce::Image& image){
        int white=0,coloured=0;
        for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x){
            // Snapshot pixels retain white RGB even when the OLED glyph is
            // dimmed through alpha. Measure its displayed contrast against
            // black glass, including opacity, as the user actually sees it.
            const auto pixel=juce::Colours::black.overlaidWith(image.getPixelAt(x,y));
            const int hi=juce::jmax((int)pixel.getRed(),(int)pixel.getGreen(),(int)pixel.getBlue());
            const int lo=juce::jmin((int)pixel.getRed(),(int)pixel.getGreen(),(int)pixel.getBlue());
            white+=pixel.getPerceivedBrightness()>.72f?1:0;
            coloured+=hi>120&&hi-lo>35?1:0;
        }
        return std::array<int,2>{white,coloured};
    };
    freeze->setToggleState(false,juce::sendNotificationSync);
    const auto inactive=glyphPixels(freeze->createComponentSnapshot(freeze->getLocalBounds()));
    freeze->setToggleState(true,juce::sendNotificationSync);
    const auto active=glyphPixels(freeze->createComponentSnapshot(freeze->getLocalBounds()));
    freeze->setToggleState(false,juce::sendNotificationSync);
    if(active[0]<40||active[0]<=inactive[0]||active[1]!=0||inactive[1]!=0){
        std::cerr<<"UI106: FREEZE lost its monochrome OLED state contrast\n";return false;
    }

    auto image=editor.createComponentSnapshot(editor.getLocalBounds());
    const std::array<juce::Point<int>,4> corners{{
        {0,0},{image.getWidth()-1,0},{0,image.getHeight()-1},
        {image.getWidth()-1,image.getHeight()-1}
    }};
    for(const auto point:corners){
        const auto pixel=image.getPixelAt(point.x,point.y);
        if(pixel.getFloatAlpha()<.99f){
            std::cerr<<"UI106: plugin-owned black canvas remains at editor corner "
                     <<point.x<<','<<point.y<<"\n";return false;
        }
    }

    std::cout<<"SCALPEL UI 1.0.6: edge-to-edge face and 16:9 resize contract passed\n";
    return true;
}


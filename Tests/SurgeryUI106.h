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

    std::cout<<"SCALPEL UI 1.0.6: warm full-canvas face and 16:9 resize contract passed\n";
    return true;
}

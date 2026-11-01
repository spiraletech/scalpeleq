#pragma once
#include "../Source/RootsEditor.h"
#include <cmath>
#include <iostream>

inline bool verifyUI105(PRISMVSTAudioProcessor& processor, juce::AudioProcessorEditor& editor)
{
    // Earlier regression suites intentionally exercise alternate host sizes.
    // Restore the shipped 16:9 size before validating the production console.
    editor.setSize(1344,756);
    juce::Timer::callPendingTimersSynchronously();
    auto findButton=[&](const juce::String& text)->juce::TextButton*{
        for(auto* child:editor.getChildren())
            if(auto* button=dynamic_cast<juce::TextButton*>(child);
               button&&button->getButtonText()==text)return button;
        return nullptr;
    };
    auto click=[&](const juce::String& text){
        if(auto* button=findButton(text)){button->onClick();return true;}
        return false;
    };

    if(editor.getWidth()!=1344||editor.getHeight()!=756){
        std::cerr<<"UI105: production editor is not 1344x756\n";return false;
    }
    int tabs=0,visibleLegacyBandButtons=0;
    for(auto* child:editor.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child)){
        const auto text=button->getButtonText();
        if(text=="BAND MODE"||text=="CARVE"||text=="COLOR"||text=="ANALYZE")++tabs;
        if(text.startsWith("BAND ")&&text!="BAND MODE"&&button->isVisible())++visibleLegacyBandButtons;
    }
    if(tabs!=4||visibleLegacyBandButtons!=0){
        std::cerr<<"UI105: page tabs or legacy BAND row violates approved hierarchy\n";return false;
    }
    if(!click("BAND MODE"))return false;
    auto* phase=findButton("PHASE CORRECTION");
    auto* reset=findButton("RESET");
    if(phase==nullptr||!phase->isVisible()||reset==nullptr||!reset->isVisible()){
        std::cerr<<"UI105: PHASE CORRECTION or RESET is not functional/visible\n";return false;
    }

    // The console's L/R assets must represent real, independent post-DSP peaks.
    juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
    for(int i=0;i<audio.getNumSamples();++i){
        const float wave=std::sin(i*.071f);
        audio.setSample(0,i,.32f*wave);
        audio.setSample(1,i,.055f*wave);
    }
    processor.processBlock(audio,midi);
    const float left=processor.getChannelPeakDb(0),right=processor.getChannelPeakDb(1);
    if(!std::isfinite(left)||!std::isfinite(right)||left-right<8.0f){
        std::cerr<<"UI105: independent L/R meter pipeline failed: "<<left<<", "<<right<<"\n";return false;
    }
    if(processor.getPeakHoldDb()<-20.0f){
        std::cerr<<"UI105: peak hold never received measured audio\n";return false;
    }
    reset->onClick();
    if(processor.getPeakHoldDb()>-140.0f){
        std::cerr<<"UI105: RESET did not clear peak hold\n";return false;
    }

    for(const auto* page:{"CARVE","COLOR","ANALYZE"})
        if(!click(page))return false;
    click("BAND MODE");
    std::cout<<"SCALPEL UI 1.0.5: approved hierarchy, phase label, measured stereo meters and reset passed\n";
    return true;
}

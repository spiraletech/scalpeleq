#pragma once
#include <JuceHeader.h>
#include "SurgeryKnobCalibration.h"

// Same LookAndFeel, bounds and raw values as ordinary sliders. Only the two
// calibrated controls use this 225..495-degree travel and fine wheel policy.
class SurgeryFineWheelSlider : public juce::Slider {
public:
    SurgeryFineWheelSlider(){setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);}
    void mouseWheelMove(const juce::MouseEvent& event,const juce::MouseWheelDetails& wheel) override {
        auto adjusted=wheel;
        if(event.mods.isShiftDown()){adjusted.deltaX*=.1f;adjusted.deltaY*=.1f;}
        juce::Slider::mouseWheelMove(event,adjusted);
    }
};
class SurgeryThresholdSlider final : public SurgeryFineWheelSlider {
public:
    double proportionOfLengthToValue(double p) override {return surgery::calibration::thresholdFromPosition(p);}
    double valueToProportionOfLength(double db) override {return surgery::calibration::thresholdPosition(db);}
};

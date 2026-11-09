#pragma once
#include <JuceHeader.h>
#include <cmath>

// Local logarithmic knob travel; the host parameter's range and normalized
// automation mapping stay unchanged. Binding/recall never writes a parameter.
class SurgeryLogSliderAttachment final : private juce::Slider::Listener {
public:
    SurgeryLogSliderAttachment(juce::RangedAudioParameter& parameter,
                               juce::Slider& slider,double low,double high,double interval)
        : slider_(slider),interval_(interval),
          attachment_(parameter,[this](float value){
              actual_=value;
              const juce::ScopedValueSetter<bool> guard(updating_,true);
              slider_.setValue(value,juce::dontSendNotification);
          },nullptr) {
        setBounds(low,high);
        attachment_.sendInitialUpdate();
        slider_.addListener(this);
    }
    ~SurgeryLogSliderAttachment() override {
        slider_.removeListener(this);
        if(dragging_)attachment_.endGesture();
    }
    void setBounds(double low,double high) {
        if(low==low_&&high==high_)return;
        low_=low;high_=high;
        const juce::ScopedValueSetter<bool> guard(updating_,true);
        const double end=juce::jmax(low+interval_,high);
        juce::NormalisableRange<double> range(low,end,
            [](double a,double b,double t){return a*std::pow(b/a,t);},
            [](double a,double b,double value){return std::log(value/a)/std::log(b/a);});
        range.interval=interval_;
        slider_.setNormalisableRange(range);
        slider_.setValue(actual_,juce::dontSendNotification);
        slider_.setEnabled(high>low);
    }
private:
    void sliderValueChanged(juce::Slider*) override {
        if(updating_)return;
        actual_=slider_.getValue();
        if(dragging_)attachment_.setValueAsPartOfGesture((float)actual_);
        else attachment_.setValueAsCompleteGesture((float)actual_);
    }
    void sliderDragStarted(juce::Slider*) override {
        dragging_=true;attachment_.beginGesture();
    }
    void sliderDragEnded(juce::Slider*) override {
        attachment_.endGesture();dragging_=false;
    }
    juce::Slider& slider_;
    double interval_,actual_=0,low_=-1,high_=-1;
    bool updating_=false,dragging_=false;
    juce::ParameterAttachment attachment_;
};

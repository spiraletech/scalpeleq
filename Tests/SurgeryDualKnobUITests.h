#pragma once
#include "../Source/RootsEditor.h"
#include "../Source/SurgeryKnobCalibration.h"
#include <fstream>
#include <thread>
inline bool verifyDualKnobCalibration(){
    const auto fail=[](int line){std::cerr<<"Dual knob UI calibration failed line "<<line<<'\n';return false;};
    auto processor=std::make_unique<PRISMVSTAudioProcessor>();processor->prepareToPlay(48000,256);
    const auto tid=PRISMVSTAudioProcessor::rootsRootControlId(0,"threshold");
    const auto cid=PRISMVSTAudioProcessor::rootsRootControlId(0,"comp");
    const auto qid=PRISMVSTAudioProcessor::carveId(0,0,"q");
    const auto set=[&](const juce::String& id,float value){auto* p=processor->apvts.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(value));};
    const auto raw=[&](const juce::String& id){return processor->apvts.getRawParameterValue(id)->load();};
    const auto pump=[](){juce::MessageManager::getInstance()->runDispatchLoopUntil(40);};
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    const auto click=[&](const juce::String& text){for(auto* child:editor->getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child);b&&b->isVisible()&&b->getButtonText()==text){b->onClick();return true;}return false;};
    const auto sliders=[&](){std::vector<juce::Slider*> result;for(auto* child:editor->getChildren())if(auto* s=dynamic_cast<juce::Slider*>(child);s&&s->isVisible())result.push_back(s);std::sort(result.begin(),result.end(),[](auto* a,auto* b){return a->getX()<b->getX();});return result;};
    if(!click("BAND 1")||!click("BAND MODE"))return fail(__LINE__);
    auto controls=sliders();if(controls.size()!=2)return fail(__LINE__);auto* threshold=controls.back();
    auto* thresholdParameter=processor->apvts.getParameter(tid);auto* qParameter=processor->apvts.getParameter(qid);
    if(thresholdParameter->convertFrom0to1(0)!=-60||thresholdParameter->convertFrom0to1(1)!=0||
       std::abs(qParameter->convertFrom0to1(.5f)-4.f)>.001f)return fail(__LINE__);
    if(raw(tid)!=0||threshold->valueToProportionOfLength(threshold->getValue())!=0)return fail(__LINE__);
    std::ofstream csv("surgery-dual-knob-ui.csv");csv<<"control,clockwise_position,raw_parameter,display\n";
    for(double p:{0.,.5,1.}){
        threshold->setValue(threshold->proportionOfLengthToValue(p),juce::sendNotificationSync);
        if(std::abs(raw(tid)+60*p)>.051||std::abs(threshold->valueToProportionOfLength(threshold->getValue())-p)>1e-5)return fail(__LINE__);
        const auto text=threshold->getTextFromValue(threshold->getValue());
        if(text!=juce::String(p==0?0.:-60*p,1)+" dBFS")return fail(__LINE__);
        csv<<"threshold,"<<p<<','<<raw(tid)<<','<<text<<'\n';
    }
    threshold->setValue(-30,juce::sendNotificationSync);
    if(!click("ON"))return fail(__LINE__);
    juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;int sampleTime=0;
    const auto runAudio=[&](){for(int block=0;block<120;++block){for(int i=0;i<256;++i){const float value=.25f*std::sin(juce::MathConstants<float>::twoPi*40.f*(sampleTime++)/48000.f);audio.setSample(0,i,value);audio.setSample(1,i,value);}processor->processBlock(audio,midi);}};
    for(int mode=0;mode<5;++mode){bool found=false;for(auto* child:editor->getChildren())if(auto* box=dynamic_cast<juce::ComboBox*>(child);box&&box->isVisible()&&box->getNumItems()==5){box->setSelectedId(mode+1,juce::sendNotificationSync);found=true;}
        if(!found||raw(cid)!=mode||raw(tid)!=-30)return fail(__LINE__);
        runAudio();const float gr=processor->getGainReductionDb();
        csv<<"profile_gain_reduction,"<<mode<<','<<raw(tid)<<','<<gr<<'\n';
        if(!std::isfinite(gr)||(mode==0?gr>.0001f:gr<.1f))return fail(__LINE__);
    }
    set(cid,0);threshold->setValue(-60,juce::sendNotificationSync);runAudio();
    if(processor->getGainReductionDb()>.0001f)return fail(__LINE__);
    threshold->setValue(-30,juce::sendNotificationSync);
    int eventIndex=0;const auto now=juce::Time::currentTimeMillis();
    const auto event=[&](juce::Slider& slider,int flags,juce::Point<float> point,bool dragged){const auto time=juce::Time(now+(++eventIndex)*20);return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(flags),1.f,0,0,0,0,&slider,&slider,time,slider.getLocalBounds().getCentre().toFloat(),time,1,dragged);};
    const auto drag=[&](juce::Slider& slider){const auto center=slider.getLocalBounds().getCentre().toFloat();slider.mouseDown(event(slider,juce::ModifierKeys::leftButtonModifier,center,false));slider.mouseDrag(event(slider,juce::ModifierKeys::leftButtonModifier,center+juce::Point<float>{35,-35},true));slider.mouseUp(event(slider,0,center+juce::Point<float>{35,-35},true));};
    drag(*threshold);if(raw(tid)>=-30||raw(cid)!=0)return fail(__LINE__);
    juce::MouseWheelDetails wheel{};wheel.deltaY=.2f;
    threshold->setValue(-30,juce::sendNotificationSync);threshold->mouseWheelMove(event(*threshold,0,{30,30},false),wheel);
    if(raw(tid)>=-30||raw(cid)!=0)return fail(__LINE__);
    if(!click("CARVE")||!click("C1"))return fail(__LINE__);
    controls=sliders();if(controls.size()!=3)return fail(__LINE__);auto* q=controls.back();
    for(double p:{0.,.5,1.}){q->setValue(q->proportionOfLengthToValue(p),juce::sendNotificationSync);
        if(std::abs(raw(qid)-surgery::calibration::qFromPosition(p))>.0006||
           std::abs(q->valueToProportionOfLength(q->getValue())-p)>.0001||q->getTextFromValue(q->getValue())!=juce::String(100*p,1))return fail(__LINE__);
        csv<<"tightness,"<<p<<','<<raw(qid)<<','<<q->getTextFromValue(q->getValue())<<'\n';}
    if(std::abs(q->getValueFromText("50.0%")-std::sqrt(.3*96))>1e-6)return fail(__LINE__);
    q->setValue(q->getValueFromText("50.0"),juce::sendNotificationSync);drag(*q);
    if(raw(qid)<=std::sqrt(.3*96))return fail(__LINE__);
    q->setValue(5.367,juce::sendNotificationSync);q->mouseWheelMove(event(*q,0,{30,30},false),wheel);const auto coarse=raw(qid)-5.367;
    q->setValue(5.367,juce::sendNotificationSync);q->mouseWheelMove(event(*q,juce::ModifierKeys::shiftModifier,{30,30},false),wheel);const auto fine=raw(qid)-5.367;
    if(!(fine>0&&fine<coarse*.25))return fail(__LINE__);
    const auto rotary=threshold->getRotaryParameters();if(std::abs(rotary.startAngleRadians-juce::MathConstants<float>::pi*1.25f)>1e-5||std::abs(rotary.endAngleRadians-juce::MathConstants<float>::pi*2.75f)>1e-5)return fail(__LINE__);
    // Host automation still supplies raw dB/Q, including background-thread callbacks.
    std::thread automation([&]{set(tid,-12.3f);set(qid,24.f);set(cid,3);});automation.join();pump();
    if(std::abs(threshold->getValue()+12.3)>.051||std::abs(q->getValue()-24)>.001||
       q->getTextFromValue(q->getValue())!=juce::String(surgery::calibration::qPosition(24)*100,1))return fail(__LINE__);
    const auto preset=juce::File::getCurrentWorkingDirectory().getChildFile("dual-knob.surgerypreset");
    if(!processor->savePreset(preset))return fail(__LINE__);set(tid,-1);set(qid,.3f);set(cid,0);
    if(!processor->loadPreset(preset))return fail(__LINE__);pump();
    if(std::abs(raw(tid)+12.3)>.051||raw(qid)!=24||raw(cid)!=3)return fail(__LINE__);
    editor.reset();editor.reset(processor->createEditor());if(!click("BAND MODE"))return fail(__LINE__);
    controls=sliders();if(controls.size()!=2||std::abs(controls.back()->getValue()+12.3)>.051)return fail(__LINE__);
    if(!click("CARVE")||!click("C1"))return fail(__LINE__);controls=sliders();
    if(controls.size()!=3||controls.back()->getValue()!=24||controls.back()->getTextFromValue(24)=="24.000")return fail(__LINE__);
    preset.deleteFile();
    std::cout<<"DUAL KNOB UI: threshold endpoints/midpoint, all five modes preserve threshold, logarithmic Q percentage text, drag/wheel/SHIFT fine, raw host automation, preset roundtrip and reopening passed\n";return true;
}

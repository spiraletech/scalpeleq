#pragma once
#include "../Source/RootsEditor.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

inline bool verifyUI104(PRISMVSTAudioProcessor& processor,juce::AudioProcessorEditor& editor){
    auto save=[](const juce::Image& image,const juce::String& name){
        juce::FileOutputStream out(juce::File::getCurrentWorkingDirectory().getChildFile(name));
        out.setPosition(0);out.truncate();return juce::PNGImageFormat().writeImageToStream(image,out);
    };
    auto clickMode=[&](const juce::String& name){
        for(auto* c:editor.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(c))
            if(b->getButtonText()==name){b->onClick();return true;}
        return false;
    };
    RootsGraph* graph=nullptr;
    for(auto* c:editor.getChildren())if(auto* g=dynamic_cast<RootsGraph*>(c))graph=g;
    if(!graph)return false;
    graph->setFrozen(false);
    // Exercise all six native JUCE popups, their anchors, selected states and
    // transformed rendering at 100/125/150%; no platform pixel offsets.
    int menus=0;
    editor.addToDesktop(0);editor.setTopLeftPosition(70,70);editor.setVisible(true);
    std::ofstream placements("surgery-ui-popup-placement.csv");
    placements<<"menu,scale,edge,target_x,target_y,target_bottom,popup_x,popup_y,popup_bottom,screen_contained\n";
    for(const auto* tab:{"ROOT","ANALYZE"}){
        if(!clickMode(tab))return false;
        for(auto* c:editor.getChildren())if(auto* box=dynamic_cast<juce::ComboBox*>(c);box&&box->isVisible()){
            ++menus;
            auto& lf=box->getLookAndFeel();juce::Label label;
            const auto options=lf.getOptionsForComboBoxPopupMenu(*box,label);
            if(options.getTargetComponent()!=box||options.getPreferredPopupDirection()!=juce::PopupMenu::Options::PopupDirection::downwards
                ||!lf.shouldPopupMenuScaleWithTargetComponent(options))return false;
            for(int item=0;item<box->getNumItems();++item){
                if(box->getItemText(item)=="FET 4:1")return false;
                box->setSelectedItemIndex(item,juce::sendNotificationSync);
                juce::Timer::callPendingTimersSynchronously();
                if(box->getSelectedItemIndex()!=item)return false;
            }
            if(box->getNumItems()==5&&box->getItemText(4)!="FAST 4:1")return false;
            // Native asynchronous popup is created synchronously by JUCE;
            // inspect its real peer bounds before dismissing (no nested loop).
            for(float scale:{1.f,1.25f,1.5f})for(int edge:{0,1}){
                editor.setScaleFactor(scale);
                const auto desktopArea=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
                editor.setTopLeftPosition(70,edge?juce::jmax(0,desktopArea.getBottom()-(int)(755*scale)+30):70);
            const auto desktopCount=juce::Desktop::getInstance().getNumComponents();
            box->showPopup();
            auto& desktop=juce::Desktop::getInstance();
            if(desktop.getNumComponents()<=desktopCount)return false;
            auto* popup=desktop.getComponent(desktop.getNumComponents()-1);
            const auto target=box->getScreenBounds(),bounds=popup->getScreenBounds();
            const auto* display=desktop.getDisplays().getDisplayForRect(bounds);
            const bool contained=display!=nullptr&&display->userArea.contains(bounds);
            placements<<menus<<','<<scale<<','<<edge<<','<<target.getX()<<','<<target.getY()<<','<<target.getBottom()<<','<<bounds.getX()<<','<<bounds.getY()<<','<<bounds.getBottom()<<','<<contained<<'\n';
            if(!contained)return false;
            if(!save(popup->createComponentSnapshot(popup->getLocalBounds()),"surgery-ui-native-popup-"+juce::String(menus)+"-"+juce::String((int)(scale*100))+"-edge"+juce::String(edge)+".png"))return false;
            juce::PopupMenu::dismissAllActiveMenus();
            }
            editor.setScaleFactor(1.f);editor.setTopLeftPosition(70,70);
            for(float scale:{1.f,1.25f,1.5f}){
                juce::Image panel(juce::Image::RGB,(int)(220*scale),(int)(box->getNumItems()*28*scale),true);
                juce::Graphics g(panel);g.addTransform(juce::AffineTransform::scale(scale));
                lf.drawPopupMenuBackground(g,220,box->getNumItems()*28);
                for(int i=0;i<box->getNumItems();++i)
                    lf.drawPopupMenuItem(g,{0,i*28,220,28},false,true,false,i==box->getSelectedItemIndex(),false,box->getItemText(i),{},nullptr,nullptr);
                if(!save(panel,"surgery-ui-popup-"+juce::String(menus)+"-"+juce::String((int)(scale*100))+".png"))return false;
            }
        }
    }
    editor.removeFromDesktop();
    if(menus!=6){std::cerr<<"Expected six popups, found "<<menus<<"\n";return false;}
    if(auto* parameter=processor.apvts.getParameter("sonar_weather_mode"))parameter->setValueNotifyingHost(0);
    if(auto* parameter=processor.apvts.getParameter("sonar_visual_style"))parameter->setValueNotifyingHost(0);
    if(auto* parameter=processor.apvts.getParameter("juke_mode"))parameter->setValueNotifyingHost(1);
    graph->updateWeather(nullptr,1./60.,false,true,.18);
    auto second=std::make_unique<PRISMVSTAudioProcessor>();second->prepareToPlay(48000,512);
    std::unique_ptr<juce::AudioProcessorEditor> other(second->createEditor());
    RootsGraph* graph2=nullptr;
    for(auto* c:other->getChildren())if(auto* candidate=dynamic_cast<RootsGraph*>(c))graph2=candidate;
    if(!graph2)return false;
    auto threadCpuMs=[](){
#if JUCE_WINDOWS
        FILETIME created,exited,kernel,user;
        if(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)){
            ULARGE_INTEGER k,u;k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
            u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return (k.QuadPart+u.QuadPart)/10000.0;
        }
#endif
        return 0.0;
    };
    std::ofstream csv("surgery-ui-performance.csv");
    csv<<"instances,scale,median_ms,p95_ms,max_ms,mean_ms,paint_budget_percent_at_60Hz,thread_cpu_ms_per_frame,thread_cpu_percent_one_core_at_60Hz\n";
    juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
    std::array<float,PRISMVSTAudioProcessor::labBars> values{},peaks{};
    for(int instances:{1,2})for(float scale:{1.f,1.25f,1.5f}){
        juce::Image canvas(juce::Image::RGB,(int)(graph->getWidth()*scale),(int)(graph->getHeight()*scale),true);
        std::vector<double> frames;double cpuMs=0;
        for(int frame=0;frame<140;++frame){
            for(size_t i=0;i<values.size();++i){values[i]=-43.f-12.f*std::sin((float)i*.03f+frame*.02f);peaks[i]=values[i]+3;}
            for(int n=0;n<instances;++n){
                for(int i=0;i<512;++i){const auto v=.06f*std::sin((frame*512+i)*.08f);audio.setSample(0,i,v);audio.setSample(1,i,v);}
                (n==0?processor:*second).processBlock(audio,midi);
                for(int ch=0;ch<2;++ch)for(int i=0;i<512;++i)if(!std::isfinite(audio.getSample(ch,i)))return false;
            }
            const auto cpuStart=threadCpuMs();
            auto start=std::chrono::steady_clock::now();
            for(int n=0;n<instances;++n){
                auto* target=n==0?graph:graph2;target->updateAnalyzer(values,peaks);
                juce::Graphics g(canvas);g.addTransform(juce::AffineTransform::scale(scale));target->paint(g);
            }
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(frame>=20){frames.push_back(ms);cpuMs+=threadCpuMs()-cpuStart;}
        }
        std::sort(frames.begin(),frames.end());double mean=0;for(auto v:frames)mean+=v;mean/=frames.size();
        csv<<instances<<','<<scale<<','<<frames[frames.size()/2]<<','<<frames[(frames.size()*95)/100]<<','<<frames.back()<<','<<mean<<','<<mean*6<<','<<cpuMs/frames.size()<<','<<cpuMs/frames.size()*6<<'\n';
        std::cout<<"GUI104: "<<instances<<" instance(s), scale "<<scale<<", graph paint median "<<frames[frames.size()/2]<<" ms p95 "<<frames[(frames.size()*95)/100]<<" ms\n";
    }
    for(const char* mode:{"ROOT","CARVE","ONYX","ANALYZE"}){
        if(!clickMode(mode))return false;
        for(float scale:{1.f,1.25f,1.5f}){
            auto capture=editor.createComponentSnapshot(editor.getLocalBounds(),true,scale);
            if(!save(capture,"surgery-ui-104-"+juce::String(mode)+"-"+juce::String((int)(scale*100))+".png"))return false;
        }
    }
    std::cout<<"GUI104: five menu models/selection notifications, three render scales, all tabs and two instances passed; offscreen timings exclude host compositor\n";
    return true;
}

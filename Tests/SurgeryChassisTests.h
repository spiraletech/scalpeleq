#pragma once
#include "../Source/RootsEditor.h"
#include <fstream>
inline bool verifyChassis105(){
    auto processor=std::make_unique<PRISMVSTAudioProcessor>();processor->prepareToPlay(48000,512);
    std::unique_ptr<juce::AudioProcessorEditor> base(processor->createEditor());auto* editor=dynamic_cast<RootsEditor*>(base.get());if(!editor)return false;
    RootsGraph* graph=nullptr;for(auto* child:editor->getChildren())if(auto* g=dynamic_cast<RootsGraph*>(child))graph=g;
    if(!graph)return false;
    std::ofstream csv("surgery-chassis-resize.csv");csv<<"width,height,scale,raster_w,raster_h,border_corner_error,signal_peak_db,snapshot_ms\n";
    juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;juce::Random noise(8015);
    std::array<juce::Image,3> reference;
    const std::array<juce::Point<int>,5> sizes{{{1320,880},{840,610},{1800,1200},{840,1200},{1800,610}}};
    for(auto size:sizes){
        editor->setSize(size.x,size.y);
        for(auto* child:editor->getChildren())if(auto* grip=dynamic_cast<juce::ResizableCornerComponent*>(child)){
            if(!grip->getTransform().isIdentity()||grip->getBounds().getRight()!=size.x||grip->getBounds().getBottom()!=size.y)return false;
        }
        for(auto* child:editor->getChildren())if(child->isVisible()&&dynamic_cast<juce::ResizableCornerComponent*>(child)==nullptr){
            const auto bounds=child->getBoundsInParent().toFloat();
            if(!surgery::rack::instrumentArea(size.x,size.y).expanded(2).contains(bounds)){
                std::cerr<<"Chassis: control overlaps frame "<<child->getName()<<'\n';return false;
            }
        }
        for(int n=0;n<24;++n){
            for(int i=0;i<512;++i){const float v=(noise.nextFloat()*2-1)*.16f;audio.setSample(0,i,v);audio.setSample(1,i,v);}
            processor->processBlock(audio,midi);if(n%4==3)juce::MessageManager::getInstance()->runDispatchLoopUntil(12);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
        if(!graph->spectralMetrics().signalPresent)return false;
        int index=0;
        for(float scale:{1.f,1.25f,1.5f}){
            const auto start=std::chrono::steady_clock::now();
            auto shot=editor->createComponentSnapshot(editor->getLocalBounds(),true,scale);
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            const auto raster=editor->backgroundRasterSize();
            if(std::abs(raster.x-juce::roundToInt(size.x*scale))>2||std::abs(raster.y-juce::roundToInt(size.y*scale))>2){
                std::cerr<<"Chassis: stale DPI cache "<<raster.x<<','<<raster.y<<" scale "<<scale<<'\n';return false;
            }
            double error=0;int count=0;const int corner=juce::roundToInt(46*scale);
            if(reference[(size_t)index].isNull())reference[(size_t)index]=shot;
            const auto& ref=reference[(size_t)index];
            for(int right=0;right<2;++right)for(int bottom=0;bottom<2;++bottom)
                for(int y=4;y<corner-4;++y)for(int x=4;x<corner-4;++x){
                    const auto a=shot.getPixelAt(right?shot.getWidth()-corner+x:x,bottom?shot.getHeight()-corner+y:y);
                    const auto b=ref.getPixelAt(right?ref.getWidth()-corner+x:x,bottom?ref.getHeight()-corner+y:y);
                    error+=std::abs(a.getRed()-b.getRed())+std::abs(a.getGreen()-b.getGreen())+std::abs(a.getBlue()-b.getBlue());count+=3;
                }
            error/=count;if(error>12){std::cerr<<"Chassis: corner proportions changed "<<error<<'\n';return false;}
            for(auto point:{juce::Point<float>(2,size.y*.5f),juce::Point<float>(size.x-3.f,size.y*.5f),juce::Point<float>(size.x*.5f,3),juce::Point<float>(size.x*.5f,size.y-4.f)})
                if(shot.getPixelAt((int)(point.x*scale),(int)(point.y*scale)).getBrightness()<.08f){std::cerr<<"Chassis: disconnected carbon/glass edge\n";return false;}
            const auto top=surgery::rack::StormChassis::top(size.x);
            if(top.getCentreX()!=size.x*.5f||top.getBottom()>40)return false;
            for(bool right:{false,true}){const auto tube=surgery::rack::StormChassis::side(size.x,size.y,right);if(tube.getY()!=52||tube.getBottom()!=size.y-52)return false;}
            const auto name="surgery-ui-chassis-"+juce::String(size.x)+"x"+juce::String(size.y)+"-"+juce::String((int)(scale*100))+".png";
            juce::FileOutputStream out(juce::File::getCurrentWorkingDirectory().getChildFile(name));out.setPosition(0);out.truncate();
            if(!juce::PNGImageFormat().writeImageToStream(shot,out))return false;
            csv<<size.x<<','<<size.y<<','<<scale<<','<<raster.x<<','<<raster.y<<','<<error<<','<<graph->spectralMetrics().loudestDb<<','<<ms<<'\n';++index;
        }
    }
    // Fast repeated logical resizes/moves while audio continues. Snapshot cost
    // above includes cold cache regeneration; steady paints are benchmarked separately.
    for(int n=0;n<16;++n){editor->setSize(840+n*48,610+n*28);editor->setTopLeftPosition(n*2,n);audio.clear();processor->processBlock(audio,midi);}
    std::cout<<"STORM CHASSIS: 15 size/DPI captures, fixed X-head corners, connected glass/carbon edges, centred chamber, control containment and cache dimensions passed\n";
    return true;
}

#include <JuceHeader.h>
#include "../Source/SurgeryStormChassis.h"
#include "../Source/SurgeryChassisLayout.h"
#include <chrono>
#include <fstream>
#include <iostream>
void startAllocationProbe() noexcept;
std::size_t finishAllocationProbe() noexcept;
bool runStormChassisTests(){
    juce::ScopedJuceInitialiser_GUI gui;
    using surgery::rack::StormChassis;
    auto fail=[](const char* text){std::cerr<<"STORM CHASSIS FAIL: "<<text<<'\n';return false;};
    auto borderStorage=std::make_unique<StormChassis>();auto& border=*borderStorage;surgery::weather::Meaning signal;signal.level=.8f;signal.bass=.6f;signal.flux=.4f;signal.width=.5f;signal.centroid=.65f;
    const double expected=.30*.8+.35*.6+.20*.4+.15*.5;
    for(int i=0;i<120;++i)border.update(signal,1./60.,.5,false,false);
    auto held=border.state();
    if(std::abs(held.energy-expected)>.00001||std::abs(held.glow-(.012+.988*std::pow(expected,1.35)))>.00001||held.phase<=0)return fail("energy/glow/flow equation");
    for(int i=0;i<120;++i)border.update({},1./60.,.5,true,false);
    if(std::memcmp(&held,&border.state(),sizeof(held)))return fail("Freeze changes held state");
    border.update({},.1,.5,false,true);auto reduced=border.state();
    if(reduced.phase<=held.phase||reduced.phase-held.phase>.003||reduced.energy>=held.energy||reduced.glow<=.012)return fail("reduced motion loses level response");
    for(int i=0;i<1000;++i)border.update({},.1,.5,false,true);
    if(border.state().energy!=0||border.state().glow!=.012)return fail("silence/reduced motion");
    startAllocationProbe();for(int i=0;i<1000;++i)border.update(signal,.016,.18,false,true);
    if(finishAllocationProbe()!=0)return fail("state update heap allocation");
    // Both static and moving layers must leave every interior pixel untouched.
    // Use software storage for exact alpha tests, independent of GPU rounding.
    for(auto size:{juce::Point<int>{840,610},juce::Point<int>{1320,880},juce::Point<int>{1800,1200},juce::Point<int>{840,1200},juce::Point<int>{1800,610}}){
        juce::Image image(juce::Image::ARGB,size.x,size.y,true,juce::SoftwareImageType{});
        {juce::Graphics g(image);border.paintStatic(g,size.x,size.y);border.paintLiquid(g,size.x,size.y);}
        const auto interior=surgery::rack::instrumentArea(size.x,size.y).toNearestInt();
        for(int y=interior.getY();y<interior.getBottom();++y)for(int x=interior.getX();x<interior.getRight();++x)
            if(image.getPixelAt(x,y).getAlpha()!=0)return fail("border overlaps instrument face");
    }
    auto hollowStorage=std::make_unique<StormChassis>();auto& hollow=*hollowStorage;
    juce::Image vessel(juce::Image::ARGB,1120,755,true,juce::SoftwareImageType{});
    auto draw=[&](){vessel.clear(vessel.getBounds(),juce::Colours::transparentBlack);juce::Graphics g(vessel);hollow.paintLiquid(g,1120,755);};
    auto normal=signal;normal.level=.65f;normal.bass=.45f;normal.flux=.20f;normal.centroid=.5f;
    for(int frame=0;frame<120;++frame){hollow.update(normal,1./60.,.3,false,false);draw();}
    const auto active=hollow.statistics();
    std::cout<<"Normal occupancy "<<active.renderedLitFraction<<" chambers ";for(auto v:active.chamberOccupancy)std::cout<<v<<' ';std::cout<<" families ";for(auto f:active.colourFamilies)for(auto c:f)std::cout<<c<<' ';std::cout<<'\n';
    std::ofstream truth("surgery-hollow-vessel.csv");truth<<"scene,mean_density,peak,rendered_lit_fraction,top_occupancy,left_occupancy,right_occupancy\n";
    auto record=[&](const char* name,const auto& stats){truth<<name<<','<<stats.meanDensity<<','<<stats.maximum<<','<<stats.renderedLitFraction;for(auto v:stats.chamberOccupancy)truth<<','<<v;truth<<'\n';truth.flush();};record("normal",active);
    if(active.maximum<.04||active.renderedLitFraction<.25||active.renderedLitFraction>.45)return fail("normal liquid occupancy outside 25-45 percent");
    for(auto occupancy:active.chamberOccupancy)if(occupancy<.25||occupancy>.45)return fail("individual chamber normal occupancy outside target");
    for(auto families:active.colourFamilies)for(auto count:families)if(count<12)return fail("a chamber lacks multicolour currents");
    // A river fills a continuous central channel; its banks must not swing
    // across the chamber like a single winding filament.
    int minCentre=100,maxCentre=0,continuous=0;
    for(int x=100;x<1020;++x){int lo=35,hi=-1;
        for(int y=7;y<33;++y)if(vessel.getPixelAt(x,y).getAlpha()>25){lo=std::min(lo,y);hi=std::max(hi,y);}
        if(hi>=lo){minCentre=std::min(minCentre,lo+hi);maxCentre=std::max(maxCentre,lo+hi);if(hi-lo>=6)++continuous;}
    }
    if(continuous<900||maxCentre-minCentre>6)return fail("river loses broad continuity / develops winding spine");
    auto density=std::make_unique<std::array<float,30720>>(hollow.densityField());auto optics=std::make_unique<std::array<StormChassis::OpticalCell,30720>>(hollow.opticalField());auto current=hollow.state();
    for(int frame=0;frame<30;++frame){hollow.update(signal,1./60.,.3,true,false);draw();}
    if(std::memcmp(&current,&hollow.state(),sizeof(current))||*density!=hollow.densityField()||*optics!=hollow.opticalField())return fail("Freeze advances plasma/trails");
    hollow.update(signal,1./60.,.3,true,false,false);draw();
    for(int y=0;y<vessel.getHeight();++y)for(int x=0;x<vessel.getWidth();++x)if(vessel.getPixelAt(x,y).getAlpha()!=0)return fail("SONAR OFF leaves animated energy visible");
    if(*density!=hollow.densityField()||*optics!=hollow.opticalField())return fail("OFF destroys frozen field");
    for(int frame=0;frame<300;++frame){hollow.update({},.05,.3,false,false,true);draw();}
    if(hollow.statistics().maximum>1.e-5||hollow.state().energy!=0)return fail("silence retains energy or full motion");
    const auto quietPhase=hollow.state().phase;for(int i=0;i<20;++i)hollow.update({},.05,.3,false,false,true);
    if(hollow.state().phase!=quietPhase)return fail("silent independent motion");
    record("silence",hollow.statistics());
    auto energetic=signal;energetic.level=.95f;energetic.bass=.85f;energetic.flux=.8f;energetic.width=.8f;
    for(int i=0;i<120;++i){hollow.update(energetic,1./60.,.3,false,false);draw();}record("energetic",hollow.statistics());
    if(hollow.statistics().meanDensity<=active.meanDensity||hollow.statistics().renderedLitFraction<=active.renderedLitFraction||hollow.statistics().renderedLitFraction>.70)return fail("energy does not increase body/bloom");
    std::array<std::uint64_t,5> signatures{};
    for(int mode=0;mode<5;++mode){auto container=std::make_unique<StormChassis>();
        for(int frame=0;frame<80;++frame){container->update(signal,1./60.,.18,false,false,true,mode);vessel.clear(vessel.getBounds());juce::Graphics g(vessel);container->paintLiquid(g,1120,755);}
        std::uint64_t hash=1469598103934665603ull;
        for(int x=100;x<1020;x+=3)for(int y=8;y<32;++y){hash^=vessel.getPixelAt(x,y).getARGB();hash*=1099511628211ull;}
        signatures[(size_t)mode]=hash;for(int previous=0;previous<mode;++previous)if(signatures[(size_t)previous]==hash)return fail("JUKE personalities are visually identical");
        auto heldDensity=std::make_unique<std::array<float,30720>>(container->densityField());
        container->update({},.1,.18,true,false,true,5);vessel.clear(vessel.getBounds());{juce::Graphics g(vessel);container->paintLiquid(g,1120,755);}
        if(*heldDensity!=container->densityField()||container->statistics().renderedLitFraction!=0)return fail("independent JUKE Off fails while frozen");
    }
    std::ofstream csv("surgery-storm-chassis-performance.csv");csv<<"scale,median_ms,p95_ms,max_ms,dynamic_texture_pixels,raw_fixed_texture_bytes\n";
    for(float scale:{1.f,1.25f,1.5f}){
        juce::Image target(juce::Image::RGB,(int)(1120*scale),(int)(755*scale),true);std::vector<double> times;
        for(int frame=0;frame<50;++frame){
            const auto start=std::chrono::steady_clock::now();border.update(signal,1./60.,.18,false,false);
            {juce::Graphics g(target);g.addTransform(juce::AffineTransform::scale(scale));border.paintLiquid(g,1120,755);}
            if(frame>=10)times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        std::sort(times.begin(),times.end());csv<<scale<<','<<times[times.size()/2]<<','<<times[times.size()*95/100]<<','<<times.back()<<",30720,799488\n";
    }
    std::cout<<"STORM CHASSIS: weighted energy/glow equations, Freeze, reduced motion, silence, allocation-free state update, five editor sizes with untouched interior, and 100/125/150% border timings passed\n";
    return true;
}

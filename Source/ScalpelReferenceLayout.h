#pragma once
#include <JuceHeader.h>
#include <array>

// Asset kit a1e576c4, 1645x956. FL's 35px title bar is not plugin artwork.
// A single uniform conversion authors the new skin in reference coordinates;
// the existing 1120x630 editor transform continues to handle resize and DPI.
namespace scalpel::reference {
inline constexpr float scale=1120.f/1645.f;
inline constexpr float pad=(630.f-921.f*scale)*.5f;
inline juce::Rectangle<float> rect(float x,float y,float w,float h){return {x*scale,(y-35)*scale+pad,w*scale,h*scale};}
inline juce::Rectangle<int> bounds(float x,float y,float w,float h){return rect(x,y,w,h).toNearestInt();}
inline juce::Rectangle<float> header(){return rect(20,41,1605,88);}
inline juce::Rectangle<float> analyzer(){return rect(33,216,1583,346);}
inline juce::Rectangle<float> plot(){return rect(122,258,1114,248);}
inline juce::Rectangle<float> deck(){return rect(25,646,1593,256);}
inline juce::Rectangle<int> band(int i){
    constexpr std::array<float,7> x{45,284,503,722,943,1165,1388};
    constexpr std::array<float,7> w{228,212,212,212,212,212,217};
    return bounds(x[(size_t)i],143,w[(size_t)i],60);
}
inline juce::Rectangle<int> tab(int i){
    constexpr std::array<float,4> x{38,456,829,1219};
    constexpr std::array<float,4> w{411,367,384,392};
    return bounds(x[(size_t)i],568,w[(size_t)i],62);
}
inline juce::Rectangle<int> carveDial(int i){return bounds(366+i*235,687,166,190);}
inline juce::Rectangle<int> colorDial(int i){return bounds(104+i*298,687,166,190);}
inline juce::Rectangle<float> plotLocal(){return plot().translated(-analyzer().toNearestInt().getX(),-analyzer().toNearestInt().getY());}
inline std::array<juce::Point<float>,4> screwCentres(juce::Rectangle<float> panel,float xInset,float yInset){
    const auto r=panel.reduced(xInset*scale,yInset*scale);
    return {r.getTopLeft(),r.getTopRight(),r.getBottomLeft(),r.getBottomRight()};
}
}

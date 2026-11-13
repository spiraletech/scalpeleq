#pragma once
#include <JuceHeader.h>
#include <array>

// Formidable kit, 1670 x 942 artwork; no host chrome in these coordinates.
// One uniform transform. Never change editor dimensions when switching pages.
namespace scalpel::reference {
inline constexpr float scale=630.f/942.f;
inline constexpr float pad=(1120.f-1670.f*scale)*.5f;
inline juce::Rectangle<float> rect(float x,float y,float w,float h){return {x*scale+pad,y*scale,w*scale,h*scale};}
inline juce::Rectangle<int> bounds(float x,float y,float w,float h){return rect(x,y,w,h).toNearestInt();}
inline juce::Rectangle<float> header(){return rect(36,8,1570,95);}
inline juce::Rectangle<float> enclosure(){return {0,0,1120,630};}
inline juce::Rectangle<float> analyzer(){return rect(41,200,1581,298);}
// Measured illuminated plot, excluding its frequency labels and inner bezel.
// The crop's 106,235..1219,475 includes the bevel and axis, not just the plot.
inline juce::Rectangle<float> plot(){return rect(136,243,1077,207);}
inline juce::Rectangle<float> deck(){return rect(34,578,1588,258);}
inline juce::Rectangle<int> band(int i){
    // Equal manufactured keys; the crop widths include unequal contact shadows.
    return bounds(65+i*221,116,214,68);
}
inline juce::Rectangle<int> tab(int i){
    constexpr std::array<float,4> x{42,469,841,1225},w{425,360,380,384};
    return bounds(x[(size_t)i],506,w[(size_t)i],62);
}
inline juce::Rectangle<int> switchBounds(bool off){return bounds(109,off?709.f:623.f,181,74);}
inline juce::Rectangle<int> carveSlot(int i){return bounds(1170+(i%2)*225,623+(i/2)*91,181,74);}
inline juce::Rectangle<int> carveDial(int i){
    constexpr std::array<float,3> centres{460,705,995};
    return bounds(centres[(size_t)i]-83,624,166,199);
}
inline juce::Rectangle<int> colorDial(int i){return bounds(137+i*310,624,166,199);}
// ANALYZE fills the existing deck; housing and screw coordinates stay locked.
inline juce::Rectangle<int> analyzeButton(bool freeze){return bounds(freeze?1272.f:192.f,626,196,196);}
inline juce::Rectangle<int> decayDial(){return bounds(733,622,204,207);}
inline constexpr int decayReadoutWidth=124,decayReadoutHeight=26;
inline juce::Rectangle<float> plotLocal(){return plot().translated(-analyzer().toNearestInt().getX(),-analyzer().toNearestInt().getY());}
inline std::array<juce::Point<float>,4> screwCentres(juce::Rectangle<float> panel,float xInset,float yInset){
    const auto r=panel.reduced(xInset*scale,yInset*scale);
    return {r.getTopLeft(),r.getTopRight(),r.getBottomLeft(),r.getBottomRight()};
}
inline std::array<juce::Point<float>,4> housingScrews(bool lower){
    const std::array<juce::Point<float>,4> source=lower?
        std::array<juce::Point<float>,4>{{{69,614},{1607,618},{69,810},{1605,812}}}:
        std::array<juce::Point<float>,4>{{{72,231},{1600,232},{72,478},{1598,478}}};
    std::array<juce::Point<float>,4> result{};
    const auto origin=(lower?deck():analyzer()).toNearestInt().getPosition().toFloat();
    for(size_t i=0;i<4;++i){const auto& c=source[i];result[i]=rect(c.x,c.y,0,0).getPosition()-(lower?juce::Point<float>{}:origin);}
    return result;
}
}

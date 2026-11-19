#pragma once
#include <JuceHeader.h>
#include <algorithm>
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
// 1.0.8.4.2 analyzer masters are authored on 2172 x 436. Every visual,
// interaction and readout is registered through this one transform so the
// DSP trace, grid, polycarbonate and hit targets cannot drift apart.
inline juce::Rectangle<float> analyzerAssetLocal(float x,float y,float w,float h){
    const auto a=analyzer();
    return {x*a.getWidth()/2172.f,y*a.getHeight()/436.f,
            w*a.getWidth()/2172.f,h*a.getHeight()/436.f};
}
inline juce::Rectangle<float> analyzerAsset(float x,float y,float w,float h){
    return analyzerAssetLocal(x,y,w,h).translated(analyzer().getX(),analyzer().getY());
}
inline juce::Rectangle<float> plot(){return analyzerAsset(208,70,1408,290);}
// New 1637x961 kit: extend the faceplate to 917/961 of client height.
// Keep its top and width anchored; this does not resize the editor.
inline juce::Rectangle<float> deck(){auto r=rect(34,578,1588,258);return r.withBottom(630.f*917.f/961.f);}
inline juce::Rectangle<int> analyzeAsset(float x,float y,float w,float h){
    const auto d=deck();return juce::Rectangle<float>{d.getX()+(x-33.f)*d.getWidth()/1570.f,
        d.getY()+(y-600.f)*d.getHeight()/317.f,w*d.getWidth()/1570.f,h*d.getHeight()/317.f}.toNearestInt();
}
inline juce::Rectangle<int> band(int i){
    // Equal manufactured keys; the crop widths include unequal contact shadows.
    return bounds(65+i*221,116,214,68);
}
inline constexpr std::array<int,3> modePages{0,1,3};
inline juce::Rectangle<int> tab(int mode){
    const auto pos=std::find(modePages.begin(),modePages.end(),mode);
    if(pos==modePages.end())return {};
    return bounds(45+522*(float)(pos-modePages.begin()),506,514,62);
}
inline juce::Rectangle<int> masterCaption(){return analyzerAsset(65,75,114,30).toNearestInt();}
inline juce::Rectangle<int> masterButton(int i){
    constexpr std::array<float,3> y{122,196,268};
    return analyzerAsset(63,y[(size_t)i],118,50).toNearestInt();
}
inline juce::Rectangle<int> analyzerXray(){return analyzerAsset(1748,55,244,81).toNearestInt();}
inline juce::Rectangle<int> analyzerReset(){return analyzerAsset(1765,306,228,76).toNearestInt();}
// Flush kit positions are local to its 32,567,1574,312 lower panel.
// Uniformly register them to the verified deck; never move the screw anchors
// or scale the whole editor to reconcile differing screenshot/host chrome.
inline float flushScale(){return deck().getWidth()/1574.f;}
inline juce::Rectangle<int> flushRect(float x,float y,float w,float h){const auto d=deck();const float s=flushScale();return juce::Rectangle<float>{d.getX()+(x-32.f)*s,d.getY()+(y-567.f)*s,w*s,h*s}.toNearestInt();}
inline juce::Rectangle<int> switchBounds(bool off,bool carve=false){
    juce::ignoreUnused(carve); // 1.0.7.7: BAND is the shared lower-panel anchor.
    return flushRect(196.f-95.f,(off?749.f:660.f)-36.f,190,72);
}
inline juce::Rectangle<int> bandAction(int i){return flushRect((i%2?1480.f:1270.f)-94.f,(i/2?753.f:662.f)-33.f,188,66);}
inline juce::Rectangle<int> carveSlot(int i){return bandAction(i);}
inline juce::Rectangle<int> carveDial(int i){
    // Locked 1.0.7.7 screenshot centres, registered to the BAND lower-panel
    // substrate (31,486,1276,265). Host chrome is deliberately excluded.
    constexpr std::array<float,3> screenCentres{371.f,598.f,826.f};
    const auto d=deck();
    const float centre=d.getX()+(screenCentres[(size_t)i]-31.f)*d.getWidth()/1276.f;
    auto r=bounds(0,624,166,199);r.setCentre(juce::roundToInt(centre),r.getCentreY());return r;
}
inline float carveDividerX(int i){
    constexpr std::array<float,4> screenX{256.f,484.f,712.f,940.f};
    const auto d=deck();return d.getX()+(screenX[(size_t)i]-31.f)*d.getWidth()/1276.f;
}
inline float columnCentre(int i,int count){const auto d=deck();return d.getX()+d.getWidth()*(i+.5f)/count;}
inline juce::Rectangle<int> colorDial(int i){auto r=rect(0,624,166,199);return r.withCentre({columnCentre(i,5),r.getCentreY()}).toNearestInt();}
// ANALYZE control proportions measured from the supplied kit, relative to its deck.
inline juce::Rectangle<int> analyzeButton(bool freeze){const float side=190.f*scale;return juce::Rectangle<float>{columnCentre(freeze?2:0,3)-side*.5f,deck().getY()+47.f,side,side}.toNearestInt();}
inline juce::Rectangle<int> decayDial(){return juce::Rectangle<float>{columnCentre(1,3)-83.f*scale,deck().getY()+43.f,166.f*scale,199.f*scale}.toNearestInt();}
inline juce::Rectangle<int> analyzeCaption(int i){return juce::Rectangle<float>{columnCentre(i,3)-140.f*scale,deck().getY()+14.f,280.f*scale,36.f*scale}.toNearestInt();}
inline constexpr int decayReadoutWidth=106,decayReadoutHeight=30;
inline juce::Rectangle<float> plotLocal(){return analyzerAssetLocal(208,70,1408,290);}
inline std::array<juce::Point<float>,4> screwCentres(juce::Rectangle<float> panel,float xInset,float yInset){
    const auto r=panel.reduced(xInset*scale,yInset*scale);
    return {r.getTopLeft(),r.getTopRight(),r.getBottomLeft(),r.getBottomRight()};
}
inline std::array<juce::Point<float>,4> housingScrews(bool lower){
    if(lower){
        const auto d=deck();
        const float l=d.getX()+39.f*d.getWidth()/1570.f,r=d.getX()+1531.f*d.getWidth()/1570.f;
        const float t=d.getY()+33.f*d.getHeight()/317.f,b=d.getY()+270.f*d.getHeight()/317.f;
        return {{{l,t},{r,t},{l,b},{r,b}}};
    }
    const std::array<juce::Point<float>,4> source=lower?
        std::array<juce::Point<float>,4>{{{69,614},{1607,618},{69,810},{1605,812}}}:
        std::array<juce::Point<float>,4>{{{72,231},{1600,232},{72,478},{1598,478}}};
    std::array<juce::Point<float>,4> result{};
    const auto origin=(lower?deck():analyzer()).toNearestInt().getPosition().toFloat();
    for(size_t i=0;i<4;++i){const auto& c=source[i];result[i]=rect(c.x,c.y,0,0).getPosition()-(lower?juce::Point<float>{}:origin);}
    return result;
}
}




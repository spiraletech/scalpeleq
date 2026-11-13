#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include "ScalpelMaterials.h"
#include "ScalpelFinish.h"

namespace scalpel::ui105 {
inline const juce::Colour champagne{0xff0d1720};
inline const juce::Colour ivory{0xffece0d5};
inline const juce::Colour ink{0xff070d13};
inline const juce::Colour amber{0xffcde8e5};
inline const juce::Colour chassisEdge{0xff4e83a5};

inline void roundedPanel(juce::Graphics& g, juce::Rectangle<float> r, bool dark, float radius=8.0f)
{
    g.setColour(juce::Colour(0xff02070b).withAlpha(.96f));
    g.fillRoundedRectangle(r.expanded(2.0f).translated(0, 1.5f), radius + 1.0f);
    juce::ColourGradient face(dark ? juce::Colour(0xff17191d) : juce::Colour(0xffc3c7cb),
                              r.getX(), r.getY(),
                              dark ? juce::Colour(0xff02060a) : juce::Colour(0xff526b7c),
                              r.getX(), r.getBottom(), false);
    if (!dark) face.addColour(.35, juce::Colour(0xffe6f1fb));
    g.setGradientFill(face);
    g.fillRoundedRectangle(r, radius);
    g.setColour((dark ? scalpel::materials::edge : scalpel::materials::silver).withAlpha(.62f));
    g.drawRoundedRectangle(r.reduced(.8f), radius - .6f, 1.0f);
    g.setColour(juce::Colour(0xff061019).withAlpha(.96f));
    g.drawRoundedRectangle(r, radius, 1.0f);
}

inline void drawRackFace(juce::Graphics& g,int width,int height,int mode)
{
    using namespace scalpel::reference;
    scalpel::finish::shell(g,{0,0,(float)width,(float)height});
    // Engraved native type on the shared shell; the kit's italic crop is not
    // a shipping wordmark and its rejected footer art is never loaded.
    const auto engraved=[&](const juce::String& text,juce::Rectangle<int> box,juce::Font font,juce::Justification align=juce::Justification::centredLeft){
        g.setFont(font);g.setColour(juce::Colour(0xffeee0cd).withAlpha(.8f));
        g.drawFittedText(text,box.translated(0,1),align,1);
        g.setColour(scalpel::finish::ink);g.drawFittedText(text,box,align,1);
    };
    auto word=scalpel::finish::font(44,true);word.setExtraKerningFactor(.32f);
    engraved("SCALPEL EQ",bounds(110,26,590,68),word);
    auto subtitle=word;subtitle.setHeight(20);subtitle.setExtraKerningFactor(.14f);
    engraved("SURGICAL EQUALIZER",bounds(1000,26,590,68),subtitle,juce::Justification::centredRight);
    // Recesses are manufactured wells, not extra decorative outline layers.
    const auto well=[&](juce::Rectangle<float> area,float radius){
        g.setColour(juce::Colour(0xff55483c));g.fillRoundedRectangle(area.expanded(1),radius+1);
        g.setColour(juce::Colour(0xff1b1713));g.fillRoundedRectangle(area,radius);
        g.setColour(scalpel::finish::silver.withAlpha(.65f));g.drawRoundedRectangle(area,radius,.8f);
    };
    well(rect(45,104,1564,92),14);well(rect(40,495,1568,78),10);
    const auto shadow=[&](juce::Rectangle<float> area,float radius){
        juce::Path path;path.addRoundedRectangle(area.reduced(1.5f),radius);
        juce::DropShadow(juce::Colours::black.withAlpha(.32f),3,{0,1}).drawForPath(g,path);
    };
    for(int i=0;i<7;++i)shadow(band(i).toFloat(),7);
    for(int i=0;i<4;++i)shadow(tab(i).toFloat(),5);
    scalpel::finish::panel(g,deck(),19);
    for(const auto c:housingScrews(true))scalpel::finish::screw(g,c);
    if(mode==0||mode==1){
        shadow(switchBounds(false).toFloat(),12.f*flushScale());shadow(switchBounds(true).toFloat(),12.f*flushScale());
        if(mode==0){
            for(int i=0;i<4;++i)shadow(bandAction(i).toFloat(),12.f*flushScale());
            shadow(bounds(837,636,296,55).toFloat(),5);
            shadow(bounds(837,732,296,55).toFloat(),5);
        }else for(int i=0;i<4;++i)shadow(carveSlot(i).toFloat(),12.f*flushScale());
    }
    std::array<float,4> dividers{};int count=4;
    if(mode==0)dividers={313,560,805,1164};
    else if(mode==1)for(int i=0;i<4;++i)dividers[(size_t)i]=carveDividerX(i);
    else if(mode==2)dividers={375,685,995,1305};
    else {dividers={570,1100,0,0};count=2;}
    for(int i=0;i<count;++i){auto line=(mode==1?juce::Rectangle<float>{dividers[(size_t)i],rect(0,622,1,187).getY(),2.f*scale,rect(0,622,1,187).getHeight()}:rect(dividers[(size_t)i],622,2,187)).withBottom(deck().getBottom()-28.f*scale);
        if(mode==2||mode==3){const int columns=mode==2?5:3;line.setX(deck().getX()+deck().getWidth()*(i+1)/columns);}
        g.setColour(scalpel::finish::silver);g.fillRect(line.translated(1,0));
        g.setColour(juce::Colour(0xff55463a));g.fillRect(line);}
}

inline void drawMeter(juce::Graphics& g, juce::Rectangle<float> r, float db, const juce::String& label,bool polished=false)
{
    g.setColour(juce::Colour(0xff050505));
    g.fillRoundedRectangle(r, 3.0f);
    constexpr int segments = 15;
    const float normalized = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    const int active = juce::roundToInt(normalized * segments);
    const float gap = 1.3f;
    const float segmentH = (r.getHeight() - gap * (segments - 1)) / segments;
    for (int i = 0; i < segments; ++i)
    {
        const bool lit = i < active;
        auto cell = juce::Rectangle<float>(r.getX() + 4.0f,
            r.getBottom() - (i + 1) * segmentH - i * gap,
            r.getWidth() - 8.0f, segmentH);
        const auto hot = i > 12 ? juce::Colour(0xfff3fbff)
                                : i > 9 ? juce::Colour(0xffcceeff) : amber;
        juce::ColourGradient segment(lit?juce::Colour(0xffd0c5b3):juce::Colour(0xff7f776a),cell.getX(),cell.getY(),
                                    lit?juce::Colour(0xffa59984):juce::Colour(0xff38332c),cell.getRight(),cell.getBottom(),false);
        g.setGradientFill(segment);
        g.fillRect(cell);
    }
    g.setColour(ivory);
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 12.f, juce::Font::plain)));
    g.drawText(label, (int)r.getX(), (int)r.getBottom() + 2, (int)r.getWidth(), 14,
               juce::Justification::centred);
}

inline void drawAnalyzerPanel(juce::Graphics& g,juce::Rectangle<float> full,
    float peakHold,float leftPeak,float rightPeak,bool polished=false)
{
    // Absolute reference positions translated into the analyzer component.
    const auto origin=scalpel::reference::analyzer().toNearestInt().getPosition().toFloat();
    const auto local=[&](float x,float y,float w,float h){return scalpel::reference::rect(x,y,w,h).translated(-origin.x,-origin.y);};
    const auto display=local(1330,332,161,43);
    g.setColour(juce::Colour(0xff0a0907));g.fillRoundedRectangle(display,4);
    g.setColour(juce::Colour(0xffa89a89));g.drawRoundedRectangle(display,4,.6f);
    g.setColour(scalpel::finish::text);g.setFont(scalpel::finish::font(12,false));
    g.drawText("PEAK (dBFS)",local(1330,302,161,24).toNearestInt(),juce::Justification::centred);
    g.setColour(juce::Colour(0xffcde8e5));
    scalpel::finish::number(g,peakHold<=-143?juce::String("-inf"):juce::String(peakHold,2),display,20);
    drawMeter(g,local(1512,250,26,192),leftPeak,"L",polished);
    drawMeter(g,local(1552,250,26,192),rightPeak,"R",polished);
    (void)full;
}
} // namespace scalpel::ui105




#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include "ScalpelMaterials.h"
#include "ScalpelFinish.h"
#include "ScalpelEtherTechMark.h"

namespace scalpel::ui105 {
inline const juce::Colour champagne{0xff0d1720};
inline const juce::Colour ivory{0xffece0d5};
inline const juce::Colour ink{0xff070d13};
inline const juce::Colour amber{0xff05dbe9};
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
    scalpel::finish::panel(g,header(),14);
    g.setColour(scalpel::finish::ink);
    g.setFont(scalpel::finish::font(40));
    g.drawText("SCALPEL EQ",bounds(84,57,340,58),juce::Justification::centredLeft);
    g.setFont(scalpel::finish::font(18));
    g.drawFittedText("S U R G I C A L   E Q U A L I Z E R",bounds(590,66,450,38),juce::Justification::centred,1);
    g.drawFittedText("E T H E R T E C H",bounds(1385,67,199,36),juce::Justification::centredLeft,1);
    etherTechMark(g);
    for(float x:{477.f,1175.f}){auto r=rect(x,63,2,45);g.fillRect(r);}
    // Band selectors are independent molded keys on the ivory chassis.
    const auto lower=deck();scalpel::finish::panel(g,lower,16);
    scalpel::finish::screws(g,lower,29,28);
    std::array<float,4> dividers{};int count=4;
    if(mode==0)dividers={307,535,783,1175};
    else if(mode==1)dividers={345,580,815,1175};
    else if(mode==2)dividers={330,628,926,1224};
    else {dividers={548,1096,0,0};count=2;}
    g.setColour(scalpel::finish::ink);
    for(int i=0;i<count;++i){auto line=rect(dividers[(size_t)i],676,2,194);g.fillRect(line);}
}

inline void drawMeter(juce::Graphics& g, juce::Rectangle<float> r, float db, const juce::String& label,bool polished=false)
{
    g.setColour(juce::Colour(0xff050505));
    g.fillRoundedRectangle(r, 3.0f);
    constexpr int segments = 15;
    const float normalized = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    const int active = juce::roundToInt(normalized * segments);
    const float gap = 2.0f;
    const float segmentH = (r.getHeight() - gap * (segments - 1)) / segments;
    for (int i = 0; i < segments; ++i)
    {
        const bool lit = i < active;
        auto cell = juce::Rectangle<float>(r.getX() + 4.0f,
            r.getBottom() - (i + 1) * segmentH - i * gap,
            r.getWidth() - 8.0f, segmentH);
        const auto hot = i > 12 ? juce::Colour(0xfff3fbff)
                                : i > 9 ? juce::Colour(0xffcceeff) : amber;
        g.setColour(lit ? (polished?juce::Colour(0xffc8d8dc):hot) : juce::Colour(polished?0xff353c3e:0xff292522));
        g.fillRect(cell);
    }
    g.setColour(ivory);
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 10.5f, juce::Font::bold)));
    g.drawText(label, (int)r.getX(), (int)r.getBottom() + 2, (int)r.getWidth(), 14,
               juce::Justification::centred);
}

inline void drawAnalyzerPanel(juce::Graphics& g,juce::Rectangle<float> full,
    float peakHold,float leftPeak,float rightPeak,bool polished=false)
{
    // Absolute reference positions translated into the analyzer component.
    const auto origin=scalpel::reference::analyzer().toNearestInt().getPosition().toFloat();
    const auto local=[&](float x,float y,float w,float h){return scalpel::reference::rect(x,y,w,h).translated(-origin.x,-origin.y);};
    const auto display=local(1306,343,159,53);
    g.setColour(juce::Colour(0xff050808));g.fillRoundedRectangle(display,4);
    g.setColour(juce::Colour(0xff8a8d86));g.drawRoundedRectangle(display,4,.6f);
    g.setColour(scalpel::finish::text);g.setFont(scalpel::finish::font(13,false));
    g.drawText("PEAK (dBFS)",local(1305,315,164,24).toNearestInt(),juce::Justification::centred);
    g.setColour(juce::Colour(0xff05dbe9));
    scalpel::finish::number(g,peakHold<=-143?juce::String("-inf"):juce::String(peakHold,2),display,25);
    drawMeter(g,local(1486,265,34,239),leftPeak,"L",polished);
    drawMeter(g,local(1537,265,34,239),rightPeak,"R",polished);
    (void)full;
}
} // namespace scalpel::ui105

#pragma once
#include <JuceHeader.h>
#include "ScalpelDbScale.h"

namespace scalpel::dbaxis {
inline void draw(juce::Graphics& g,juce::Rectangle<float> plot){
    juce::Graphics::ScopedSaveState saved(g);
    const float right=plot.getRight()+gutterGap+labelRightOffset;
    // Native production treatment of the old gutter; never blit a reference
    // composite or cover the plot. Nodes are painted afterwards, unchanged.
    g.setColour(juce::Colour(0xff040708));
    g.fillRect(plot.getRight()+gutterGap,plot.getY()-6.f*logicalPerNativePixel,
               gutterWidth,plot.getHeight()+11.f*logicalPerNativePixel);
    auto labelFont=juce::Font(juce::FontOptions("Arial",labelFontHeight,juce::Font::bold));
    // CSS/SVG font-size is an em size; JUCE's legacy Windows font height
    // includes different ascent/descent metrics. Match the measured 8px
    // numeral cap in the 11px reference font, using its actual outline.
    juce::GlyphArrangement probe;probe.addLineOfText(labelFont,"0",0,0);
    juce::Path zeroOutline;probe.createPath(zeroOutline);
    if(const float height=zeroOutline.getBounds().getHeight();height>0)
        labelFont.setHeight(labelFont.getHeight()*labelInkHeight/height);
    auto unitFont=juce::Font(juce::FontOptions("Arial",unitFontHeight,juce::Font::bold));
    unitFont.setExtraKerningFactor(.4f/9.f);
    const auto text=[&](const juce::String& value,const juce::Font& font,float x,float y,bool centred){
        juce::GlyphArrangement glyphs;glyphs.addLineOfText(font,value,0,0);
        const auto advance=glyphs.getBoundingBox(0,glyphs.getNumGlyphs(),false);
        juce::Path outline;glyphs.createPath(outline);
        const auto ink=outline.getBounds();
        glyphs.draw(g,juce::AffineTransform::translation(x-(centred?advance.getCentreX():advance.getRight()),y-ink.getCentreY()));
    };
    g.setColour(juce::Colour(0xffc9bcaf));
    text("dB",unitFont,right,plot.getY()-unitElevation,true);
    for(const int db:labels){
        g.setColour(juce::Colour(db==0?0xfff6efe7:0xffe6e0d8));
        text(juce::String(db),labelFont,right,toY((float)db,plot.getY(),plot.getHeight()),false);
    }
}
}

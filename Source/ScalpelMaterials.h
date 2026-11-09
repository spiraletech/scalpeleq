#pragma once
#include <JuceHeader.h>

namespace scalpel::materials {
inline const juce::Colour chassis{0xff111317},deck{0xff0e0f12},graphite{0xff292d33};
inline const juce::Colour edge{0xff858e96},silver{0xffc3c7cb},ice{0xffa6eeff};
inline const juce::Colour white{0xfff1f3f5},muted{0xffa5adb6};
inline void hardware(juce::Graphics& g,juce::Rectangle<float> r,bool selected)
{
    g.setColour(juce::Colours::black.withAlpha(.9f));
    g.fillRoundedRectangle(r.translated(0,2).expanded(1),4);
    juce::ColourGradient face(juce::Colour(0xffe2e4e6),0,r.getY(),
                              juce::Colour(0xff858d94),0,r.getBottom(),false);
    face.addColour(.45,silver);
    g.setGradientFill(face);g.fillRoundedRectangle(r.reduced(1),4);
    g.setColour(edge);g.drawRoundedRectangle(r,4,1);
    g.setColour(juce::Colours::white.withAlpha(.65f));
    g.drawLine(r.getX()+5,r.getY()+2,r.getRight()-5,r.getY()+2,.8f);
    g.setColour(juce::Colour(0xff303439));
    g.drawLine(r.getX()+4,r.getBottom()-1,r.getRight()-4,r.getBottom()-1,1.2f);
    if(selected){g.setColour(ice.withAlpha(.75f));g.drawRoundedRectangle(r.reduced(1),3,1.2f);}
}
// A small, deterministic material tile. Construct once; reuse at every DPI.
inline const juce::Image& carbon()
{
    static const juce::Image tile=[] {
        juce::Image image(juce::Image::RGB,128,128,true);
        juce::Random random(1062026);
        for(int y=0;y<128;++y)for(int x=0;x<128;++x){
            const int weave=((x+y)%8<2)?3:0;
            const auto v=(juce::uint8)(12+weave+random.nextInt(4));
            image.setPixelAt(x,y,juce::Colour(v,v,(juce::uint8)(v+1)));
        }
        return image;
    }();
    return tile;
}
inline void surface(juce::Graphics& g,juce::Rectangle<float> bounds,float radius)
{
    juce::Graphics::ScopedSaveState saved(g);
    juce::Path clip;clip.addRoundedRectangle(bounds,radius);g.reduceClipRegion(clip);
    g.setTiledImageFill(carbon(),0,0,1.0f);g.fillRect(bounds);
    juce::ColourGradient lighting(juce::Colours::white.withAlpha(.012f),bounds.getX(),bounds.getY(),
                                 juce::Colours::black.withAlpha(.35f),bounds.getRight(),bounds.getBottom(),false);
    g.setGradientFill(lighting);g.fillRect(bounds);
}
}

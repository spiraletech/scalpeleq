#pragma once
#include <JuceHeader.h>

namespace scalpel::materials {
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
    juce::ColourGradient lighting(juce::Colours::white.withAlpha(.045f),bounds.getX(),bounds.getY(),
                                 juce::Colours::black.withAlpha(.35f),bounds.getRight(),bounds.getBottom(),false);
    g.setGradientFill(lighting);g.fillRect(bounds);
}
}

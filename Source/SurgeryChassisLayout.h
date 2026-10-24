#pragma once
#include <JuceHeader.h>
namespace surgery::rack {
// Logical-pixel sections; host DPI scales them uniformly. Metal never changes
// thickness as editor aspect ratio changes. Original artwork is 1536x1024.
inline constexpr float chassisScale=.75f,cornerSize=72.f;
inline juce::Rectangle<float> instrumentArea(int width,int height){
    return {80.f,40.f,(float)width-160.f,(float)height-100.f};
}
inline juce::Rectangle<float> engravingArea(int width){return {width*.5f-119.f,1.f,238.f,27.f};}
inline void drawChassis(juce::Graphics& g,const juce::Image& image,int width,int height){
    if(!image.isValid())return;
    g.setOpacity(1.f);
    // Source corners remain square and are never cropped or anisotropically
    // stretched. Their outer edges follow the actual editor bounds.
    auto section=[&](juce::Rectangle<float> destination,int x,int y,int w,int h){
        g.drawImage(image,juce::roundToInt(destination.getX()),juce::roundToInt(destination.getY()),
                    juce::roundToInt(destination.getWidth()),juce::roundToInt(destination.getHeight()),x,y,w,h);
    };
    section({72,72,width-144.f,height-144.f},96,96,1344,832); // dark interior only
    for(int right=0;right<2;++right)for(int bottom=0;bottom<2;++bottom)
        section({right?width-72.f:0.f,bottom?height-72.f:0.f,72,72},right?1440:0,bottom?928:0,96,96);
    // Tile the horizontal metal grain. No enlarged screw or stretched grain.
    auto horizontal=[&](float x,float end,float y,int sourceY){
        for(;x<end;x+=96.f){
            const float length=std::min(96.f,end-x);
            juce::Graphics::ScopedSaveState clipped(g);g.reduceClipRegion(juce::Rectangle<float>(x,y,length,72).toNearestInt());
            section({x,y,96,72},128,sourceY,128,96);
        }
    };
    const float plaqueLeft=width*.5f-142.5f;
    horizontal(72,plaqueLeft,0,0);
    section({plaqueLeft,0,285,72},580,0,380,96);
    horizontal(plaqueLeft+285,width-72.f,0,0);
    horizontal(72,width-72.f,height-72.f,928);
    // Preserve complete warning-stripe assemblies at their original aspect.
    // Only plain lower rail is tiled; corner fasteners remain at the corners.
    for(int right=0;right<2;++right){
        const float x=right?width-72.f:0.f;const int sx=right?1440:0;
        section({x,72,72,303},sx,96,96,404);
        for(float y=375;y<height-72;y+=96){
            juce::Graphics::ScopedSaveState clipped(g);
            g.reduceClipRegion(juce::Rectangle<float>(x,y,72,std::min(96.f,height-72.f-y)).toNearestInt());
            section({x,y,72,96},sx,600,96,128);
        }
    }
}
}

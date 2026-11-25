#pragma once
#include <JuceHeader.h>
#include "ScalpelOmnisModel.h"
#include "ScalpelAnalyzeGlyphs.h"

namespace scalpel::omnis {
inline constexpr float designWidth=1280,designHeight=718;
inline juce::Rectangle<float> header{8,8,1264,64},strip{8,80,1264,62},
    analyzer{8,150,1264,296},analyzerInner{16,158,1248,280},viewport{119,165,921,255},
    touchscreen{8,462,1264,250},touchInner{12,466,1256,242},content{16,538,1248,166};
struct Layout {
    float scale,ox,oy;
    Layout(int w,int h):scale(std::min(w/designWidth,h/designHeight)),
        ox((w-designWidth*scale)*.5f),oy((h-designHeight*scale)*.5f){}
    juce::AffineTransform transform() const {return juce::AffineTransform::scale(scale).translated(ox,oy);}
    juce::Rectangle<int> physical(juce::Rectangle<float> r) const {return r.transformedBy(transform()).toNearestInt();}
};
// Resolve names once on the message thread. Linux diagnostics explicitly
// report the fallback; production Arial metrics require Windows verification.
inline juce::String face(bool black=false) {
    static const auto names=juce::Font::findAllTypefaceNames();
    for(const auto* n:black?std::array<const char*,3>{"Arial Black","Arial","Helvetica"}:
                              std::array<const char*,3>{"Arial","Helvetica","Liberation Sans"})
        if(names.contains(n,true))return n;
    if(names.contains("Liberation Sans",true))return "Liberation Sans";
    return juce::Font::getDefaultSansSerifFontName();
}
inline juce::Font font(float size,bool bold=false,float tracking=0,bool black=false){
    juce::Font f(juce::FontOptions(face(black),size,bold?juce::Font::bold:juce::Font::plain));
    f.setExtraKerningFactor(tracking/size);return f;
}
inline void text(juce::Graphics& g,const juce::String& s,juce::Rectangle<float> r,
                 float size,float brightness,bool bold=false,float tracking=0,int align=0,bool black=false,float sx=1){
    juce::GlyphArrangement a;a.addLineOfText(font(size,bold,tracking,black),s,0,0);
    auto b=a.getBoundingBox(0,-1,true);const float x=align<0?r.getX():align>0?r.getRight()-b.getWidth()*sx:r.getCentreX()-b.getWidth()*sx*.5f;
    g.setColour(juce::Colours::white.withAlpha(brightness));
    a.draw(g,juce::AffineTransform::scale(sx,1).translated(x-b.getX()*sx,r.getCentreY()-b.getCentreY()));
}
struct Material {float depth,shadow,absorption,roughness,reflection,edge,curvature,thickness,internal,bevel,parallax;};
inline constexpr Material analyzerMaterial{38,60,50,57,3,38,29,77,73,2,38},
    touchMaterial{14,91,47,64,3,39,17,72,21,39,24},
    chassisMaterial{41,75,30,47,11,42,17,87,77,15,22},
    controlMaterial{44,66,54,61,5,36,12,39,46,0,20};
struct Illumination {float direction=222,ambient=.34f,strength=.43f,temperature=.53f;};
inline constexpr Illumination illumination{};
inline void material(juce::Graphics& g,juce::Rectangle<float> r,const Material& m,float radius){
    // Artistic percentage translation, not a physically calibrated shader.
    g.setColour(juce::Colours::black.withAlpha(m.shadow*.01f));g.fillRoundedRectangle(r,radius);
    auto f=r.reduced(2);const float tone=(.025f+(100-m.absorption)*.00035f)*(.75f+illumination.ambient);
    const auto grey=[](float x){const float t=(illumination.temperature-.5f)*.06f;
        return juce::Colour::fromFloatRGBA(juce::jlimit(0.f,1.f,x+t),x,juce::jlimit(0.f,1.f,x-t),1);};
    juce::ColourGradient base(grey(tone+m.internal*.00055f),f.getX(),f.getY(),
                               grey(tone*.38f),f.getRight(),f.getBottom(),false);
    base.addColour(.30+m.curvature*.003f,grey(tone*.7f));g.setGradientFill(base);g.fillRoundedRectangle(f,radius-1);
    g.setColour(juce::Colours::white.withAlpha(m.edge*.003f));g.drawRoundedRectangle(f,radius-1,.65f+m.bevel*.006f);
    juce::Graphics::ScopedSaveState saved(g);juce::Path clip;clip.addRoundedRectangle(f,radius-1);g.reduceClipRegion(clip);
    // Shared light at 222 degrees; broad diffuse field, no isolated shine stripe.
    const float light=(illumination.direction-180.f)*juce::MathConstants<float>::pi/180.f;
    const float diffuse=1.f+m.roughness*.004f;
    const float opticalOffset=m.parallax*.012f; // Registered static optical offset, not head tracking.
    juce::ColourGradient reflection(juce::Colours::white.withAlpha(m.reflection*.004f*illumination.strength),
        f.getX()-std::cos(light)*f.getWidth()*.2f*diffuse+opticalOffset,f.getY()-std::sin(light)*f.getHeight()*.2f*diffuse+opticalOffset,
        juce::Colours::transparentWhite,f.getRight(),f.getBottom(),true);
    g.setGradientFill(reflection);g.fillRect(f);
    g.setColour(juce::Colours::black.withAlpha(m.depth*.002f));g.drawRoundedRectangle(f.reduced(1),radius-2,.6f+m.thickness*.008f);
}
inline void rotary(juce::Graphics& g,juce::Point<float> c,float radius,double value,bool enabled){
    const float s=radius/59.f;const int lit=litSegments(value);
    juce::Graphics::ScopedSaveState saved(g);g.addTransform(juce::AffineTransform::scale(s).translated(c.x,c.y));
    for(int i=0;i<33;++i){const auto a=angle(i/32.);const float x=(float)std::cos(a),y=(float)std::sin(a);
        g.setColour((i<lit?juce::Colour(0xffb72835).withAlpha(.86f):juce::Colour(0xff36373a)).withMultipliedAlpha(enabled?1.f:.35f));
        g.drawLine(x*49,y*49,x*59,y*59,3.5f);
    }
    const auto a=angle(value);const float x=(float)std::cos(a),y=(float)std::sin(a);
    g.setColour(juce::Colours::white.withAlpha(enabled?.87f:.30f));g.drawLine(x*39,y*39,x*60,y*60,1.5f);
}
inline void number(juce::Graphics& g,const juce::String& source,juce::Rectangle<float> r,bool enabled){
    auto numeric=source.retainCharacters("0123456789.+-"),unitText=source.removeCharacters("0123456789.+- ");
    // Fixed tabular cells and fixed unit anchor: signs/digit count do not move units.
    static const auto numericFont=font(24,true,1);
    static const float advance=numericFont.getStringWidthFloat("00")-numericFont.getStringWidthFloat("0");
    const float anchor=r.getCentreX()+40.f;
    g.setColour(juce::Colours::white.withAlpha(enabled?.87f:.30f));
    for(int i=0;i<numeric.length();++i){
        juce::GlyphArrangement a;a.addLineOfText(numericFont,numeric.substring(i,i+1),0,0);
        const auto b=a.getBoundingBox(0,-1,true);
        a.draw(g,juce::AffineTransform::translation(anchor-(numeric.length()-i)*advance+(advance-b.getWidth())*.5f-b.getX(),r.getY()+18));
    }
    juce::GlyphArrangement units;units.addLineOfText(font(10),unitText,anchor+5,r.getY()+18);units.draw(g);
}
inline bool navigation(const juce::String& t){return t=="BAND MODE"||t=="CARVE"||t=="ANALYZE";}
inline void oled(juce::Graphics& g,juce::Button& b,bool over,bool down){
    auto r=b.getLocalBounds().toFloat().reduced(.8f);const auto t=b.getButtonText();
    const bool power=t=="ON"||t=="OFF",graphPower=t=="GRAPH POWER",nav=navigation(t),master=b.getComponentID().startsWith("master-output-");
    const bool active=b.getToggleState()||down;const bool enabled=b.isEnabled();
    if(nav){
        juce::Path p;const float cut=6;p.startNewSubPath(r.getX()+cut,r.getY());p.lineTo(r.getRight(),r.getY());p.lineTo(r.getRight(),r.getBottom()-cut);p.lineTo(r.getRight()-cut,r.getBottom());p.lineTo(r.getX(),r.getBottom());p.lineTo(r.getX(),r.getY()+cut);p.closeSubPath();
        g.setColour(juce::Colours::white.withAlpha(active?.045f:over?.025f:.008f));g.fillPath(p);
        g.setColour(juce::Colours::white.withAlpha(active?.28f:.07f));g.strokePath(p,juce::PathStrokeType(.75f));
    }else{
        if(active||over){g.setColour(juce::Colours::white.withAlpha(active?.023f:.014f));g.fillRoundedRectangle(r,power?14.67f:(master||graphPower)?8.5f:12.f);}
        g.setColour(juce::Colours::white.withAlpha(!enabled?.025f:active?(power?.06f:master?.47f:.36f):.025f));
        g.drawRoundedRectangle(r,power?14.67f:(master||graphPower)?8.5f:12.f,1.5f);
    }
    const float alpha=!enabled?.12f:active?(power||nav?1.f:master?.88f:.86f):power?.12f:nav?.43f:master?.23f:.33f;
    if(t=="GRAPH POWER"){
        const auto c=r.getCentre();juce::Path arc;arc.addCentredArc(c.x,c.y,7,7,0,.65f,juce::MathConstants<float>::twoPi-.65f,true);
        g.setColour(juce::Colours::white.withAlpha(active?.88f:.23f));g.strokePath(arc,juce::PathStrokeType(1.4f));g.drawLine(c.x,c.y-10,c.x,c.y-1,1.4f);return;
    }
    if(t=="HEAT MAP"||t=="FREEZE"||t=="RESUME"){
        // Retained native contours pending recovery of exact LAB-043/044 paths.
        const bool heat=t=="HEAT MAP";auto p=heat?analyzeGlyphs::flame():analyzeGlyphs::snowflake();
        p.applyTransform(p.getTransformToScaleToFit({r.getCentreX()-13,r.getCentreY()-13,26,26},true));
        g.setColour(juce::Colours::white.withAlpha(!enabled?.10f:active?(heat?.94f:.90f):(heat?.17f:.22f)));g.fillPath(p);return;
    }
    text(g,t.startsWith("PHASE")?"PHASE":t,r,nav?20.f:master?10.5f:12.f,alpha,power||nav||master,nav?2.33f:0.f);
    if(b.hasKeyboardFocus(false)){g.setColour(juce::Colours::white.withAlpha(.45f));g.drawRoundedRectangle(r.reduced(2),6,.8f);}
}
inline void chassis(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff08090a));material(g,{0,0,1280,718},chassisMaterial,16);
    material(g,header,chassisMaterial,8);material(g,strip,controlMaterial,12);
    material(g,analyzer,analyzerMaterial,12);material(g,touchscreen,touchMaterial,12);
    material(g,touchInner,touchMaterial,8);
    text(g,"SCALPEL EQ",{32,8,850,64},45,.75f,true,4,-1,true,1.0148f);
    text(g,"SURGICAL EQUALIZER",{850,8,398,64},18,.88f,true,3,1);
}
}

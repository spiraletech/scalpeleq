#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include "ScalpelReferenceLayout.h"
// Proposed ivory 1.0.7 material masters. Rendering owns no processing state.
namespace scalpel::finish {
inline constexpr bool validatedAllPages=true;
inline bool enabledForMode(int) noexcept {return true;}
inline const juce::Colour chassis{0xffebd7c7},faceplate{0xff303131},deck{0xffebd7c7};
inline const juce::Colour analyzer{0xff080c0e},graphite{0xff252627},titanium{0xffa7a6a2};
inline const juce::Colour silver{0xfff0dece},text{0xffece0d5},secondary{0xffc7b8ab};
inline const juce::Colour ice{0xfff9f1e8},accent{0xfff9f1e8},ink{0xff161a1e};
inline const juce::Colour onGreen{0xff8cd848},offRed{0xffef4845},pointerWhite{0xfff4f4f2};
inline juce::Font font(float size,bool bold=true){return juce::Font(juce::FontOptions("Segoe UI",size,bold?juce::Font::bold:juce::Font::plain));}
// Software-backed immutable microtexture, created once, safe at DLL unload.
inline const juce::Image& grain(){
    static const juce::Image tile=[]{
        auto image=juce::SoftwareImageType{}.convert(juce::Image(juce::Image::ARGB,128,128,true));
        juce::Random random(1071645);juce::Image::BitmapData pixels(image,juce::Image::BitmapData::writeOnly);
        for(int y=0;y<128;++y)for(int x=0;x<128;++x)
            pixels.setPixelColour(x,y,(random.nextBool()?juce::Colours::white:juce::Colours::black).withAlpha((juce::uint8)(2+random.nextInt(5))));
        return image;
    }();return tile;
}
inline void roughness(juce::Graphics& g,juce::Rectangle<float> r,float radius,float opacity=.65f){
    juce::Graphics::ScopedSaveState saved(g);juce::Path clip;clip.addRoundedRectangle(r,radius);g.reduceClipRegion(clip);
    g.setTiledImageFill(grain(),0,0,opacity);g.fillRect(r);
}
inline void scratches(juce::Graphics& g,juce::Rectangle<float> r,float radius){
    juce::Graphics::ScopedSaveState saved(g);juce::Path clip;clip.addRoundedRectangle(r,radius);g.reduceClipRegion(clip);
    juce::Random rng(107031);
    for(int i=0;i<70;++i){const float x=r.getX()+rng.nextFloat()*r.getWidth(),y=r.getY()+rng.nextFloat()*r.getHeight(),n=4+rng.nextFloat()*28;
        g.setColour((i%2?juce::Colours::white:ink).withAlpha(i%2?.10f:.045f));g.drawLine(x,y,x+n,y-n,.35f);}
}
inline void panel(juce::Graphics& g,juce::Rectangle<float> r,float radius,bool=false){
    g.setColour(juce::Colours::black.withAlpha(.62f));g.fillRoundedRectangle(r.expanded(1.5f).translated(0,3),radius+1);
    juce::ColourGradient base(silver,r.getX(),r.getY(),juce::Colour(0xffc7b8ab),r.getRight(),r.getBottom(),false);
    base.addColour(.25,chassis);base.addColour(.67,juce::Colour(0xffe6d1bf));g.setGradientFill(base);g.fillRoundedRectangle(r,radius);
    roughness(g,r.reduced(1),radius-1,.75f);scratches(g,r.reduced(1),radius-1);
    g.setColour(juce::Colour(0xfffff4e8).withAlpha(.90f));g.drawRoundedRectangle(r.reduced(.7f),radius-.5f,1);
    g.setColour(juce::Colour(0xff7d756e));g.drawRoundedRectangle(r,radius,.8f);
}
inline void shell(juce::Graphics& g,juce::Rectangle<float> r){
    juce::ColourGradient outer(juce::Colour(0xff4c4d4b),r.getX(),r.getY(),juce::Colour(0xff131719),r.getRight(),r.getBottom(),false);
    outer.addColour(.12,juce::Colour(0xff242729));g.setGradientFill(outer);g.fillRect(r);panel(g,r.reduced(7),22);
    g.setColour(juce::Colour(0xffe5e0d8));g.drawRoundedRectangle(r.reduced(3),25,1.2f);
    g.setColour(juce::Colour(0xff0c0e0f));g.drawRoundedRectangle(r.reduced(5),23,1);
}
inline void hardware(juce::Graphics& g,juce::Rectangle<float> r,bool selected,bool hover,bool pressed){
    g.setColour(juce::Colours::black.withAlpha(.72f));g.fillRoundedRectangle(r.translated(0,3).expanded(.8f),6);
    juce::ColourGradient face(selected?juce::Colour(0xffffe6bf):silver,r.getX(),r.getY(),
        selected?juce::Colour(0xffe0bc99):juce::Colour(0xffcbb9a8),r.getX(),r.getBottom(),false);
    face.addColour(.18,selected?juce::Colour(0xffffdfae):juce::Colour(0xffead8c8));
    face.addColour(.76,selected?juce::Colour(0xffedc5a4):juce::Colour(0xffdfcbb8));
    g.setGradientFill(face);g.fillRoundedRectangle(r,6);roughness(g,r.reduced(1),5,.6f);
    g.setColour(juce::Colour(0xfffff4e8).withAlpha(.85f));g.drawRoundedRectangle(r.reduced(.7f),5.5f,.8f);
    g.setColour(juce::Colour(0xff5b554f));g.drawRoundedRectangle(r,6,.8f);
    if(hover||pressed){g.setColour((pressed?ink:juce::Colours::white).withAlpha(pressed?.07f:.04f));g.fillRoundedRectangle(r.reduced(1.5f),5);}
}
inline void darkButton(juce::Graphics& g,juce::Rectangle<float> r,bool selected,bool hover,bool lamp=false,juce::Colour lampColour=onGreen){
    g.setColour(juce::Colours::black.withAlpha(.75f));g.fillRoundedRectangle(r.translated(0,3).expanded(1),5);
    juce::ColourGradient face(juce::Colour(0xff444444),r.getX(),r.getY(),juce::Colour(0xff080b0c),r.getX(),r.getBottom(),false);
    face.addColour(.20,faceplate);face.addColour(.66,juce::Colour(0xff191b1b));g.setGradientFill(face);g.fillRoundedRectangle(r,5);
    roughness(g,r.reduced(1),4,.45f);
    juce::ColourGradient edge(juce::Colour(0xffb9b7b1),r.getX(),r.getY(),juce::Colour(0xff3d4142),r.getRight(),r.getBottom(),false);
    g.setGradientFill(edge);g.drawRoundedRectangle(r,5,.9f);
    g.setColour(juce::Colours::white.withAlpha(.22f));g.drawLine(r.getX()+6,r.getY()+2,r.getRight()-6,r.getY()+2,.7f);
    if(lamp){const auto light=juce::Rectangle<float>(r.getCentreX()-17,r.getY()+5,34,3.5f);
        g.setColour(juce::Colour(0xff131515));g.fillRoundedRectangle(light.expanded(1.5f,1),3);
        if(selected){g.setColour(lampColour.withAlpha(.15f));g.fillRoundedRectangle(light.expanded(2,1),3);}
        g.setColour(selected?lampColour:lampColour.darker(.82f));g.fillRoundedRectangle(light,2);}
    if(hover){g.setColour(juce::Colours::white.withAlpha(.04f));g.fillRoundedRectangle(r.reduced(2),4);}
}
inline void tab(juce::Graphics& g,juce::Rectangle<float> r,bool selected,bool hover){
    darkButton(g,r,false,hover);
    if(selected){const auto light=juce::Rectangle<float>(r.getX()+r.getWidth()*.16f,r.getBottom()-7,r.getWidth()*.74f,4.5f);
        g.setColour(ice.withAlpha(.12f));g.fillRoundedRectangle(light.expanded(1.5f,1),3);g.setColour(ice);g.fillRoundedRectangle(light,2.25f);}
}
inline void screw(juce::Graphics& g,juce::Point<float> c){
    const float radius=7.5f;const auto r=juce::Rectangle<float>(c.x-radius,c.y-radius,2*radius,2*radius);
    g.setColour(juce::Colours::black.withAlpha(.75f));g.fillEllipse(r.expanded(1).translated(0,1.5f));
    juce::ColourGradient head(juce::Colour(0xfff1efea),c.x-3,c.y-5,juce::Colour(0xff555c60),c.x+5,c.y+6,false);
    head.addColour(.4,juce::Colour(0xffb6b9b8));g.setGradientFill(head);g.fillEllipse(r);
    g.setColour(juce::Colour(0xff252a2c));g.drawEllipse(r,.65f);
    juce::Path cross;cross.addRoundedRectangle(c.x-4,c.y-1.15f,8,2.3f,.6f);cross.addRoundedRectangle(c.x-1.15f,c.y-4,2.3f,8,.6f);
    cross.applyTransform(juce::AffineTransform::rotation(.65f,c.x,c.y));g.setColour(juce::Colour(0xff101516));g.fillPath(cross);
}
inline void screws(juce::Graphics& g,juce::Rectangle<float> r,float xInset,float yInset){
    for(auto c:scalpel::reference::screwCentres(r,xInset,yInset))screw(g,c);
}
inline void analyzerHousing(juce::Graphics& g,juce::Rectangle<float> r){
    g.setColour(juce::Colours::black.withAlpha(.7f));g.fillRoundedRectangle(r.expanded(2).translated(0,2),16);
    juce::ColourGradient rim(juce::Colour(0xffeee8e1),r.getX(),r.getY(),juce::Colour(0xff595b59),r.getRight(),r.getBottom(),false);
    rim.addColour(.40,juce::Colour(0xff969590));rim.addColour(.72,juce::Colour(0xfff0e9e1));g.setGradientFill(rim);g.fillRoundedRectangle(r,15);
    const auto inside=r.reduced(4);
    juce::ColourGradient dark(juce::Colour(0xff121516),inside.getX(),inside.getY(),juce::Colour(0xff030607),inside.getRight(),inside.getBottom(),false);
    dark.addColour(.23,juce::Colour(0xff07090a));g.setGradientFill(dark);g.fillRoundedRectangle(inside,11);
    g.setColour(juce::Colour(0xffece9e4));g.drawRoundedRectangle(r.reduced(1.2f),14,1);
    g.setColour(juce::Colour(0xff414849));g.drawRoundedRectangle(inside,11,.7f);
}
inline void knob(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,const juce::Slider&){
    const float cx=x+w*.5f,cy=y+h*.5f,radius=.46f*(float)std::min(w,h);
    const auto r=juce::Rectangle<float>(cx-radius,cy-radius,2*radius,2*radius);
    g.setColour(juce::Colours::black.withAlpha(.7f));g.fillEllipse(r.expanded(2).translated(1,3));
    juce::ColourGradient rim(juce::Colour(0xffd8d9d5),cx-radius,cy-radius,juce::Colour(0xff242829),cx+radius,cy+radius,false);
    rim.addColour(.38,juce::Colour(0xff727676));rim.addColour(.58,juce::Colour(0xffb9bbb8));rim.addColour(.78,juce::Colour(0xff555a59));
    g.setGradientFill(rim);g.fillEllipse(r);
    for(int i=0;i<128;++i){const float a=i*juce::MathConstants<float>::twoPi/128;
        g.setColour((i%2?juce::Colour(0xff0d1112):juce::Colour(0xffdde0dc)).withAlpha(.65f));
        g.drawLine(cx+std::sin(a)*(radius-3.5f),cy+std::cos(a)*(radius-3.5f),cx+std::sin(a)*(radius-.5f),cy+std::cos(a)*(radius-.5f),.55f);}
    g.setColour(juce::Colour(0xff090c0d));g.fillEllipse(r.reduced(4));const auto face=r.reduced(6);
    juce::ColourGradient radial(juce::Colour(0xff404244),cx-radius*.3f,cy-radius*.45f,juce::Colour(0xff111516),cx+radius*.6f,cy+radius*.75f,true);
    radial.addColour(.4,graphite);g.setGradientFill(radial);g.fillEllipse(face);
    for(float inset=7;inset<radius-2;inset+=1.1f){g.setColour(juce::Colours::white.withAlpha(.035f));g.drawEllipse(r.reduced(inset),.28f);}
    g.setColour(juce::Colour(0xff9c9f9d));g.drawEllipse(face,.7f);
    juce::Path pointer;pointer.addRectangle(cx-1.55f,cy-radius+7,3.1f,radius*.47f);
    pointer.applyTransform(juce::AffineTransform::rotation(start+position*(end-start),cx,cy));
    g.setColour(pointerWhite);g.fillPath(pointer); // Printed, non-emissive inlay; no arc.
}
// Explicit tabular digit advances, retaining the same font as the labels.
inline void number(juce::Graphics& g,const juce::String& value,juce::Rectangle<float> r,float size=20){
    auto f=font(size);const float cell=f.getStringWidthFloat("0");
    float width=0;for(auto c:value)width+=juce::CharacterFunctions::isDigit(c)?cell:f.getStringWidthFloat(juce::String::charToString(c));
    if(width>r.getWidth()){f=font(size*r.getWidth()/width);}
    const float digit=f.getStringWidthFloat("0");width=0;
    for(auto c:value)width+=juce::CharacterFunctions::isDigit(c)?digit:f.getStringWidthFloat(juce::String::charToString(c));
    float x=r.getCentreX()-width*.5f;
    const float baseline=r.getCentreY()+(f.getAscent()-f.getDescent())*.5f;
    g.setFont(f);
    for(auto c:value){
        const auto ch=juce::String::charToString(c);const float advance=juce::CharacterFunctions::isDigit(c)?digit:f.getStringWidthFloat(ch);
        juce::GlyphArrangement glyph;glyph.addLineOfText(f,ch,x+(advance-f.getStringWidthFloat(ch))*.5f,baseline);glyph.draw(g);x+=advance;
    }
}
}

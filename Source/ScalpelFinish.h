#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
#include <algorithm>
#include <cmath>
#include "ScalpelReferenceLayout.h"
// Formidable 1.0.7.1 material masters. Rendering owns no processing state.
namespace scalpel::finish {
inline constexpr bool validatedAllPages=true;
inline bool enabledForMode(int) noexcept {return true;}
inline const juce::Colour chassis{0xffb6a493},faceplate{0xff292522},deck{0xffb6a493};
inline const juce::Colour analyzer{0xff0b0b0a},graphite{0xff292522},titanium{0xffa89a89};
inline const juce::Colour silver{0xffccb7a5},text{0xffe6d4bc},secondary{0xffb8a794};
inline const juce::Colour ice{0xfff0ebe1},accent{0xffd5b18a},ink{0xff151310};
inline const juce::Colour onGreen{0xff88db32},offRed{0xffe94a45},pointerWhite{0xfff0ebe1};
inline juce::Font font(float size,bool bold=false){return juce::Font(juce::FontOptions("Segoe UI",size,bold?juce::Font::bold:juce::Font::plain));}
// Software-backed immutable microtexture, created once, safe at DLL unload.
inline const juce::Image& grain(){
    // The kit's MATERIAL_001 is texture only, with no lettering or controls.
    // Mirror its edges once to remove hard seams; derive neutral relief so the
    // same owned asset can shade warm polymer and graphite without an overlay.
    static const juce::Image tile=[]{
        const auto source=juce::ImageFileFormat::loadFrom(BinaryData::FormidableShell_png,BinaryData::FormidableShell_pngSize);
        const int w=source.getWidth(),h=source.getHeight();
        auto image=juce::SoftwareImageType{}.convert(juce::Image(juce::Image::ARGB,w*2,h*2,true));
        juce::Image::BitmapData pixels(image,juce::Image::BitmapData::writeOnly);
        for(int y=0;y<h*2;++y)for(int x=0;x<w*2;++x){
            const auto c=source.getPixelAt(x<w?x:2*w-1-x,y<h?y:2*h-1-y);
            const float relief=c.getPerceivedBrightness()-.523f;
            pixels.setPixelColour(x,y,(relief>0?juce::Colours::white:juce::Colours::black).withAlpha(juce::jmin(.38f,std::abs(relief)*1.25f)));
        }
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
inline void panel(juce::Graphics& g,juce::Rectangle<float> r,float radius,bool outer=false){
    g.setColour(juce::Colours::black.withAlpha(.70f));g.fillRoundedRectangle(r.expanded(1.5f).translated(0,3),radius+1);
    juce::ColourGradient base(juce::Colour(outer?0xff9f8c75:0xffc8ae98),r.getX(),r.getY(),
        juce::Colour(outer?0xff76604b:0xff746353),r.getRight(),r.getBottom(),false);
    if(outer){base.addColour(.15,juce::Colour(0xff96816c));base.addColour(.55,juce::Colour(0xff9c8978));base.addColour(.72,juce::Colour(0xffad967f));}
    else {base.addColour(.23,juce::Colour(0xffb29b86));base.addColour(.56,juce::Colour(0xffa38c77));base.addColour(.78,juce::Colour(0xffbea58f));}
    g.setGradientFill(base);g.fillRoundedRectangle(r,radius);
    roughness(g,r.reduced(1),radius-1,.72f);
    g.setColour(juce::Colour(0xffeadaca));g.drawRoundedRectangle(r.reduced(1),radius-1,.8f);
    g.setColour(juce::Colour(0xff594c40));g.drawRoundedRectangle(r,radius,.8f);
    g.setColour(juce::Colour(0xffeee3d5).withAlpha(.55f));g.drawRoundedRectangle(r.reduced(2.8f),radius-2.5f,.5f);
}
inline void shell(juce::Graphics& g,juce::Rectangle<float> r){
    g.setColour(juce::Colour(0xff100f0d));g.fillRect(r);
    const auto outer=scalpel::reference::enclosure().reduced(1.5f);
    panel(g,outer,39,true);
    g.setColour(juce::Colour(0xfff0e5d8));g.drawRoundedRectangle(outer.reduced(4),35,1.2f);
    g.setColour(juce::Colour(0xff5e5347));g.drawRoundedRectangle(outer.reduced(7),32,.8f);
    // Rejected footer plaque and perforations are deliberately absent.
}
inline void hardware(juce::Graphics& g,juce::Rectangle<float> r,bool selected,bool hover,bool pressed){
    g.setColour(juce::Colours::black.withAlpha(.85f));g.fillRoundedRectangle(r.translated(0,3).expanded(1),7);
    juce::ColourGradient rim(juce::Colour(0xffe1cbb4),r.getX(),r.getY(),juce::Colour(0xff514339),r.getRight(),r.getBottom(),false);
    rim.addColour(.42,titanium);rim.addColour(.73,juce::Colour(0xffdcbea0));g.setGradientFill(rim);g.fillRoundedRectangle(r,7);
    const auto face=r.reduced(3).translated(0,pressed?1.f:0.f);
    juce::ColourGradient material(selected?juce::Colour(0xffe7c9a0):juce::Colour(0xffc7ae96),face.getX(),face.getY(),
        selected?juce::Colour(0xffba8e5b):juce::Colour(0xff806852),face.getRight(),face.getBottom(),false);
    material.addColour(.16,selected?juce::Colour(0xffd5b18a):juce::Colour(0xffb39c87));
    material.addColour(.52,selected?juce::Colour(0xffba946a):juce::Colour(0xff9b826b));
    material.addColour(.82,selected?juce::Colour(0xffd3ad7c):juce::Colour(0xffab8f74));
    g.setGradientFill(material);g.fillRoundedRectangle(face,4.8f);roughness(g,face,4.8f,.62f);
    g.setColour(juce::Colour(0xfff3e3cf).withAlpha(pressed?.25f:.72f));g.drawRoundedRectangle(face.reduced(.5f),4.3f,.75f);
    g.setColour(juce::Colour(0xff56483b));g.drawRoundedRectangle(r.reduced(1.5f),5.8f,.6f);
    if(pressed){g.setColour(ink.withAlpha(.12f));g.drawRoundedRectangle(face.reduced(1),4,1.5f);}
    if(hover){g.setColour(juce::Colours::white.withAlpha(.035f));g.fillRoundedRectangle(face,4.8f);}
}
inline void darkButton(juce::Graphics& g,juce::Rectangle<float> r,bool selected,bool hover,bool lamp=false,juce::Colour lampColour=onGreen,bool navigation=false){
    g.setColour(juce::Colours::black.withAlpha(.75f));g.fillRoundedRectangle(r.translated(0,3).expanded(1),5);
    juce::ColourGradient face(juce::Colour(navigation?0xff554a3e:0xff52463e),r.getX(),r.getY(),juce::Colour(0xff171411),r.getX(),r.getBottom(),false);
    face.addColour(.20,navigation?juce::Colour(0xff463c33):faceplate);face.addColour(.66,juce::Colour(navigation?0xff29231d:0xff211d19));g.setGradientFill(face);g.fillRoundedRectangle(r,5);
    roughness(g,r.reduced(1),4,.52f);
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
    darkButton(g,r,false,hover,false,onGreen,true);
    if(selected){const auto light=juce::Rectangle<float>(r.getX()+r.getWidth()*.20f,r.getBottom()-5,r.getWidth()*.63f,2.7f);
        g.setColour(accent.withAlpha(.16f));g.fillRoundedRectangle(light.expanded(1.5f,1),2);g.setColour(juce::Colour(0xffffe0a4));g.fillRoundedRectangle(light,1.35f);}
}
inline void screw(juce::Graphics& g,juce::Point<float> c){
    const float radius=7.1f;const auto r=juce::Rectangle<float>(c.x-radius,c.y-radius,2*radius,2*radius);
    g.setColour(juce::Colours::black.withAlpha(.75f));g.fillEllipse(r.expanded(1).translated(0,1.5f));
    juce::ColourGradient head(juce::Colour(0xffeee0cb),c.x-3,c.y-5,juce::Colour(0xff66523f),c.x+5,c.y+6,false);
    head.addColour(.4,juce::Colour(0xffb9a084));g.setGradientFill(head);g.fillEllipse(r);
    g.setColour(juce::Colour(0xff30271e));g.drawEllipse(r,.65f);
    juce::Path cross;cross.addRoundedRectangle(c.x-4,c.y-1.15f,8,2.3f,.6f);cross.addRoundedRectangle(c.x-1.15f,c.y-4,2.3f,8,.6f);
    cross.applyTransform(juce::AffineTransform::rotation(.65f,c.x,c.y));g.setColour(juce::Colour(0xff101516));g.fillPath(cross);
}
inline void screws(juce::Graphics& g,juce::Rectangle<float> r,float xInset,float yInset){
    for(auto c:scalpel::reference::screwCentres(r,xInset,yInset))screw(g,c);
}
inline void analyzerHousing(juce::Graphics& g,juce::Rectangle<float> r){
    g.setColour(juce::Colours::black.withAlpha(.7f));g.fillRoundedRectangle(r.expanded(2).translated(0,2),16);
    juce::ColourGradient rim(juce::Colour(0xffe4cfb7),r.getX(),r.getY(),juce::Colour(0xff55483c),r.getRight(),r.getBottom(),false);
    rim.addColour(.40,juce::Colour(0xff9e8872));rim.addColour(.72,juce::Colour(0xffd5b99b));g.setGradientFill(rim);g.fillRoundedRectangle(r,15);
    const auto inside=r.reduced(4);
    juce::ColourGradient dark(juce::Colour(0xff1a1713),inside.getX(),inside.getY(),juce::Colour(0xff080706),inside.getRight(),inside.getBottom(),false);
    dark.addColour(.23,juce::Colour(0xff100e0b));g.setGradientFill(dark);g.fillRoundedRectangle(inside,11);
    g.setColour(juce::Colour(0xffebd9c1));g.drawRoundedRectangle(r.reduced(1.2f),14,1);
    g.setColour(juce::Colour(0xff50443a));g.drawRoundedRectangle(inside,11,.7f);
}
inline const juce::Image& knobMaterial(){
    static const juce::Image texture=[]{
        auto image=juce::SoftwareImageType{}.convert(juce::Image(juce::Image::ARGB,512,512,true));
        juce::Image::BitmapData pixels(image,juce::Image::BitmapData::writeOnly);juce::Random rng(107252627);
        for(int y=0;y<512;++y)for(int x=0;x<512;++x){
            const float dx=(x-255.5f)/255.5f,dy=(y-255.5f)/255.5f,r=std::sqrt(dx*dx+dy*dy);
            if(r>1)continue;
            const float theta=std::atan2(dy,dx),direction=.5f+.5f*std::cos(theta*2-2.5f);
            const float machining=.025f*std::sin(r*440.f)+(rng.nextFloat()-.5f)*.009f;
            const float shade=(.065f+.31f*direction*(.15f+.85f*r)+machining)*(1-.24f*std::pow(r,8.f));
            pixels.setPixelColour(x,y,juce::Colour::fromFloatRGBA(shade*1.05f,shade,shade*.88f,1));
        }
        return image;
    }();return texture;
}
inline void knob(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,const juce::Slider&){
    const float cx=x+w*.5f,cy=y+h*.5f,radius=.485f*(float)std::min(w,h);
    const auto r=juce::Rectangle<float>(cx-radius,cy-radius,2*radius,2*radius);
    g.setColour(juce::Colours::black.withAlpha(.7f));g.fillEllipse(r.expanded(2).translated(1,3));
    juce::ColourGradient rim(juce::Colour(0xffe7d5b7),cx-radius,cy-radius,juce::Colour(0xff2b241c),cx+radius,cy+radius,false);
    rim.addColour(.38,juce::Colour(0xff88745d));rim.addColour(.58,juce::Colour(0xffbca487));rim.addColour(.78,juce::Colour(0xff61523f));
    g.setGradientFill(rim);g.fillEllipse(r);
    for(int i=0;i<128;++i){const float a=i*juce::MathConstants<float>::twoPi/128;
        g.setColour((i%2?juce::Colour(0xff0d1112):juce::Colour(0xfff0dfc5)).withAlpha(.65f));
        g.drawLine(cx+std::sin(a)*(radius-3.5f),cy+std::cos(a)*(radius-3.5f),cx+std::sin(a)*(radius-.5f),cy+std::cos(a)*(radius-.5f),.55f);}
    g.setColour(juce::Colour(0xff090c0d));g.fillEllipse(r.reduced(4));const auto face=r.reduced(6);
    g.setColour(graphite);g.fillEllipse(face);g.drawImage(knobMaterial(),face);
    g.setColour(juce::Colour(0xffa08c72));g.drawEllipse(face,.7f);
    juce::Path pointer;pointer.addRectangle(cx-1.55f,cy-radius+7,3.1f,radius*.47f);
    pointer.applyTransform(juce::AffineTransform::rotation(start+position*(end-start),cx,cy));
    g.setColour(pointerWhite);g.fillPath(pointer); // Printed, non-emissive inlay; no arc.
}
// Explicit tabular digit advances, retaining the same font as the labels.
inline void number(juce::Graphics& g,const juce::String& value,juce::Rectangle<float> r,float size=20){
    auto f=font(size,true);const float cell=f.getStringWidthFloat("0");
    float width=0;for(auto c:value)width+=juce::CharacterFunctions::isDigit(c)?cell:f.getStringWidthFloat(juce::String::charToString(c));
    if(width>r.getWidth()){f=font(size*r.getWidth()/width,true);}
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

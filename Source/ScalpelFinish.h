#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>

// Rendering only. All dimensions come from existing component bounds.
namespace scalpel::finish {
inline constexpr bool validatedAllPages=false; // CARVE/COLOR verification gate.
inline bool enabledForMode(int mode) noexcept {return validatedAllPages||mode==1||mode==2;}
inline const juce::Colour chassis{0xff121417},faceplate{0xff0b0e12},deck{0xff151719};
inline const juce::Colour analyzer{0xff030d19},graphite{0xff10151a},titanium{0xff9ea8b1};
inline const juce::Colour silver{0xffdee4e8},text{0xfff3f7fa},secondary{0xffaab4be};
inline const juce::Colour ice{0xffa6eeff},accent{0xff65ddf5};
inline juce::Font font(float size,bool bold=true){
    return juce::Font(juce::FontOptions("Segoe UI",size,bold?juce::Font::bold:juce::Font::plain));
}

// Software-backed immutable grain; no native resources survive DLL unload.
// Alpha carries roughness so the underlying material retains its own colour.
inline const juce::Image& grain(){
    static const juce::Image tile=[] {
        auto image=juce::SoftwareImageType{}.convert(juce::Image(juce::Image::ARGB,128,128,true));
        juce::Random random(106110);
        juce::Image::BitmapData pixels(image,juce::Image::BitmapData::writeOnly);
        for(int y=0;y<128;++y)for(int x=0;x<128;++x){
            const bool light=random.nextBool();
            const auto alpha=(juce::uint8)(2+random.nextInt(6));
            pixels.setPixelColour(x,y,(light?juce::Colours::white:juce::Colours::black).withAlpha(alpha));
        }
        return image;
    }();
    return tile;
}
inline void roughness(juce::Graphics& g,juce::Rectangle<float> r,float radius,float opacity=.65f){
    juce::Graphics::ScopedSaveState saved(g);
    juce::Path clip;clip.addRoundedRectangle(r,radius);g.reduceClipRegion(clip);
    g.setTiledImageFill(grain(),0,0,opacity);g.fillRect(r);
}
inline void panel(juce::Graphics& g,juce::Rectangle<float> r,float radius,bool carbon=false){
    g.setColour(juce::Colours::black.withAlpha(.88f));
    g.fillRoundedRectangle(r.expanded(2).translated(0,1.5f),radius+1);
    juce::ColourGradient base(carbon?deck:faceplate,r.getX(),r.getY(),
        juce::Colour(0xff05080c),r.getRight(),r.getBottom(),false);
    base.addColour(.18,carbon?juce::Colour(0xff191c20):juce::Colour(0xff12171d));
    base.addColour(.70,carbon?juce::Colour(0xff101316):faceplate);
    g.setGradientFill(base);g.fillRoundedRectangle(r,radius);
    roughness(g,r.reduced(1),radius-1,.60f);
    if(carbon){
        juce::Graphics::ScopedSaveState saved(g);
        juce::Path clip;clip.addRoundedRectangle(r.reduced(2),radius-2);g.reduceClipRegion(clip);
        g.setColour(silver.withAlpha(.019f));
        for(float x=r.getX()-r.getHeight();x<r.getRight();x+=4.f)
            g.drawLine(x,r.getBottom(),x+r.getHeight(),r.getY(),.45f);
    }
    juce::ColourGradient edge(titanium.withAlpha(.62f),r.getX(),r.getY(),
        juce::Colour(0xff30383f),r.getRight(),r.getBottom(),false);
    edge.addColour(.36,juce::Colour(0xff434c56));
    g.setGradientFill(edge);g.drawRoundedRectangle(r.reduced(.8f),radius-.6f,1.f);
    g.setColour(juce::Colours::black.withAlpha(.78f));g.drawRoundedRectangle(r,radius,1.f);
}
inline void shell(juce::Graphics& g,juce::Rectangle<float> r){
    juce::ColourGradient body(juce::Colour(0xff1d2228),r.getX(),r.getY(),
        juce::Colour(0xff090c10),r.getRight(),r.getBottom(),false);
    body.addColour(.12,chassis);body.addColour(.68,juce::Colour(0xff0e1115));
    g.setGradientFill(body);g.fillRect(r);
    roughness(g,r.reduced(6),22,.78f);
    juce::ColourGradient edge(silver.withAlpha(.72f),r.getX(),r.getY(),
        titanium.withAlpha(.25f),r.getRight(),r.getBottom(),false);
    edge.addColour(.45,titanium.withAlpha(.38f));
    g.setGradientFill(edge);g.drawRoundedRectangle(r.reduced(2),23,1.3f);
    g.setColour(juce::Colour(0xff010305).withAlpha(.98f));g.drawRoundedRectangle(r.reduced(5),20,1.f);
}
inline void hardware(juce::Graphics& g,juce::Rectangle<float> r,bool selected,bool hover,bool pressed){
    g.setColour(juce::Colours::black.withAlpha(.9f));g.fillRoundedRectangle(r.translated(0,2).expanded(1),4);
    juce::ColourGradient face(silver,r.getX(),r.getY(),juce::Colour(0xff626d77),r.getX(),r.getBottom(),false);
    face.addColour(.09,juce::Colour(0xffcdd5db));face.addColour(.32,juce::Colour(0xffbfc9d0));
    face.addColour(.61,juce::Colour(0xffaeb9c2));face.addColour(.90,titanium.darker(.25f));
    g.setGradientFill(face);g.fillRoundedRectangle(r.reduced(1),4);
    roughness(g,r.reduced(1.5f),3,.65f);
    juce::ColourGradient edge(silver,r.getX(),r.getY(),juce::Colour(0xff3e4852),r.getRight(),r.getBottom(),false);
    g.setGradientFill(edge);g.drawRoundedRectangle(r,4,1);
    g.setColour(silver.withAlpha(.70f));g.drawLine(r.getX()+5,r.getY()+2,r.getRight()-5,r.getY()+2,.8f);
    g.setColour(juce::Colour(0xff28313a));g.drawLine(r.getX()+4,r.getBottom()-1,r.getRight()-4,r.getBottom()-1,1.2f);
    if(selected){g.setColour(ice.withAlpha(.85f));g.drawRoundedRectangle(r.reduced(1),3,1.2f);}
    if(hover||pressed){g.setColour((pressed?juce::Colours::black:juce::Colours::white).withAlpha(pressed?.055f:.035f));g.fillRoundedRectangle(r.reduced(1.5f),3);}
}
inline void darkButton(juce::Graphics& g,juce::Rectangle<float> r,bool selected,bool hover,bool lamp=false){
    g.setColour(juce::Colours::black.withAlpha(.90f));g.fillRoundedRectangle(r.translated(0,2).expanded(1.2f),3);
    juce::ColourGradient face(juce::Colour(0xff1d252c),r.getX(),r.getY(),
        juce::Colour(0xff06090d),r.getX(),r.getBottom(),false);
    face.addColour(.22,juce::Colour(0xff11191f));face.addColour(.68,faceplate);
    g.setGradientFill(face);g.fillRoundedRectangle(r,3);
    roughness(g,r.reduced(1.5f),2,.35f);
    juce::ColourGradient edge(titanium.withAlpha(.65f),r.getX(),r.getY(),
        juce::Colour(0xff28323c),r.getRight(),r.getBottom(),false);
    g.setGradientFill(edge);g.drawRoundedRectangle(r,3,.95f);
    g.setColour(silver.withAlpha(.25f));g.drawLine(r.getX()+5,r.getY()+2,r.getRight()-5,r.getY()+2,.7f);
    if(selected){
        g.setColour(accent.withAlpha(.8f));g.drawRoundedRectangle(r.reduced(1.1f),2.6f,1.15f);
        if(lamp){
            const auto light=juce::Rectangle<float>(r.getCentreX()-17,r.getY()+5,34,4);
            g.setColour(ice.withAlpha(.12f));g.fillRoundedRectangle(light.expanded(2,1),3);
            g.setColour(ice);g.fillRoundedRectangle(light,2);
        }
    }
    if(hover){g.setColour(silver.withAlpha(.035f));g.fillRoundedRectangle(r.reduced(2),2);}
}
inline void tab(juce::Graphics& g,juce::Rectangle<float> r,bool selected,bool hover){
    darkButton(g,r,false,hover);
    if(selected){
        const auto light=juce::Rectangle<float>(r.getX()+25,r.getBottom()-7,r.getWidth()-50,5);
        g.setColour(ice.withAlpha(.14f));g.fillRoundedRectangle(light.expanded(1.5f,1),3);
        g.setColour(ice);g.fillRoundedRectangle(light,2.5f);
    }
}
inline void knob(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,
                 const juce::Slider& slider){
    const float cx=x+w*.5f,cy=y+h*.46f;
    const float radius=juce::jlimit(14.f,44.f,.48f*(float)std::min(w,h));
    const auto face=juce::Rectangle<float>(cx-radius,cy-radius,2*radius,2*radius);
    g.setColour(juce::Colours::black.withAlpha(.72f));g.fillEllipse(face.translated(1.5f,3).expanded(1.5f));
    juce::ColourGradient rim(silver.withAlpha(.88f),cx-radius*.8f,cy-radius,
        juce::Colour(0xff151c24),cx+radius,cy+radius,false);
    rim.addColour(.28,titanium);rim.addColour(.49,juce::Colour(0xff3d4852));rim.addColour(.75,juce::Colour(0xff17222b));
    g.setGradientFill(rim);g.fillEllipse(face.expanded(1.4f));
    g.setColour(juce::Colour(0xff03070a));g.fillEllipse(face);
    const auto inner=face.reduced(4.5f);
    juce::ColourGradient radial(juce::Colour(0xff253039),cx-radius*.25f,cy-radius*.32f,
        juce::Colour(0xff070b10),cx+radius*.75f,cy+radius*.65f,true);
    radial.addColour(.48,graphite);radial.addColour(.82,juce::Colour(0xff0d1217));
    g.setGradientFill(radial);g.fillEllipse(inner);
    {
        juce::Graphics::ScopedSaveState saved(g);juce::Path clip;clip.addEllipse(inner);g.reduceClipRegion(clip);
        // Very faint directional reflections across the concentric machining.
        juce::ColourGradient reflection(silver.withAlpha(.065f),cx-radius*.7f,cy-radius,
            juce::Colours::transparentWhite,cx+radius*.2f,cy+radius*.45f,false);
        g.setGradientFill(reflection);g.fillEllipse(inner);
        for(float inset=5.5f;inset<radius-2;inset+=1.15f){
            g.setColour(silver.withAlpha(.025f));g.drawEllipse(face.reduced(inset),.30f);
        }
    }
    g.setColour(titanium.withAlpha(.60f));g.drawEllipse(inner,.85f);
    juce::Path track;track.addCentredArc(cx,cy,radius+3,radius+3,0,start,end,true);
    g.setColour(titanium.withAlpha(.30f));g.strokePath(track,juce::PathStrokeType(2));
    const float angle=start+position*(end-start);
    const auto& range=slider.getNormalisableRange();
    const float home=juce::jlimit(0.f,1.f,(float)range.convertTo0to1(slider.getDoubleClickReturnValue()));
    if(std::abs(position-home)>.002f){
        juce::Path active;const float homeAngle=start+home*(end-start);
        active.addCentredArc(cx,cy,radius+3,radius+3,0,std::min(homeAngle,angle),std::max(homeAngle,angle),true);
        g.setColour(accent.withAlpha(.5f));g.strokePath(active,juce::PathStrokeType(2.0f));
    }
    juce::Path pointer;pointer.addRoundedRectangle(cx-1.5f,cy-radius+10,3,radius*.43f,1.5f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle,cx,cy));
    g.setColour(ice.withAlpha(.16f));g.strokePath(pointer,juce::PathStrokeType(3));
    g.setColour(ice);g.fillPath(pointer);
}
// Explicit tabular digit advances, retaining the same font as the labels.
inline void number(juce::Graphics& g,const juce::String& value,juce::Rectangle<float> r,float size=18){
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

#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
#include <algorithm>
#include <cmath>
#include "ScalpelReferenceLayout.h"
#include "ScalpelFormidableAssets.h"
// Formidable material masters. Rendering owns no processing state.
namespace scalpel::finish {
inline constexpr bool validatedAllPages=true;
inline bool enabledForMode(int) noexcept {return true;}
inline const juce::Colour chassis{0xff08090a},faceplate{0xff121416},deck{0xff050607};
inline const juce::Colour analyzer{0xff050607},graphite{0xff25282b},titanium{0xff777b7e};
inline const juce::Colour silver{0xffb7b9ba},text{0xfff2f2ef},secondary{0xff62666a};
inline const juce::Colour ice{0xfffafaf7},accent{0xffe31b27},ink{0xff08090a};
inline const juce::Colour onGreen{0xff8fdb51},offRed{0xffe64e49},pointerWhite{0xfff6f3eb};
inline juce::Font font(float size,bool bold=false){return juce::Font(juce::FontOptions("Segoe UI",size,bold?juce::Font::bold:juce::Font::plain));}
// Software-backed immutable microtexture, created once, safe at DLL unload.
inline const juce::Image& grain(){
    // Unified ANALYZE material sample, high-pass relief only: no baked lighting, text or controls.
    // Mirror its edges once to remove hard seams; derive neutral relief so the
    // same owned asset can shade warm polymer and graphite without an overlay.
    static const juce::Image tile=[]{
        const auto& source=formidable::image(formidable::Art::champagne_header);
        const int w=source.getWidth(),h=source.getHeight();
        float mean=0.f;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)
            mean+=source.getPixelAt(x,y).getPerceivedBrightness();
        mean/=(float)(w*h);
        auto image=juce::SoftwareImageType{}.convert(juce::Image(juce::Image::ARGB,w*2,h*2,true));
        juce::Image::BitmapData pixels(image,juce::Image::BitmapData::writeOnly);
        for(int y=0;y<h*2;++y)for(int x=0;x<w*2;++x){
            const auto c=source.getPixelAt(x<w?x:2*w-1-x,y<h?y:2*h-1-y);
            const float relief=c.getPerceivedBrightness()-mean;
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
inline void panel(juce::Graphics& g,juce::Rectangle<float> r,float radius,bool outer=false,
                  formidable::Art innerFace=formidable::Art::control_deck){
    g.setColour(juce::Colours::black.withAlpha(.70f));g.fillRoundedRectangle(r.expanded(1.5f).translated(0,3),radius+1);
    formidable::draw(g,outer?formidable::Art::shell:innerFace,r);
    if(outer)roughness(g,r.reduced(1),radius-1,.18f);
    g.setColour(juce::Colour(0xffd8d9d7).withAlpha(.48f));g.drawRoundedRectangle(r.reduced(1),radius-1,.8f);
    g.setColour(juce::Colour(0xff050607));g.drawRoundedRectangle(r,radius,.8f);
    g.setColour(juce::Colours::white.withAlpha(.10f));g.drawRoundedRectangle(r.reduced(2.8f),radius-2.5f,.5f);
}
inline void shell(juce::Graphics& g,juce::Rectangle<float> r){
    g.setColour(juce::Colour(0xff100f0d));g.fillRect(r);
    const auto outer=scalpel::reference::enclosure().reduced(1.5f);
    panel(g,outer,39,true);
    g.setColour(juce::Colour(0xffdadbd9).withAlpha(.65f));g.drawRoundedRectangle(outer.reduced(4),35,1.2f);
    g.setColour(juce::Colour(0xff111315));g.drawRoundedRectangle(outer.reduced(7),32,.8f);
    // Rejected footer plaque and perforations are deliberately absent.
}
inline int bandKeyIndex(const juce::String& label){
    if(label.length()!=6||!label.startsWith("BAND "))return -1;
    const auto digit=label[5];
    return digit>='1'&&digit<='7'?static_cast<int>(digit-'1'):-1;
}
inline void drawPageOrBandKey(juce::Graphics& g,juce::Rectangle<float> bounds,
                              const juce::Button& button,int column,bool page,bool over,bool down){
    // Atlas rows match the art handoff: disabled, pressed, selected, hover, idle.
    const int row=!button.isEnabled()?0:down?1:button.getToggleState()?2:over?3:4;
    const int width=page?1028:408;
    const int height=page?112:124;
    const auto& atlas=formidable::image(page?formidable::Art::mode_keys_1087:formidable::Art::band_keys_1087);
    const auto cell=atlas.getClippedImage({column*width,row*height,width,height});
    g.drawImage(cell,bounds,juce::RectanglePlacement::stretchToFit);
    if(button.hasKeyboardFocus(false)){
        g.setColour(juce::Colours::white.withAlpha(.72f));
        g.drawRoundedRectangle(bounds.reduced(2.f),4.f,.8f);
    }
}
// Native reconstruction of the kit's no-text SVG cap masters. One physical
// cap/bevel family for lower-deck controls; no baked labels or sprite margin
// is included in the hit rectangle. Parent cache owns the single shadow.
inline void flushCap(juce::Graphics& g,juce::Rectangle<float> r,bool graphiteCap,
                     bool selected,bool hover,bool pressed,bool off=false){
    if(!graphiteCap){
        using A=formidable::Art;
        formidable::draw(g,pressed?A::command_pressed:hover?A::command_hover:A::command_idle,r);
        roughness(g,r.reduced(3),4.0f,.25f);
        if(selected){
            g.setColour(accent.withAlpha(.8f));
            g.drawRoundedRectangle(r.reduced(2),5,1.2f);
        }else if(hover){
            g.setColour(juce::Colours::white.withAlpha(.07f));
            g.fillRoundedRectangle(r.reduced(3),4);
        }
        return;
    }
    const float s=reference::flushScale(),radius=12.f*s;
    g.setColour(juce::Colour(0xff27211d));g.fillRoundedRectangle(r,radius);
    juce::ColourGradient rim(juce::Colour(0xfff3dec8),r.getX(),r.getY(),juce::Colour(0xff4a3529),r.getRight(),r.getBottom(),false);
    g.setGradientFill(rim);g.drawRoundedRectangle(r.reduced(.6f),radius-.5f,1.1f*s);
    const auto face=r.reduced(3.f*s).translated(0,pressed?.5f*s:0.f);
    const auto top=juce::Colour(graphiteCap?0xff534b45:selected?0xfffbcb83:0xfff2d9be);
    const auto bottom=juce::Colour(graphiteCap?0xff201c1a:selected?0xffcc925c:0xffb48e76);
    juce::ColourGradient material(top,face.getX(),face.getY(),bottom,face.getX(),face.getBottom(),false);
    g.setGradientFill(material);g.fillRoundedRectangle(face,radius-2.f*s);
    roughness(g,face,radius-2.f*s,.24f);
    g.setColour(juce::Colour(0xffefe0cd).withAlpha(.54f));g.drawRoundedRectangle(face.reduced(.4f),radius-2.4f*s,.9f*s);
    g.setColour(juce::Colour(0xffffe8d0).withAlpha(.32f));g.drawLine(face.getX()+8*s,face.getY()+3*s,face.getRight()-8*s,face.getY()+3*s,s);
    if(graphiteCap){
        const auto light=juce::Rectangle<float>{r.getCentreX()-25.f*s,r.getY()+8.f*s,50.f*s,4.5f*s};
        const auto color=juce::Colour(off?0xfffc494f:0xff87e54f);
        g.setColour(juce::Colour(0xff131210));g.fillRoundedRectangle(light.expanded(s),2.5f*s);
        if(selected){g.setColour(color.withAlpha(.13f));g.fillRoundedRectangle(light.expanded(s,.6f*s),2.5f*s);}
        g.setColour(selected?color:color.darker(.83f));g.fillRoundedRectangle(light,2.f*s);
    }
    if(hover){g.setColour(juce::Colours::white.withAlpha(.035f));g.fillRoundedRectangle(face,radius-2.f*s);}
}
inline void darkButton(juce::Graphics& g,juce::Rectangle<float> r,bool selected,bool hover,bool lamp=false,juce::Colour lampColour=onGreen,bool navigation=false){
    // Recessed controls use their bevel; raised keys get a parent shadow.
    juce::ColourGradient face(juce::Colour(navigation?0xff554a3e:0xff52463e),r.getX(),r.getY(),juce::Colour(0xff171411),r.getX(),r.getBottom(),false);
    face.addColour(.25,navigation?juce::Colour(0xff463c33):faceplate);face.addColour(.66,juce::Colour(navigation?0xff29231d:0xff211d19));g.setGradientFill(face);g.fillRoundedRectangle(r,5);
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
inline void screw(juce::Graphics& g,juce::Point<float> c){
    constexpr float side=16.f;
    formidable::draw(g,formidable::Art::screw,{c.x-side*.5f,c.y-side*.5f,side,side});
}
inline void screws(juce::Graphics& g,juce::Rectangle<float> r,float xInset,float yInset){
    for(auto c:scalpel::reference::screwCentres(r,xInset,yInset))screw(g,c);
}
inline void analyzerHousing(juce::Graphics& g,juce::Rectangle<float> r){
    const auto mapped=[r](float x,float y,float w,float h){
        return juce::Rectangle<float>{r.getX()+x*r.getWidth()/2172.f,
            r.getY()+y*r.getHeight()/436.f,w*r.getWidth()/2172.f,h*r.getHeight()/436.f};
    };
    formidable::draw(g,formidable::Art::analyzer_faceplate_10842,r);
    formidable::draw(g,formidable::Art::analyzer_bezel_10842,r);
    const auto lens=mapped(189,24,1529,392);
    formidable::draw(g,formidable::Art::analyzer_smoked_10842,lens);
    formidable::draw(g,formidable::Art::analyzer_gasket_10842,lens);
    formidable::draw(g,formidable::Art::analyzer_pixels_10842,mapped(208,70,1408,290));
}
inline void analyzerReflection(juce::Graphics& g,juce::Rectangle<float> r){
    formidable::draw(g,formidable::Art::analyzer_reflection_10842,
        {r.getX()+189.f*r.getWidth()/2172.f,r.getY()+24.f*r.getHeight()/436.f,
         1529.f*r.getWidth()/2172.f,392.f*r.getHeight()/436.f});
}
// 154px diameter on the master canvas, uniformly scaled with the editor.
inline constexpr float knobDiameter=154.f*scalpel::reference::scale;
// JUCE rotary angles are measured clockwise from twelve o'clock. Keeping this
// conversion in one function prevents the former 90-degree tick/pointer drift.
inline juce::Point<float> dialPoint(float cx,float cy,float radius,float angle) noexcept {
    return {cx+std::sin(angle)*radius,cy-std::cos(angle)*radius};
}
inline void knob(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,const juce::Slider& slider){
    const float cx=x+w*.5f,cy=y+h*.5f,radius=juce::jmin(knobDiameter*.5f,.485f*(float)std::min(w,h));
    const float active=juce::jlimit(0.f,1.f,position);
    const bool bandGain=slider.getComponentID()=="band-gain-dial";
    const int segments=bandGain?49:37;
    const auto& atlas=formidable::image(formidable::Art::band_dial_rings_1087);
    const auto source=juce::Rectangle<int>{bandGain?0:304,0,304,304};
    const auto targetRing=juce::Rectangle<float>{cx-radius-4.f,cy-radius-4.f,2.f*(radius+4.f),2.f*(radius+4.f)};
    g.drawImage(atlas.getClippedImage(source),targetRing,juce::RectanglePlacement::stretchToFit);
    g.setColour(accent);
    for(int i=0;i<segments;++i){
        const float u=i/(float)(segments-1);
        if(u>active)break;
        const float angle=start+u*(end-start);
        const auto a=dialPoint(cx,cy,radius-6.f,angle);
        const auto b=dialPoint(cx,cy,radius+.25f,angle);
        g.drawLine({a,b},2.2f);
    }
    const float angle=start+active*(end-start);
    const auto& pointer=formidable::image(formidable::Art::dial_pointer_1086);
    const auto target=juce::Rectangle<float>{cx-2.1f,cy-radius+2.f,4.2f,radius*.42f};
    const auto map=juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit)
        .getTransformToFit(pointer.getBounds().toFloat(),target);
    g.setOpacity(1.f);
    g.drawImageTransformed(pointer,map.followedBy(juce::AffineTransform::rotation(angle,cx,cy)),false);
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

// Transparent seven-segment OLED readout. Numeric glyphs come from the
// formidable atlas; unit text is deliberately quiet and shares the glass.
// No background is painted here: the lower display remains one continuous
// black surface instead of a collection of inset khaki LCD cards.
inline void oledNumber(juce::Graphics& g,const juce::String& value,juce::Rectangle<float> r,float height=21.f){
    const auto& atlas=formidable::image(formidable::Art::oled_digits_1087);
    constexpr float sourceW=48.f,sourceH=72.f;
    const float glyphH=juce::jmin(height,r.getHeight()*.82f);
    const float glyphW=glyphH*sourceW/sourceH;
    const float gap=juce::jmax(1.f,glyphW*.10f);
    auto unitFont=font(glyphH*.52f,true);
    float width=0.f;
    const auto cell=[](juce_wchar c){
        if(c>='0'&&c<='9')return (int)(c-'0');
        if(c=='-')return 10;if(c=='.')return 11;if(c=='+')return 12;
        return -1;
    };
    for(auto c:value){
        const int index=cell(c);
        width+=index>=0?(index==11?glyphW*.42f:glyphW):unitFont.getStringWidthFloat(juce::String::charToString(c));
        width+=gap;
    }
    if(!value.isEmpty())width-=gap;
    if(width>r.getWidth()){
        const float scale=r.getWidth()/width;
        return oledNumber(g,value,r,height*scale);
    }
    float x=r.getCentreX()-width*.5f;
    const float y=r.getCentreY()-glyphH*.5f;
    g.setColour(ice);
    for(auto c:value){
        const int index=cell(c);
        if(index>=0){
            const float drawW=index==11?glyphW*.42f:glyphW;
            const auto source=juce::Rectangle<int>{index*48,0,48,72};
            const auto target=juce::Rectangle<float>{x,y,drawW,glyphH};
            g.drawImage(atlas.getClippedImage(source),target,juce::RectanglePlacement::stretchToFit);
            x+=drawW+gap;
        }else{
            const auto ch=juce::String::charToString(c);
            const float advance=unitFont.getStringWidthFloat(ch);
            g.setFont(unitFont);
            g.drawText(ch,juce::Rectangle<float>{x,r.getY(),advance,r.getHeight()},juce::Justification::centred,false);
            x+=advance+gap;
        }
    }
}
}



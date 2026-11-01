#pragma once
// SCALPEL EQ: matte champagne-ivory molded hardware, NO exterior tick marks.
// Vector components stay crisp at high DPI and every knob points to a real
// APVTS value; the signed indicator is inside the ivory dial face.
#include <JuceHeader.h>
#include <BinaryData.h>
#include "ScalpelAssets.h"
#include <cmath>
#include "OnyxRackSkin.h"
class SurgeryLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    SurgeryLookAndFeel(){
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff261e1a));
        setColour(juce::PopupMenu::textColourId,juce::Colour(0xfff1ddc3));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff7b5340));
        setColour(juce::PopupMenu::highlightedTextColourId,juce::Colour(0xffffe8c7));
        setColour(juce::ComboBox::textColourId,juce::Colour(0xfff9e2c5));
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override {return juce::Font(juce::FontOptions(13.0f));}
    juce::Font getPopupMenuFont() override {return juce::Font(juce::FontOptions(13.0f));}
    void positionComboBoxText(juce::ComboBox& box,juce::Label& label) override {
        label.setBounds(10,1,juce::jmax(0,box.getWidth()-36),box.getHeight()-2);
        label.setFont(getComboBoxFont(box));
        label.setJustificationType(juce::Justification::centredLeft);
    }
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(juce::ComboBox& box,juce::Label&) override {
        // Target component carries host DPI and our uniform editor transform.
        // No item-to-align: JUCE can place the complete list below, then fall
        // back above/clamp to its display work area when space is insufficient.
        return juce::PopupMenu::Options().withTargetComponent(&box)
            .withMinimumWidth(box.getWidth()).withMaximumNumColumns(1)
            .withStandardItemHeight(28).withInitiallySelectedItem(box.getSelectedId())
            .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards);
    }
    bool shouldPopupMenuScaleWithTargetComponent(const juce::PopupMenu::Options&) override {return true;}
    void drawPopupMenuItem(juce::Graphics& g,const juce::Rectangle<int>& area,bool separator,bool active,
                          bool highlighted,bool ticked,bool submenu,const juce::String& text,
                          const juce::String& shortcut,const juce::Drawable* icon,const juce::Colour* colour) override {
        juce::LookAndFeel_V4::drawPopupMenuItem(g,area,separator,active,highlighted||ticked,ticked,submenu,text,shortcut,icon,colour);
        if(ticked&&!separator){g.setColour(juce::Colour(0xffdd745a));g.fillRect(area.getX()+2,area.getY()+4,2,area.getHeight()-8);}
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,
                          float start,float end,juce::Slider& slider) override {
        const float cx=x+w*.5f,cy=y+h*.46f;
        const float radius=juce::jlimit(14.f,43.f,.45f*(float)std::min(w,h));
        const float angle=start+position*(end-start);
        const auto face=juce::Rectangle<float>(cx-radius,cy-radius,2*radius,2*radius);

        // Black broadcast-console dial with a real parameter arc. This is
        // vector-drawn so 100/125/150/200% host DPI all share one asset.
        g.setColour(juce::Colour(0xff3b2418).withAlpha(.36f));
        g.fillEllipse(face.translated(2.0f,4.0f).expanded(3.0f));
        juce::ColourGradient rim(juce::Colour(0xff8e6b52),cx-radius,cy-radius,
                                 juce::Colour(0xff160f0c),cx+radius,cy+radius,false);
        g.setGradientFill(rim);g.fillEllipse(face.expanded(2.0f));
        g.setColour(juce::Colour(0xff080706));g.fillEllipse(face);
        juce::ColourGradient dial(juce::Colour(0xff3a3734),cx-radius*.5f,cy-radius,
                                  juce::Colour(0xff151312),cx+radius*.6f,cy+radius,false);
        g.setGradientFill(dial);g.fillEllipse(face.reduced(5.0f));
        g.setColour(juce::Colour(0xff6c625a).withAlpha(.72f));
        g.drawEllipse(face.reduced(5.0f),1.0f);

        juce::Path backgroundArc,activeArc;
        backgroundArc.addCentredArc(cx,cy,radius+4.0f,radius+4.0f,0.0f,start,end,true);
        activeArc.addCentredArc(cx,cy,radius+4.0f,radius+4.0f,0.0f,start,angle,true);
        g.setColour(juce::Colour(0xff4b3c33));g.strokePath(backgroundArc,juce::PathStrokeType(3.0f));
        if(position>0.002f){
            g.setColour(juce::Colour(0xffff7900).withAlpha(.18f));
            g.strokePath(activeArc,juce::PathStrokeType(7.0f));
            g.setColour(juce::Colour(0xffff7900));
            g.strokePath(activeArc,juce::PathStrokeType(3.3f));
        }

        juce::Path pointer;
        pointer.addRoundedRectangle(cx-1.8f,cy-radius+10.0f,3.6f,radius*.48f,1.7f);
        pointer.applyTransform(juce::AffineTransform::rotation(angle,cx,cy));
        g.setColour(juce::Colour(0xfffff0dc));g.fillPath(pointer);
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,
                              const juce::Colour&,bool over,bool down) override {
        const auto r=button.getLocalBounds().toFloat().reduced(1.5f);
        const bool active=button.getToggleState()||down;
        const bool xray=button.getButtonText()=="X-RAY";
        const bool deckSwitch=button.getButtonText()=="ON"||button.getButtonText()=="OFF";
        const bool actionSwitch=button.getButtonText()=="MONO"||button.getButtonText()=="SOLO"||
            button.getButtonText().startsWith("PHASE")||button.getButtonText()=="LIMIT";
        const bool depressed=active&&(deckSwitch||actionSwitch);
        const bool snow=button.getButtonText()=="FREEZE"||button.getButtonText()=="RESUME";
        const bool heat=button.getButtonText().startsWith("SONAR ")||button.getButtonText()=="HEAT MAP";
        const auto text=button.getButtonText();
        const bool pageTab=text=="BAND MODE"||text=="CARVE"||text=="COLOR"||text=="ANALYZE";
        if(pageTab){
            const bool selected=button.getToggleState();
            g.setColour(juce::Colour(0xff493226).withAlpha(.90f));
            g.fillRoundedRectangle(r.translated(0,2.0f).expanded(1.0f),5.0f);
            juce::ColourGradient face(selected?juce::Colour(0xff24201d):juce::Colour(0xffffe2c3),
                                      0,r.getY(),selected?juce::Colour(0xff0d0b0a):juce::Colour(0xffc99d7e),
                                      0,r.getBottom(),false);
            g.setGradientFill(face);g.fillRoundedRectangle(r,5.0f);
            g.setColour(juce::Colour(selected?0xff8d6f59:0xffffeed8));
            g.drawRoundedRectangle(r.reduced(.8f),4.4f,1.0f);
            if(selected){
                g.setColour(juce::Colour(0xffff7900).withAlpha(.23f));
                g.fillRoundedRectangle({r.getX()+40,r.getBottom()-7,r.getWidth()-80,7},3.5f);
                g.setColour(juce::Colour(0xffff7900));
                g.fillRoundedRectangle({r.getX()+49,r.getBottom()-4,r.getWidth()-98,3},1.5f);
            }
            return;
        }
        if(snow||heat){
            static const auto snowImage=scalpel::loadSoftwareAsset(
                BinaryData::scalpelfreeze_png,BinaryData::scalpelfreeze_pngSize);
            static const auto heatImage=scalpel::loadSoftwareAsset(
                BinaryData::scalpelheatmap_png,BinaryData::scalpelheatmap_pngSize);
            const auto& sprite=snow?snowImage:heatImage;
            if(!sprite.isNull()){
                const float scale=.88f*std::min((float)button.getWidth(),(float)button.getHeight())/
                                  (.67f*(float)sprite.getWidth());
                const auto transform=juce::AffineTransform::scale(scale)
                    .translated(button.getWidth()*.5f-sprite.getWidth()*scale*.5f,
                                button.getHeight()*.5f-sprite.getHeight()*scale*.5f);
                g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
                g.drawImageTransformed(sprite,transform,false);
                if(active){g.setColour(juce::Colour(0xff7b5c48).withAlpha(.14f));
                    g.fillRoundedRectangle(r.reduced(6.f),5.f);}
                return;
            }
        }
        // Answering-machine ON/OFF switches really sink into a dark well when
        // selected. The red pilot illuminates ONLY that active switch; the
        // opposite switch stays the same raised powder-ivory as the mockup.
        g.setColour(juce::Colour(0xff33231b).withAlpha(.93f));
        g.fillRoundedRectangle(r.translated(0,2.f).expanded(1.2f),3.f);
        g.setColour(juce::Colour(0xff9b8066));
        g.drawRoundedRectangle(r.expanded(.5f),3.5f,1.2f);
        juce::ColourGradient plastic(
            xray?juce::Colour(active?0xff4b0908:0xff34100d):
                juce::Colour(depressed?0xff29201d:active?0xffd5bca0:0xffffecd3),
            r.getX(),r.getY()+1.f,
            xray?juce::Colour(active?0xff150404:0xff581613):
                juce::Colour(depressed?0xff100e0d:active?0xffb89e83:0xffd4bea5),
            r.getX(),r.getBottom(),false);
        plastic.addColour(.18,xray?juce::Colour(0xff681c15):
            depressed?juce::Colour(0xff3a302b):juce::Colour(0xfff2ddc1));
        g.setGradientFill(plastic);g.fillRoundedRectangle(r.reduced(1.f),2.5f);
        g.setColour(juce::Colour(xray?0xffbd4d3c:0xfffff3df).withAlpha(active?.34f:.91f));
        g.drawLine(r.getX()+4,r.getY()+2,r.getRight()-4,r.getY()+2,.9f);
        g.setColour(juce::Colour(0xff513c2f).withAlpha(.5f));
        g.drawRoundedRectangle(r.reduced(1.f),2.5f,.7f);
        if(xray){
            const float lamp=r.getHeight()*.14f;
            g.setColour(active?juce::Colour(0xffff4127):juce::Colour(0xff7a1e16));
            g.fillEllipse(r.getX()+8,r.getCentreY()-lamp,lamp*2,lamp*2);
            if(active){g.setColour(juce::Colour(0xffff4028).withAlpha(.26f));
                g.fillEllipse(r.getX()+5,r.getCentreY()-lamp-3,2*lamp+6,2*lamp+6);}
        }
        if(deckSwitch){
            const auto lamp=juce::Rectangle<float>(r.getCentreX()-12.f,r.getY()+5.f,24.f,4.f);
            g.setColour(active?juce::Colour(0xffed2d20):juce::Colour(0xff514a43));
            g.fillRoundedRectangle(lamp,1.4f);
            if(active){
                g.setColour(juce::Colour(0xffff4c35).withAlpha(.23f));
                g.fillRoundedRectangle(lamp.expanded(4.f,2.f),2.5f);
            }
        }
        if(actionSwitch&&active){
            // A tiny pale pilot identifies engaged processing without ever
            // painting the band red; red is reserved for true peak warnings.
            g.setColour(juce::Colour(0xffffeed8).withAlpha(.88f));
            g.fillEllipse(r.getRight()-10.f,r.getY()+6.f,3.5f,3.5f);
        }
        const auto warning=button.findColour(juce::TextButton::buttonColourId);
        if(!xray&&warning.getRed()>180&&warning.getGreen()<65&&warning.getBlue()<65){
            g.setColour(juce::Colour(0xffed1919).withAlpha(.28f+warning.getFloatAlpha()*.62f));
            g.fillRoundedRectangle(r.reduced(2.f),3.f);
        }
        if(over){g.setColour(juce::Colours::white.withAlpha(.08f));g.fillRoundedRectangle(r.reduced(2),3.f);}
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool) override {
        if(b.getButtonText()=="FREEZE"||b.getButtonText()=="RESUME"||b.getButtonText().startsWith("SONAR ")||b.getButtonText()=="HEAT MAP")return;
        const bool xray=b.getButtonText()=="X-RAY";
        const auto tabText=b.getButtonText();
        const bool pageTab=tabText=="BAND MODE"||tabText=="CARVE"||tabText=="COLOR"||tabText=="ANALYZE";
        if(pageTab){
            g.setColour(b.getToggleState()?juce::Colour(0xffffe8ce):juce::Colour(0xff17110d));
            g.setFont(juce::Font(juce::FontOptions(15.0f,juce::Font::bold|juce::Font::italic)));
            g.drawFittedText(tabText,b.getLocalBounds().reduced(7,3),juce::Justification::centred,1);
            return;
        }
        const bool depressedSwitch=b.getToggleState()&&
            (b.getButtonText()=="ON"||b.getButtonText()=="OFF"||
             b.getButtonText()=="MONO"||b.getButtonText()=="SOLO"||
             b.getButtonText().startsWith("PHASE")||b.getButtonText()=="LIMIT");
        g.setColour(b.isEnabled()?juce::Colour(xray?0xffff624b:
            depressedSwitch?0xffffeee0:0xff241a14):juce::Colour(0xff8d8177));
        g.setFont(juce::Font(juce::FontOptions(juce::jlimit(11.0f,13.0f,b.getHeight()*.43f),juce::Font::bold)));
        g.drawFittedText(b.getButtonText(),xray?b.getLocalBounds().withTrimmedLeft(15):b.getLocalBounds().reduced(5,2),
                         juce::Justification::centred,1);
    }
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox& box) override {
        auto r=juce::Rectangle<float>(1,1,(float)w-2,(float)h-2);
        g.setColour(juce::Colour(0xff38261e));g.fillRoundedRectangle(r.expanded(.8f),3.f);
        juce::ColourGradient inset(juce::Colour(0xff241b16),0,0,
                                  juce::Colour(0xff4b3930),0,(float)h,false);
        g.setGradientFill(inset);g.fillRoundedRectangle(r.reduced(1.f),2.f);
        g.setColour(juce::Colour(0xffbca084));g.drawRoundedRectangle(r,3.f,.9f);
        g.setColour(juce::Colour(0xffffe7cf));
        juce::Path arrow;arrow.startNewSubPath((float)w-20,h*.42f);arrow.lineTo((float)w-15,h*.6f);arrow.lineTo((float)w-10,h*.42f);
        g.strokePath(arrow,juce::PathStrokeType(1.6f));
    }
    void drawLabel(juce::Graphics& g,juce::Label& label) override {
        if(dynamic_cast<juce::Slider*>(label.getParentComponent())!=nullptr){
            const auto r=label.getLocalBounds().toFloat().reduced(.5f);
            g.setColour(juce::Colour(0xffe2d0b9));g.fillRoundedRectangle(r,2.f);
            g.setColour(juce::Colour(0xffa88f79));g.drawRoundedRectangle(r,2.f,.7f);
            if(!label.isBeingEdited()){
                g.setColour(juce::Colour(0xff32261f));g.setFont(juce::Font(juce::FontOptions(12.5f,juce::Font::bold)));
                g.drawFittedText(label.getText(),label.getLocalBounds().reduced(2),juce::Justification::centred,1);
            }
            return;
        }
        juce::LookAndFeel_V4::drawLabel(g,label);
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float min,float max,
                          juce::Slider::SliderStyle style,juce::Slider& slider) override {
        if(style!=juce::Slider::LinearHorizontal){juce::LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,pos,min,max,style,slider);return;}
        auto track=juce::Rectangle<float>((float)x,(float)y+height*.35f,(float)width,height*.3f);
        g.setColour(juce::Colour(0xff281e19));g.fillRoundedRectangle(track.expanded(2),3);
        g.setColour(juce::Colour(0xff4c3830));g.fillRoundedRectangle(track,2);
        auto fill=track.withWidth(juce::jlimit(0.f,(float)width,pos-x));
        juce::ColourGradient light(juce::Colour(0xffe4ba89),fill.getX(),fill.getY(),juce::Colour(0xff92644b),fill.getRight(),fill.getY(),false);
        g.setGradientFill(light);g.fillRoundedRectangle(fill,2);
        g.setColour(juce::Colour(0xffdec4a3));g.fillEllipse(pos-6,track.getCentreY()-6,12,12);
        g.setColour(juce::Colour(0xfffaf0dc));g.drawEllipse(pos-6,track.getCentreY()-6,12,12,.7f);
    }
};

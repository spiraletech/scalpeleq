#pragma once
// SCALPEL EQ: matte champagne-ivory molded hardware, NO exterior tick marks.
// Vector components stay crisp at high DPI and every knob points to a real
// APVTS value; the signed indicator is inside the ivory dial face.
#include <JuceHeader.h>
#include <BinaryData.h>
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
        const float cx=x+w*.5f,cy=y+h*.48f;
        const float radius=juce::jlimit(13.f,44.f,.48f*(float)std::min(w,h));
        // The sculpted V mark is at twelve o'clock at each control's HOME
        // value (0% COLOR, 0 dB gain, Q reset, etc.). It does not inherit
        // Surgery's lower-left minimum-angle pointer drift.
        const float neutral=(float)slider.getNormalisableRange().convertTo0to1(
            slider.getDoubleClickReturnValue());
        const float rotation=(position-neutral)*(end-start);
        // Exact user-approved powdery ivory knob artwork. Its molded V-index
        // rotates with the REAL APVTS parameter. No exterior calibration ticks.
        static const auto dial=juce::ImageCache::getFromMemory(
            BinaryData::scalpelknob_png,BinaryData::scalpelknob_pngSize);
        if(!dial.isNull()){
            const float scale=2*radius/(.64f*(float)dial.getWidth());
            const auto transform=juce::AffineTransform::scale(scale)
                .translated(cx-dial.getWidth()*scale*.5f,cy-dial.getHeight()*scale*.5f)
                .rotated(rotation,cx,cy);
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImageTransformed(dial,transform,false);
            return;
        }
        const auto face=juce::Rectangle<float>(cx-radius,cy-radius,2*radius,2*radius);
        g.setColour(juce::Colour(0xff54483d).withAlpha(.25f));
        g.fillEllipse(face.translated(1.4f,3.f).expanded(2.f));
        juce::ColourGradient rim(juce::Colour(0xfffcf1dc),cx-radius,cy-radius,
                                 juce::Colour(0xff887460),cx+radius,cy+radius,false);
        g.setGradientFill(rim);g.fillEllipse(face.expanded(1.f));
        g.setColour(juce::Colour(0xff493b31).withAlpha(.63f));g.drawEllipse(face.expanded(1.f),1.1f);
        juce::ColourGradient ivory(juce::Colour(0xfff6ead6),cx-radius*.4f,cy-radius,
                                   juce::Colour(0xffd3c0a9),cx+radius*.4f,cy+radius,false);
        g.setGradientFill(ivory);g.fillEllipse(face.reduced(3.f));
        g.setColour(juce::Colour(0xfffdf6e8).withAlpha(.58f));g.drawEllipse(face.reduced(4.2f),.8f);
        // Subtle powder-finish speckles stay inside the manufactured face.
        for(int i=0;i<64;++i){
            const float a=i*2.3999632f;
            const float rr=radius*.73f*std::sqrt((i+.5f)/64.f);
            g.setColour(juce::Colour(0xff7d6957).withAlpha(.065f));
            g.fillEllipse(cx+std::cos(a)*rr-.34f,cy+std::sin(a)*rr-.34f,.68f,.68f);
        }
        juce::Path pointer;
        pointer.addTriangle(cx-3.4f,cy-radius+9.f,cx+3.4f,cy-radius+9.f,cx,cy-radius+25.f);
        pointer.applyTransform(juce::AffineTransform::rotation(rotation,cx,cy));
        g.setColour(juce::Colour(0xff6b4e35));g.fillPath(pointer);
        g.setColour(juce::Colour(0xfffff0d8).withAlpha(.50f));g.strokePath(pointer,juce::PathStrokeType(.5f));
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,
                              const juce::Colour&,bool over,bool down) override {
        const auto r=button.getLocalBounds().toFloat().reduced(1.5f);
        const bool active=button.getToggleState()||down;
        const bool xray=button.getButtonText()=="X-RAY";
        const bool deckSwitch=button.getButtonText()=="ON"||button.getButtonText()=="OFF";
        const bool actionSwitch=button.getButtonText()=="MONO"||button.getButtonText()=="SOLO"||
            button.getButtonText()=="PHASE"||button.getButtonText()=="LIMIT";
        const bool depressed=active&&(deckSwitch||actionSwitch);
        const bool snow=button.getButtonText()=="FREEZE"||button.getButtonText()=="RESUME";
        const bool heat=button.getButtonText().startsWith("SONAR ")||button.getButtonText()=="HEAT MAP";
        if(snow||heat){
            static const auto snowImage=juce::ImageCache::getFromMemory(
                BinaryData::scalpelfreeze_png,BinaryData::scalpelfreeze_pngSize);
            static const auto heatImage=juce::ImageCache::getFromMemory(
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
        const bool depressedSwitch=b.getToggleState()&&
            (b.getButtonText()=="ON"||b.getButtonText()=="OFF"||
             b.getButtonText()=="MONO"||b.getButtonText()=="SOLO"||
             b.getButtonText()=="PHASE"||b.getButtonText()=="LIMIT");
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

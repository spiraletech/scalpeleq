#pragma once
// SCALPEL EQ: matte champagne-ivory molded hardware, NO exterior tick marks.
// Vector components stay crisp at high DPI and every knob points to a real
// APVTS value; the signed indicator is inside the ivory dial face.
#include <JuceHeader.h>
#include <cmath>
#include "OnyxRackSkin.h"
class SurgeryLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    SurgeryLookAndFeel(){
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xffe3d1bc));
        setColour(juce::PopupMenu::textColourId,juce::Colour(0xff30251e));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xffbca48b));
        setColour(juce::PopupMenu::highlightedTextColourId,juce::Colours::black);
        setColour(juce::ComboBox::textColourId,juce::Colour(0xff261e18));
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
        if(ticked&&!separator){g.setColour(juce::Colour(0xff675349));g.fillRect(area.getX()+2,area.getY()+4,2,area.getHeight()-8);}
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,
                          float start,float end,juce::Slider&) override {
        const float cx=x+w*.5f,cy=y+h*.48f;
        const float radius=juce::jlimit(13.f,36.f,.44f*(float)std::min(w,h));
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
        const float angle=start+position*(end-start);
        juce::Path pointer;
        pointer.addTriangle(cx-3.4f,cy-radius+9.f,cx+3.4f,cy-radius+9.f,cx,cy-radius+25.f);
        pointer.applyTransform(juce::AffineTransform::rotation(angle-juce::MathConstants<float>::twoPi,cx,cy));
        g.setColour(juce::Colour(0xff6b4e35));g.fillPath(pointer);
        g.setColour(juce::Colour(0xfffff0d8).withAlpha(.50f));g.strokePath(pointer,juce::PathStrokeType(.5f));
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,
                              const juce::Colour&,bool over,bool down) override {
        const auto r=button.getLocalBounds().toFloat().reduced(1.5f);
        const bool active=button.getToggleState()||down;
        const bool xray=button.getButtonText()=="X-RAY";
        g.setColour(juce::Colour(0xff766453).withAlpha(.56f));g.fillRoundedRectangle(r.translated(0,1.2f),3.f);
        juce::ColourGradient plastic(
            xray?juce::Colour(active?0xff8e1712:0xff421613):juce::Colour(active?0xffc7af92:0xfff5e6d0),r.getX(),r.getY(),
            xray?juce::Colour(0xff290b09):juce::Colour(active?0xffe6d2b8:0xffccbaa4),r.getX(),r.getBottom(),false);
        g.setGradientFill(plastic);g.fillRoundedRectangle(r.reduced(1.f),3.f);
        g.setColour(juce::Colour(xray?0xff170607:0xfffef4e3).withAlpha(.70f));
        g.drawRoundedRectangle(r.reduced(1.1f),3.f,.75f);
        const auto warning=button.findColour(juce::TextButton::buttonColourId);
        if(!xray&&warning.getRed()>180&&warning.getGreen()<65&&warning.getBlue()<65){
            g.setColour(juce::Colour(0xffed1919).withAlpha(.28f+warning.getFloatAlpha()*.62f));
            g.fillRoundedRectangle(r.reduced(2.f),3.f);
        }
        if(over){g.setColour(juce::Colours::white.withAlpha(.08f));g.fillRoundedRectangle(r.reduced(2),3.f);}
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool) override {
        const bool xray=b.getButtonText()=="X-RAY";
        g.setColour(b.isEnabled()?juce::Colour(xray?0xffff4c37:0xff231b16):juce::Colour(0xff8d8177));
        g.setFont(juce::Font(juce::FontOptions(juce::jlimit(11.0f,13.0f,b.getHeight()*.43f),juce::Font::bold)));
        g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(5,2),
                         juce::Justification::centred,1);
    }
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox& box) override {
        auto r=juce::Rectangle<float>(1,1,(float)w-2,(float)h-2);
        g.setColour(juce::Colour(0xffd1bca5));g.fillRoundedRectangle(r,3.f);
        g.setColour(juce::Colour(0xff746456));g.drawRoundedRectangle(r,3.f,.8f);
        g.setColour(juce::Colour(0xff40342b));
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
        g.setColour(juce::Colour(0xff010506));g.fillRoundedRectangle(track.expanded(2),3);
        g.setColour(juce::Colour(0xff19353d));g.fillRoundedRectangle(track,2);
        auto fill=track.withWidth(juce::jlimit(0.f,(float)width,pos-x));
        juce::ColourGradient light(juce::Colour(0xff00efff),fill.getX(),fill.getY(),juce::Colour(0xff008ea9),fill.getRight(),fill.getY(),false);
        g.setGradientFill(light);g.fillRoundedRectangle(fill,2);
        g.setColour(juce::Colour(0xff00d9ec));g.fillEllipse(pos-6,track.getCentreY()-6,12,12);
        g.setColour(juce::Colour(0xffb2f5ff));g.drawEllipse(pos-6,track.getCentreY()-6,12,12,.7f);
    }
};

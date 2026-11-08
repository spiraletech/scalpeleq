#pragma once
// SCALPEL EQ: blue-black carbon polymer hardware, NO exterior tick marks.
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
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff0b141d));
        setColour(juce::PopupMenu::textColourId,juce::Colour(0xffedf5ff));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff244866));
        setColour(juce::PopupMenu::highlightedTextColourId,juce::Colour(0xfff5fbff));
        setColour(juce::ComboBox::textColourId,juce::Colour(0xffedf5ff));
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override {return juce::Font(juce::FontOptions("Segoe UI",13.0f,juce::Font::plain));}
    juce::Font getPopupMenuFont() override {return juce::Font(juce::FontOptions("Segoe UI",13.0f,juce::Font::plain));}
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
        const float radius=juce::jlimit(14.f,42.f,.43f*(float)std::min(w,h));
        const float angle=start+position*(end-start);
        const auto face=juce::Rectangle<float>(cx-radius,cy-radius,2*radius,2*radius);

        // One machined bezel, one dial face and one precise indicator. Keeping
        // the geometry vector-native removes the stacked/rasterized "AI knob"
        // look and remains sharp at every host DPI.
        g.setColour(juce::Colour(0xff02070b).withAlpha(.55f));
        g.fillEllipse(face.translated(1.5f,3.0f).expanded(1.5f));
        juce::ColourGradient bezel(juce::Colour(0xff6d879a),cx-radius,cy-radius,
                                   juce::Colour(0xff0b1219),cx+radius,cy+radius,false);
        g.setGradientFill(bezel);g.fillEllipse(face.expanded(1.4f));
        g.setColour(juce::Colour(0xff0b0908));g.fillEllipse(face);
        juce::ColourGradient dial(juce::Colour(0xff2e4454),cx-radius*.45f,cy-radius,
                                  juce::Colour(0xff090f15),cx+radius*.55f,cy+radius,false);
        g.setGradientFill(dial);g.fillEllipse(face.reduced(4.5f));
        g.setColour(juce::Colour(0xff99b9cf).withAlpha(.58f));
        g.drawEllipse(face.reduced(4.5f),.85f);

        juce::Path track;
        track.addCentredArc(cx,cy,radius+3.0f,radius+3.0f,0.0f,start,end,true);
        g.setColour(juce::Colour(0xff5c7890).withAlpha(.64f));
        g.strokePath(track,juce::PathStrokeType(2.0f));

        const auto& range=slider.getNormalisableRange();
        const float home=juce::jlimit(0.0f,1.0f,(float)range.convertTo0to1(slider.getDoubleClickReturnValue()));
        const float homeAngle=start+home*(end-start);
        if(std::abs(position-home)>.002f){
            juce::Path activeArc;
            activeArc.addCentredArc(cx,cy,radius+3.0f,radius+3.0f,0.0f,
                                    std::min(homeAngle,angle),std::max(homeAngle,angle),true);
            g.setColour(juce::Colour(0xff8fdcff).withAlpha(.16f));
            g.strokePath(activeArc,juce::PathStrokeType(5.0f));
            g.setColour(juce::Colour(0xff8fdcff));
            g.strokePath(activeArc,juce::PathStrokeType(2.4f));
        }

        juce::Path pointer;
        pointer.addRoundedRectangle(cx-1.5f,cy-radius+10.0f,3.0f,radius*.43f,1.5f);
        pointer.applyTransform(juce::AffineTransform::rotation(angle,cx,cy));
        g.setColour(juce::Colour(0xffe7f5ff));g.fillPath(pointer);
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,
                              const juce::Colour&,bool over,bool down) override {
        const auto r=button.getLocalBounds().toFloat().reduced(1.5f);
        const bool active=button.getToggleState()||down;
        const bool xray=button.getButtonText()=="X-RAY";
        const bool reset=button.getButtonText()=="RESET";
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
            g.setColour(juce::Colour(0xff02070b).withAlpha(.96f));
            g.fillRoundedRectangle(r.translated(0,2.0f).expanded(1.0f),5.0f);
            juce::ColourGradient face(selected?juce::Colour(0xff1b2a36):juce::Colour(0xffbccbd8),
                                      0,r.getY(),selected?juce::Colour(0xff070c11):juce::Colour(0xff61778a),
                                      0,r.getBottom(),false);
            g.setGradientFill(face);g.fillRoundedRectangle(r,5.0f);
            g.setColour(juce::Colour(selected?0xff5a89a8:0xffdcebf6));
            g.drawRoundedRectangle(r.reduced(.8f),4.4f,1.0f);
            if(selected){
                g.setColour(juce::Colour(0xff8fdcff).withAlpha(.23f));
                g.fillRoundedRectangle({r.getX()+40,r.getBottom()-7,r.getWidth()-80,7},3.5f);
                g.setColour(juce::Colour(0xff8fdcff));
                g.fillRoundedRectangle({r.getX()+49,r.getBottom()-4,r.getWidth()-98,3},1.5f);
            }
            return;
        }
        if(reset){
            // RESET belongs to the analyzer's carbon instrument bay. Keep the
            // same copper/ivory machining language without a stray pale key.
            g.setColour(juce::Colour(0xff2c1c14).withAlpha(.58f));
            g.fillRoundedRectangle(r.translated(0,2.0f).expanded(1.0f),4.0f);
            juce::ColourGradient carbon(juce::Colour(0xff2a2724),r.getX(),r.getY(),
                                        juce::Colour(0xff070605),r.getRight(),r.getBottom(),false);
            carbon.addColour(.48,juce::Colour(0xff171411));
            g.setGradientFill(carbon);g.fillRoundedRectangle(r,3.5f);
            g.saveState();
            g.reduceClipRegion(r.reduced(1.0f).toNearestInt());
            g.setColour(juce::Colours::white.withAlpha(.025f));
            for(float x=r.getX()-r.getHeight();x<r.getRight();x+=7.0f)
                g.drawLine(x,r.getBottom(),x+r.getHeight(),r.getY(),.65f);
            g.restoreState();
            g.setColour(juce::Colour(0xffa47b5d));
            g.drawRoundedRectangle(r,3.5f,1.0f);
            g.setColour(juce::Colour(0xffffead4).withAlpha(.16f));
            g.drawLine(r.getX()+5.0f,r.getY()+2.0f,r.getRight()-5.0f,r.getY()+2.0f,.8f);
            if(over){
                g.setColour(juce::Colours::white.withAlpha(.055f));
                g.fillRoundedRectangle(r.reduced(1.5f),2.5f);
            }
            return;
        }
        if(snow||heat){
            // ANALYZE uses scalable console switches, not pale raster tiles.
            // The reference hierarchy is a dark recessed face, machined ivory
            // bezel and a restrained orange lamp only for an active HEAT MAP.
            const float side=std::min(r.getWidth(),r.getHeight())-14.0f;
            const auto tile=juce::Rectangle<float>(r.getCentreX()-side*.5f,
                                                   r.getCentreY()-side*.5f,
                                                   side,side);
            g.setColour(juce::Colour(0xff3d281c).withAlpha(.42f));
            g.fillRoundedRectangle(tile.translated(2.0f,4.0f).expanded(2.0f),8.0f);
            if(heat&&active){
                g.setColour(juce::Colour(0xffff7800).withAlpha(.16f));
                g.drawRoundedRectangle(tile.expanded(5.0f),10.0f,7.0f);
                g.setColour(juce::Colour(0xffff8500));
                g.drawRoundedRectangle(tile.expanded(2.0f),8.5f,2.3f);
            }
            juce::ColourGradient bezel(juce::Colour(0xffffe7cb),tile.getX(),tile.getY(),
                                       juce::Colour(0xff6d5140),tile.getRight(),tile.getBottom(),false);
            g.setGradientFill(bezel);g.fillRoundedRectangle(tile,7.0f);
            const auto face=tile.reduced(5.0f);
            juce::ColourGradient darkFace(juce::Colour(0xff35302c),face.getX(),face.getY(),
                                          juce::Colour(0xff0b0908),face.getRight(),face.getBottom(),false);
            g.setGradientFill(darkFace);g.fillRoundedRectangle(face,5.0f);
            g.setColour(juce::Colour(0xfffff0dc).withAlpha(.34f));
            g.drawRoundedRectangle(face.reduced(.7f),4.5f,1.0f);
            g.setColour(juce::Colour(0xff080605));
            g.drawRoundedRectangle(tile,7.0f,1.2f);

            const auto centre=face.getCentre();
            if(heat){
                // Two-tongue outline flame: broad at the base with an offset
                // upper lick, matching the approved embossed hardware symbol.
                juce::Path flame;
                flame.startNewSubPath(centre.x-2.0f,face.getBottom()-9.0f);
                flame.cubicTo(centre.x-16.0f,face.getBottom()-15.0f,
                              centre.x-17.0f,centre.y-8.0f,
                              centre.x-7.0f,face.getY()+15.0f);
                flame.cubicTo(centre.x-8.0f,centre.y-3.0f,
                              centre.x-2.0f,centre.y-7.0f,
                              centre.x+3.0f,centre.y-12.0f);
                flame.cubicTo(centre.x+2.0f,centre.y-20.0f,
                              centre.x+7.0f,face.getY()+10.0f,
                              centre.x+10.0f,face.getY()+7.0f);
                flame.cubicTo(centre.x+12.0f,centre.y-10.0f,
                              centre.x+19.0f,centre.y-3.0f,
                              centre.x+15.0f,centre.y+9.0f);
                flame.cubicTo(centre.x+12.0f,face.getBottom()-12.0f,
                              centre.x+5.0f,face.getBottom()-8.0f,
                              centre.x-2.0f,face.getBottom()-9.0f);
                juce::Path inner;
                inner.startNewSubPath(centre.x-1.0f,face.getBottom()-12.0f);
                inner.cubicTo(centre.x-7.0f,centre.y+8.0f,
                              centre.x-4.0f,centre.y+1.0f,
                              centre.x+1.0f,centre.y-5.0f);
                inner.cubicTo(centre.x+1.0f,centre.y+1.0f,
                              centre.x+8.0f,centre.y+4.0f,
                              centre.x+6.0f,centre.y+10.0f);
                inner.cubicTo(centre.x+5.0f,face.getBottom()-13.0f,
                              centre.x+2.0f,face.getBottom()-11.0f,
                              centre.x-1.0f,face.getBottom()-12.0f);
                g.setColour(juce::Colour(heat&&active?0xffff9200:0xffc87932));
                g.strokePath(flame,juce::PathStrokeType(3.2f,juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
                g.strokePath(inner,juce::PathStrokeType(2.4f,juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
            }else{
                g.setColour(active?juce::Colour(0xffbfe8ff):juce::Colour(0xffdcebf6));
                const float length=face.getWidth()*.36f;
                // Three uninterrupted axes guarantee a readable crystalline
                // centre. Six outward forks stay connected at native scale.
                for(int axis=0;axis<3;++axis){
                    const float angle=juce::MathConstants<float>::pi*axis/3.0f;
                    const juce::Point<float> unit{std::cos(angle),std::sin(angle)};
                    g.drawLine({centre-unit*length,centre+unit*length},2.7f);
                }
                for(int arm=0;arm<6;++arm){
                    const float angle=juce::MathConstants<float>::twoPi*arm/6.0f;
                    const juce::Point<float> unit{std::cos(angle),std::sin(angle)};
                    const juce::Point<float> normal{-unit.y,unit.x};
                    const auto root=centre+unit*(length*.50f);
                    const auto forward=unit*(length*.25f);
                    const auto side=normal*(length*.14f);
                    g.drawLine({root,root+forward+side},1.9f);
                    g.drawLine({root,root+forward-side},1.9f);
                }
                g.fillEllipse(centre.x-1.7f,centre.y-1.7f,3.4f,3.4f);
                if(active){
                    g.setColour(juce::Colour(0xff8fdcff).withAlpha(.32f));
                    g.drawRoundedRectangle(tile.expanded(2.0f),8.5f,3.0f);
                }
            }
            return;
        }
        // Answering-machine ON/OFF switches really sink into a dark well when
        // selected. The red pilot illuminates ONLY that active switch; the
        // opposite switch stays the same raised powder-ivory as the mockup.
        g.setColour(juce::Colour(0xff02070b).withAlpha(.93f));
        g.fillRoundedRectangle(r.translated(0,2.f).expanded(1.2f),3.f);
        g.setColour(juce::Colour(0xff5c7890));
        g.drawRoundedRectangle(r.expanded(.5f),3.5f,1.2f);
        juce::ColourGradient plastic(
            xray?juce::Colour(active?0xff4b0908:0xff18232d):
                juce::Colour(depressed?0xff14222d:active?0xffcbd9e5:0xffdce8f2),
            r.getX(),r.getY()+1.f,
            xray?juce::Colour(active?0xff150404:0xff581613):
                juce::Colour(depressed?0xff100e0d:active?0xffb89e83:0xffd4bea5),
            r.getX(),r.getBottom(),false);
        plastic.addColour(.18,xray?juce::Colour(0xff681c15):
            depressed?juce::Colour(0xff1d3342):juce::Colour(0xffedf7ff));
        g.setGradientFill(plastic);g.fillRoundedRectangle(r.reduced(1.f),2.5f);
        g.setColour(juce::Colour(xray?0xffbd4d3c:0xffe6f5ff).withAlpha(active?.34f:.91f));
        g.drawLine(r.getX()+4,r.getY()+2,r.getRight()-4,r.getY()+2,.9f);
        g.setColour(juce::Colour(0xff45657c).withAlpha(.5f));
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
            g.setColour(active?juce::Colour(0xff8fdcff):juce::Colour(0xff45515a));
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
        const bool reset=b.getButtonText()=="RESET";
        const auto tabText=b.getButtonText();
        const bool pageTab=tabText=="BAND MODE"||tabText=="CARVE"||tabText=="COLOR"||tabText=="ANALYZE";
        if(pageTab){
            g.setColour(b.getToggleState()?juce::Colour(0xffedf7ff):juce::Colour(0xff9eb4c5));
            g.setFont(juce::Font(juce::FontOptions("Segoe UI",15.0f,juce::Font::bold|juce::Font::italic)));
            g.drawFittedText(tabText,b.getLocalBounds().reduced(7,3),juce::Justification::centred,1);
            return;
        }
        const bool depressedSwitch=b.getToggleState()&&
            (b.getButtonText()=="ON"||b.getButtonText()=="OFF"||
             b.getButtonText()=="MONO"||b.getButtonText()=="SOLO"||
             b.getButtonText().startsWith("PHASE")||b.getButtonText()=="LIMIT");
        g.setColour(b.isEnabled()?juce::Colour(xray?0xffff624b:
            reset?0xffe4f2ff:depressedSwitch?0xffedf7ff:0xffd9e7f2):juce::Colour(0xff788b99));
        g.setFont(juce::Font(juce::FontOptions("Segoe UI",juce::jlimit(11.0f,13.0f,b.getHeight()*.43f),juce::Font::bold)));
        g.drawFittedText(b.getButtonText(),xray?b.getLocalBounds().withTrimmedLeft(15):b.getLocalBounds().reduced(5,2),
                         juce::Justification::centred,1);
    }
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox& box) override {
        auto r=juce::Rectangle<float>(1,1,(float)w-2,(float)h-2);
        g.setColour(juce::Colour(0xff02070b));g.fillRoundedRectangle(r.expanded(.8f),3.f);
        juce::ColourGradient inset(juce::Colour(0xff0c1822),0,0,
                                  juce::Colour(0xff1c2d3b),0,(float)h,false);
        g.setGradientFill(inset);g.fillRoundedRectangle(r.reduced(1.f),2.f);
        g.setColour(juce::Colour(0xff5f829a));g.drawRoundedRectangle(r,3.f,.9f);
        g.setColour(juce::Colour(0xffe8f6ff));
        juce::Path arrow;arrow.startNewSubPath((float)w-20,h*.42f);arrow.lineTo((float)w-15,h*.6f);arrow.lineTo((float)w-10,h*.42f);
        g.strokePath(arrow,juce::PathStrokeType(1.6f));
    }
    void drawLabel(juce::Graphics& g,juce::Label& label) override {
        if(auto* slider=dynamic_cast<juce::Slider*>(label.getParentComponent())){
            const auto r=label.getLocalBounds().toFloat().reduced(.5f);
            g.setColour(slider->findColour(juce::Slider::textBoxBackgroundColourId));
            g.fillRoundedRectangle(r,3.f);
            g.setColour(slider->findColour(juce::Slider::textBoxOutlineColourId));
            g.drawRoundedRectangle(r,3.f,.8f);
            if(!label.isBeingEdited()){
                g.setColour(slider->findColour(juce::Slider::textBoxTextColourId));
                g.setFont(juce::Font(juce::FontOptions("Segoe UI",13.0f,juce::Font::bold)));
                g.drawFittedText(label.getText(),label.getLocalBounds().reduced(3,1),
                                 juce::Justification::centred,1);
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


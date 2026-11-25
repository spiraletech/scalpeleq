#pragma once
#include "../Source/RootsEditor.h"
#include <fstream>
#include <iostream>

inline bool verifyScalpelDimensions(){
    using namespace scalpel::omnis;
    const auto fail=[](int line){std::cerr<<"ET017 dimensional regression at "<<line<<'\n';return false;};
    const auto pump=[](int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);};
    const auto save=[](const juce::Image& im,const juce::String& name){
        juce::FileOutputStream f(juce::File::getCurrentWorkingDirectory().getChildFile(name));f.setPosition(0);f.truncate();
        return juce::PNGImageFormat{}.writeImageToStream(im,f);
    };
    PRISMVSTAudioProcessor p;p.prepareToPlay(48000,512);
    std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
    if(e->getWidth()!=defaultWidth||e->getHeight()!=defaultHeight)return fail(__LINE__);
    const auto* constraint=e->getConstrainer();
    if(!constraint||std::abs(constraint->getFixedAspectRatio()-editorAspect)>1.e-9||constraint->getMinimumHeight()!=524||constraint->getMaximumHeight()!=1048)return fail(__LINE__);
    e->addToDesktop(0);e->setVisible(true);pump(30);
    auto button=[&](const juce::String& text)->juce::Button*{for(auto* c:e->getChildren())if(auto* b=dynamic_cast<juce::Button*>(c);b&&b->getButtonText()==text&&b->isVisible())return b;return nullptr;};
    auto click=[&](const juce::String& text){auto* b=button(text);if(!b||!b->isEnabled())return false;
        b->triggerClick();pump(5);return true;};
    std::ofstream measurements("surgery-et017-measured-bounds.csv");
    measurements<<"mode,control,logical_x,logical_y,logical_w,logical_h,rendered_x,rendered_y,rendered_w,rendered_h,kind\n";
    auto measure=[&](const juce::String& mode,juce::Component& c,juce::String name,juce::Rectangle<float> local,juce::String kind){
        const auto physical=local.translated((float)c.getX(),(float)c.getY()).transformedBy(c.getTransform());
        const auto logical=physical.transformedBy(Layout(e->getWidth(),e->getHeight()).transform().inverted());
        measurements<<mode<<','<<name<<','<<logical.getX()<<','<<logical.getY()<<','<<logical.getWidth()<<','<<logical.getHeight()<<','<<physical.getX()<<','<<physical.getY()<<','<<physical.getWidth()<<','<<physical.getHeight()<<','<<kind<<'\n';
        return logical;
    };
    juce::Image referenceRing;int totalDials=0,cell=0;
    const std::array<const char*,6> dialNames{{"BAND GAIN","COMP THRESHOLD","FREQUENCY","CARVE GAIN","Q WIDTH","VISUAL DECAY"}};
    juce::Image actualControls(juce::Image::ARGB,600,400,true,juce::SoftwareImageType{});
    juce::Image comparison(juce::Image::ARGB,3*200,2*190,true,juce::SoftwareImageType{});
    for(const auto* mode:{"BAND MODE","CARVE","ANALYZE"}){
        if(!click(mode))return fail(__LINE__);
        if(e->getWidth()!=1344||e->getHeight()!=786)return fail(__LINE__);
        int dials=0;
        for(auto* c:e->getChildren())if(c->isVisible()){
            if(auto* s=dynamic_cast<juce::Slider*>(c)){
                ++dials;++totalDials;
                if(s->getWidth()!=168||s->getHeight()!=154||std::abs(s->getY()+controls::localRotaryY-controls::rotaryCentreY)>.001f)return fail(__LINE__);
                const auto layout=s->getLookAndFeel().getSliderLayout(*s);
                if(std::abs(s->getY()+layout.textBoxBounds.getY()+controls::numericOffset-controls::numericBaseline)>.001f)return fail(__LINE__);
                auto body=measure(mode,*s,juce::String(dialNames[(size_t)cell]),{84-RotaryGeometry::outer,controls::localRotaryY-RotaryGeometry::outer,2*RotaryGeometry::outer,2*RotaryGeometry::outer},"ring-reference");
                if(std::abs(body.getWidth()-123.9f)>.002f||std::abs(body.getHeight()-123.9f)>.002f)return fail(__LINE__);
                const bool wasEnabled=s->isEnabled();s->setEnabled(true);
                juce::Image ring(juce::Image::ARGB,168,130,true,juce::SoftwareImageType{});
                {juce::Graphics g(ring);s->getLookAndFeel().drawRotarySlider(g,0,0,168,130,.5f,0,0,*s);}
                s->setEnabled(wasEnabled);
                if(!referenceRing.isValid())referenceRing=ring.createCopy();
                for(int y=0;y<130;++y)for(int x=0;x<168;++x)if(ring.getPixelAt(x,y)!=referenceRing.getPixelAt(x,y))return fail(__LINE__);
                {juce::Graphics g(comparison);g.setColour(juce::Colour(0xff090a0b));g.fillRect((cell%3)*200,(cell/3)*190,200,190);
                    g.drawImageAt(ring,(cell%3)*200+16,(cell/3)*190+30);
                    text(g,juce::String(dialNames[(size_t)cell]),{(float)(cell%3)*200,(float)(cell/3)*190,200,25},11,.91f);}
                {juce::Graphics g(actualControls);g.setColour(juce::Colour(0xff090a0b));g.fillRect((cell%3)*200,(cell/3)*200,200,200);
                    g.drawImageAt(s->createComponentSnapshot(s->getLocalBounds()),(cell%3)*200+16,(cell/3)*200+30);
                    text(g,juce::String(dialNames[(size_t)cell]),{(float)(cell%3)*200,(float)(cell/3)*200,200,25},11,.91f);}
                ++cell;
            }
            if(auto* b=dynamic_cast<juce::Button*>(c);b&&b->getY()>=538){
                const auto t=b->getButtonText();const bool power=t=="ON"||t=="OFF"||t=="HEAT MAP"||t=="FREEZE"||t=="RESUME";
                const auto r=measure(mode,*b,t,b->getLocalBounds().toFloat(),"button-hit/body");
                if(std::abs(r.getWidth()-(power?143.f:154.f))>.002f||std::abs(r.getHeight()-(power?46.f:47.f))>.002f)return fail(__LINE__);
                if(power){
                    const bool bottom=t=="OFF"||t=="FREEZE"||t=="RESUME";
                    if(std::abs(r.getCentreX()-135.f)>.002f||std::abs(r.getCentreY()-(bottom?676.142857f:610.142857f))>.002f)return fail(__LINE__);
                }else{
                    const bool right=t=="SOLO"||t=="LIMIT"||t=="C2"||t=="C4";
                    const bool bottom=t.startsWith("PHASE")||t=="LIMIT"||t=="C3"||t=="C4";
                    if(std::abs(r.getCentreX()-(right?1166.f:991.f))>.002f||std::abs(r.getCentreY()-(bottom?665.142857f:598.142857f))>.002f)return fail(__LINE__);
                }
                const auto actual=b->getBounds().toFloat().transformedBy(b->getTransform()).getCentre();
                if(e->getComponentAt(actual.toInt())!=b)return fail(__LINE__);
            }
            if(auto* b=dynamic_cast<juce::ComboBox*>(c)){
                auto body=b->getLocalBounds().toFloat().withSizeKeepingCentre(controls::carouselWidth,controls::carouselHeight);
                const auto r=measure(mode,*b,b->getY()<600?"COMPRESSION":"RATIO",body,"carousel-body");
                if(std::abs(r.getWidth()*referenceScale-242)>.002f||std::abs(r.getHeight()*referenceScale-48)>.002f)return fail(__LINE__);
            }
            if(auto* label=dynamic_cast<juce::Label*>(c);label&&(label->getText()=="HEAT MAP"||label->getText()=="FREEZE"))return fail(__LINE__);
            if(auto* label=dynamic_cast<juce::Label*>(c);label&&label->getProperties().contains("aura-baseline")){
                const float baseline=label->getY()+(float)label->getProperties()["aura-baseline"];
                if(label->getText()!="COMPRESSION"&&label->getText()!="RATIO"&&std::abs(baseline-controls::labelBaseline)>.001f)return fail(__LINE__);
            }
        }
        if(dials!=(juce::String(mode)=="BAND MODE"?2:juce::String(mode)=="CARVE"?3:1))return fail(__LINE__);
        if(!save(e->createComponentSnapshot(e->getLocalBounds()),"omnis-et017-"+juce::String(mode).replaceCharacter(' ','-')+"-1344x786.png"))return fail(__LINE__);
    }
    if(totalDials!=6||!save(comparison,"omnis-et017-six-rotaries.png")||!save(actualControls,"omnis-et017-six-controls-native.png"))return fail(__LINE__);
    // Freeze and heat remain independent; mode changes/presentation do not turn one into the other.
    auto* heat=button("HEAT MAP");auto* freeze=button("FREEZE");if(!heat||!freeze)return fail(__LINE__);
    const bool heatBefore=heat->getToggleState();if(!click("FREEZE")||!freeze->getToggleState()||heat->getToggleState()!=heatBefore)return fail(__LINE__);
    if(!click("HEAT MAP")||!freeze->getToggleState()||heat->getToggleState()==heatBefore)return fail(__LINE__);
    if(!click("RESUME")||freeze->getToggleState()||heat->getToggleState()==heatBefore)return fail(__LINE__);
    click("HEAT MAP");
    // The full expanded area is shielded during motion, and navigation remains above it.
    auto* reduced=p.apvts.getParameter("sonar_reduced_motion");reduced->setValueNotifyingHost(0);
    if(!click("CARVE"))return fail(__LINE__);
    juce::Component* shield=nullptr;for(auto* c:e->getChildren())if(c->getComponentID()=="aura-transition-shield")shield=c;
    if(!shield||!shield->isVisible()||shield->getHeight()<194)return fail(__LINE__);
    if(!save(e->createComponentSnapshot(e->getLocalBounds()),"omnis-et017-expanded-transition.png"))return fail(__LINE__);
    for(int n=0;n<12;++n){
        const int w=n%2?1792:896,h=n%2?1048:524;e->setSize(w,h);
        if(!click(n%2?"ANALYZE":"CARVE"))return fail(__LINE__);
        const auto point=content.getBottomRight().translated(-4,-4).transformedBy(Layout(w,h).transform());
        if(e->getComponentAt(point.toInt())!=shield)return fail(__LINE__);
    }
    pump(700);if(shield->isVisible()||!button("ANALYZE")->getToggleState())return fail(__LINE__);
    reduced->setValueNotifyingHost(1);e->setSize(defaultWidth,defaultHeight);click("BAND MODE");
    if(shield->isVisible())return fail(__LINE__);
    std::cout<<"PASS ET017:1344x786,224:131 bounds,all3 modes/6 pixel-identical rotary rings,integrated independent ANALYZE toggles,measured buttons/carousels,expanded shield and interrupted resize transitions\n";
    return true;
}

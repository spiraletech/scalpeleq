#pragma once
#include <JuceHeader.h>
#include <cmath>
#include "SurgeryChassisLayout.h"
namespace surgery::rack {
inline juce::AffineTransform contentTransform(int width,int height){
    const auto area=instrumentArea(width,height);
    const float s=std::min(area.getWidth()/1120.f,area.getHeight()/755.f);
    return juce::AffineTransform::scale(s).translated(area.getCentreX()-560*s,area.getCentreY()-377.5f*s);
}
inline juce::Path cut(juce::Rectangle<float> r,float c=4){
    juce::Path p;
    p.startNewSubPath(r.getX()+c,r.getY());p.lineTo(r.getRight()-c,r.getY());
    p.lineTo(r.getRight(),r.getY()+c);p.lineTo(r.getRight(),r.getBottom()-c);
    p.lineTo(r.getRight()-c,r.getBottom());p.lineTo(r.getX()+c,r.getBottom());
    p.lineTo(r.getX(),r.getBottom()-c);p.lineTo(r.getX(),r.getY()+c);p.closeSubPath();return p;
}
inline void bezel(juce::Graphics& g,juce::Rectangle<float> r,bool lit=false,float corner=4){
    auto p=cut(r,corner);
    g.setColour(juce::Colour(0xff000204));g.fillPath(p);
    if(lit){
        g.setColour(juce::Colour(0xff00e6ff).withAlpha(.10f));g.strokePath(p,juce::PathStrokeType(7));
        g.setColour(juce::Colour(0xff00e6ff).withAlpha(.23f));g.strokePath(p,juce::PathStrokeType(3.5f));
    }
    juce::ColourGradient rim(juce::Colour(0xff899496),r.getX(),r.getY(),juce::Colour(0xff242f33),r.getX(),r.getBottom(),false);
    rim.addColour(.08,juce::Colour(0xff4c6067));rim.addColour(.5,juce::Colour(0xff131d22));rim.addColour(.94,juce::Colour(0xff536166));
    g.setGradientFill(rim);g.strokePath(p,juce::PathStrokeType(1.4f));
    auto inner=cut(r.reduced(2),std::max(1.f,corner-1));
    juce::ColourGradient face(juce::Colour(lit?0xff14434e:0xff263139),r.getX(),r.getY()+2,juce::Colour(0xff02090d),r.getX(),r.getBottom()-2,false);
    face.addColour(.48,juce::Colour(0xff061219));g.setGradientFill(face);g.fillPath(inner);
    g.setColour(juce::Colour(lit?0xff00e6ff:0xff48575c));g.strokePath(inner,juce::PathStrokeType(lit?1.2f:.5f));
    g.setColour(juce::Colours::white.withAlpha(lit?.24f:.06f));g.drawLine(r.getX()+6,r.getY()+3,r.getRight()-6,r.getY()+3,.7f);
}
inline void screw(juce::Graphics& g,float x,float y,float radius=3.5f){
    g.setColour(juce::Colours::black);g.fillEllipse(x-radius-1,y-radius+1,2*radius+2,2*radius+2);
    juce::ColourGradient metal(juce::Colour(0xffdce4e3),x-radius,y-radius,juce::Colour(0xff273036),x+radius,y+radius,false);
    metal.addColour(.5,juce::Colour(0xff747d80));g.setGradientFill(metal);g.fillEllipse(x-radius,y-radius,2*radius,2*radius);
    g.setColour(juce::Colour(0xff030506));g.drawLine(x-radius*.55f,y-radius*.18f,x+radius*.55f,y+radius*.18f,1.4f);
    g.drawLine(x+radius*.18f,y-radius*.55f,x-radius*.18f,y+radius*.55f,1.4f);
}
inline void panel(juce::Graphics& g,juce::Rectangle<float> r){
    bezel(g,r,false,8);
    g.setColour(juce::Colour(0xff070c0f));g.fillPath(cut(r.reduced(4),5));
    // Deterministic fine brushed-steel grain; cached artwork supplies outer chassis.
    juce::Random grain(83017);
    for(int i=0;i<500;++i){
        float x=r.getX()+5+grain.nextFloat()*(r.getWidth()-10),y=r.getY()+5+grain.nextFloat()*(r.getHeight()-10);
        g.setColour(juce::Colours::white.withAlpha(.009f+grain.nextFloat()*.018f));
        g.drawLine(x,y,std::min(r.getRight()-5,x+2+grain.nextFloat()*9),y,.5f);
    }
    for(float x:{r.getX()+8,r.getRight()-8})for(float y:{r.getY()+8,r.getBottom()-8})screw(g,x,y,2.8f);
}
inline void knob(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end){
    const float cx=x+w*.5f,cy=y+h*.5f,r=juce::jmin((float)w,(float)h)*.39f;
    const float theta=start+position*(end-start);
    for(int i=0;i<=40;++i){
        const float a=start+(end-start)*i/40.f;
        g.setColour(juce::Colour(i%5==0?0xffc5d1d6:0xff6e888f));
        g.drawLine(cx+std::sin(a)*r*1.17f,cy-std::cos(a)*r*1.17f,cx+std::sin(a)*r*(i%5==0?1.28f:1.22f),cy-std::cos(a)*r*(i%5==0?1.28f:1.22f),i%5==0?1.0f:.6f);
    }
    g.setColour(juce::Colours::black.withAlpha(.8f));g.fillEllipse(cx-r-3,cy-r+2,2*r+6,2*r+6);
    juce::ColourGradient rim(juce::Colour(0xffeff4f4),cx-r,cy-r,juce::Colour(0xff11171c),cx+r,cy+r,false);
    rim.addColour(.28,juce::Colour(0xff7c8b91));rim.addColour(.46,juce::Colour(0xfff0f3f1));rim.addColour(.58,juce::Colour(0xff303b40));rim.addColour(.83,juce::Colour(0xffa1afb4));
    g.setGradientFill(rim);g.fillEllipse(cx-r,cy-r,2*r,2*r);
    g.setColour(juce::Colour(0xff020609));g.fillEllipse(cx-r*.87f,cy-r*.87f,r*1.74f,r*1.74f);
    for(int i=0;i<64;++i){
        const float a=i*juce::MathConstants<float>::twoPi/64, b=(i+1)*juce::MathConstants<float>::twoPi/64+.025f;
        const float reflect=.5f+.34f*std::cos(2*a-.5f)+.12f*std::cos(5*a+1);
        juce::Path sector;sector.startNewSubPath(cx,cy);
        sector.lineTo(cx+std::sin(a)*r*.76f,cy-std::cos(a)*r*.76f);
        sector.lineTo(cx+std::sin(b)*r*.76f,cy-std::cos(b)*r*.76f);sector.closeSubPath();
        g.setColour(juce::Colour::fromFloatRGBA(reflect,reflect+.015f,reflect+.02f,1));g.fillPath(sector);
    }
    g.setColour(juce::Colour(0xff04090d));g.drawEllipse(cx-r*.76f,cy-r*.76f,1.52f*r,1.52f*r,1.1f);
    g.setColour(juce::Colours::white.withAlpha(.24f));g.drawEllipse(cx-r*.60f,cy-r*.60f,1.2f*r,1.2f*r,.6f);
    g.setColour(juce::Colour(0xff00eaff).withAlpha(.18f));g.drawLine(cx+std::sin(theta)*r*.25f,cy-std::cos(theta)*r*.25f,cx+std::sin(theta)*r*.98f,cy-std::cos(theta)*r*.98f,5);
    g.setColour(juce::Colour(0xff00eaff));g.drawLine(cx+std::sin(theta)*r*.25f,cy-std::cos(theta)*r*.25f,cx+std::sin(theta)*r*.98f,cy-std::cos(theta)*r*.98f,2.5f);
}
}

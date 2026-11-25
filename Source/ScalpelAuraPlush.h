#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
#include "ScalpelAuraMotion.h"
#include <array>

namespace scalpel::aura {
inline constexpr std::array<const char*,5> stateNames{"disabled","pressed","selected","hover","idle"};
// Constructed by RootSelectorButton on the message thread, before its first
// paint. Images/crops are immutable software images, never audio-thread work.
class Images final {
public:
    static const Images& get(){static const Images images;return images;}
    const juce::Image& endpoint(int band,State s) const {return endpoints_[(size_t)band][(size_t)s];}
    const juce::Image& surface(State s) const {return surfaces_[(size_t)s];}
    const juce::Image& lettering(int band,State s) const {return lettering_[(size_t)band][(size_t)s];}
    const juce::Image& indicator(State s) const {return indicators_[(size_t)s];}
    const juce::Image& housing() const {return housing_;}
    bool valid() const noexcept {
        const auto registered=[](const juce::Image& i){return i.isValid()&&i.getWidth()==572&&i.getHeight()==200;};
        if(!registered(housing_))return false;
        for(size_t s=0;s<5;++s){
            if(!registered(surfaces_[s])||!registered(indicators_[s]))return false;
            for(size_t b=0;b<7;++b)if(!registered(lettering_[b][s])||!registered(endpoints_[b][s]))return false;
        }
        return true;
    }
private:
    static juce::Image load(const juce::String& suffix){
        int size=0;
        const auto* data=BinaryData::getNamedResource(("aura_c01_r02_"+suffix+"_png").toRawUTF8(),size);
        jassert(data!=nullptr&&size>0);
        auto image=juce::SoftwareImageType{}.convert(juce::ImageFileFormat::loadFrom(data,(size_t)size));
        jassert(image.isValid());return image;
    }
    Images(){
        housing_=load("stationary_housing");
        const auto atlas=load("five_state_atlas");
        jassert(atlas.getWidth()==4004&&atlas.getHeight()==1000);
        for(size_t s=0;s<stateNames.size();++s){
            surfaces_[s]=load("surface_"+juce::String(stateNames[s]));
            indicators_[s]=load("indicator_"+juce::String(stateNames[s]));
            for(int band=0;band<7;++band){
                lettering_[(size_t)band][s]=load("lettering_BAND_"+juce::String(band+1)+"_"+stateNames[s]);
                // R02 cells are 143x50 reference units at 4x, NOT legacy 408x124.
                endpoints_[(size_t)band][s]=atlas.getClippedImage({band*572,(int)s*200,572,200});
            }
        }
    }
    juce::Image housing_;
    std::array<juce::Image,5> surfaces_,indicators_;
    std::array<std::array<juce::Image,5>,7> lettering_,endpoints_;
};
inline void draw(juce::Graphics& g,juce::Rectangle<float> bounds,int band,State s,float d){
    jassert(band>=0&&band<7);
    const auto& images=Images::get();
    const auto f=fit(bounds.getWidth(),bounds.getHeight());
    const float x=bounds.getX()+f.textureX,y=bounds.getY()+f.textureY,k=f.scale/4.f;
    const juce::AffineTransform stationary(k,0,x,0,k,y);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    // At endpoints use the supplied composite verbatim. During travel use the
    // registered zero-travel layers, never translate a padded composite.
    if(d==targetTravel(s)){
        g.drawImageTransformed(images.endpoint(band,s),stationary);return;
    }
    g.drawImageTransformed(images.housing(),stationary);
    g.drawImageTransformed(images.surface(s),juce::AffineTransform(k,0,x,0,k*surfaceScale(d),y+f.scale*surfaceOffset(d)));
    g.drawImageTransformed(images.lettering(band,s),juce::AffineTransform(k,0,x,0,k,y+f.scale*d));
    g.drawImageTransformed(images.indicator(s),stationary);
}
}

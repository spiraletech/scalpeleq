#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
#include "SurgeryChassisLayout.h"
#include "SurgeryWeatherInterpretation.h"
#include "SurgerySynesthesia.h"

namespace surgery::rack {
// JUKE JUICE: a border-only renderer. No parameters, analyzer writes or DSP.
class StormChassis final {
public:
    StormChassis() {
        // The supplied artwork is used for the four X-head screws only.
        // All energy is generated from current measurements, not neon artwork.
        const auto reference=juce::ImageCache::getFromMemory(BinaryData::approvedreference_png,BinaryData::approvedreference_pngSize);
        auto sample=[&](juce::Image& image,juce::Rectangle<int> source){juce::Graphics g(image);g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImage(reference,0,0,image.getWidth(),image.getHeight(),source.getX(),source.getY(),source.getWidth(),source.getHeight());};
        // Plasma pixels from the reference are deliberately not used.
        for(int i=0;i<4;++i){screws_[(size_t)i]=juce::Image(juce::Image::RGB,40,40,false,juce::SoftwareImageType{});sample(screws_[(size_t)i],{i%2?917:3,i/2?630:11,40,40});}
    }
    struct State { double energy=0,glow=.012,phase=0,temperature=.5,level=0,bass=0,flux=0,width=0,flash=0,flashPosition=.5; };
    struct OpticalCell {float r=0,g=0,b=0,core=0;bool operator==(const OpticalCell&) const = default;};
    struct Statistics { double meanDensity=0,litFraction=0,renderedLitFraction=0;float maximum=0;std::array<double,3> chamberOccupancy{};std::array<std::array<int,3>,3> colourFamilies{}; };
    static juce::Rectangle<float> top(int w) { return {70.f,6.f,w-140.f,28.f}; }
    static juce::Rectangle<float> side(int w,int h,bool right) { return {right?w-61.f:5.f,52.f,56.f,h-104.f}; }
    const State& state() const noexcept { return state_; }
    const auto& densityField() const noexcept {return density_;}
    const auto& opticalField() const noexcept {return optics_;}
    Statistics statistics() const noexcept {
        Statistics out;for(float d:density_){out.meanDensity+=d;out.litFraction+=d>.05f?1:0;out.maximum=std::max(out.maximum,d);}
        out.meanDensity/=density_.size();out.litFraction/=density_.size();out.renderedLitFraction=visiblePixels_/(double)density_.size();
        for(int i=0;i<3;++i)out.chamberOccupancy[(size_t)i]=chamberVisible_[(size_t)i]/(double)(i==0?12288:9216);out.colourFamilies=colourFamilies_;return out;
    }
    // Existing Storm values only. SONAR OFF overrides visibility even while
    // frozen; it never alters the held field. No bright decorative base fill.
    bool update(const weather::Meaning& measured,double dt,double decay,bool frozen,bool reduced,bool active=true,int mode=2) {
        active=active&&mode!=5;const bool gateChanged=enabled_!=active;enabled_=active;frozen_=frozen;
        if(frozen){pendingDt_=0;if(gateChanged)dirty_=true;return gateChanged;}
        if(mode_!=mode){mode_=std::clamp(mode,0,5);dirty_=true;}
        reduced_=reduced;dt=std::clamp(std::isfinite(dt)?dt:0.,0.,.25);
        decay_=std::clamp(std::isfinite(decay)?decay:.18,.05,10.);
        const auto previous=state_;
        const double level=active?weather::unit(measured.level):0;
        const double gate=weather::unit(level/.08);
        const double target=gate*weather::unit(.30*level+.35*measured.bass+.20*measured.flux+.15*measured.width);
        auto smooth=[&](double& value,double goal){const double tau=goal>value?.025:decay_;value=goal+(value-goal)*std::exp(-dt/tau);if(std::abs(value)<1.e-6)value=0;};
        const auto profile=personality(mode_);
        smooth(state_.energy,target);smooth(state_.level,level);
        colourBias_=active?synesthesia::musicBias(measured):0;
        smooth(state_.bass,active?weather::unit(measured.bass):0);smooth(state_.flux,active?weather::unit(measured.flux):0);smooth(state_.width,active?weather::unit(measured.width):0);
        smooth(state_.temperature,level>1.e-6?weather::unit(measured.centroid):.5);
        state_.flash=active&&!reduced?std::min(.10f,measured.flash):0;state_.flashPosition=weather::unit(measured.flashPosition);
        state_.glow=.012+.988*std::pow(state_.energy,1.35);
        if(active&&state_.level>1.e-5)
            state_.phase=std::fmod(state_.phase+dt*(.25+.60*state_.flux+.20*state_.bass)*state_.energy*profile.speed*(reduced?.035:1.),64.);
        pendingDt_=std::min(.25,pendingDt_+dt);
        const bool changed=gateChanged||std::abs(previous.glow-state_.glow)>1.e-6||previous.phase!=state_.phase||activity_>1.e-5;
        if(changed)dirty_=true;return changed;
    }
    // Static carbon, brushed socket hardware and engraved micro-equations.
    // The inner hole and instrument transform never change.
    void paintStatic(juce::Graphics& g,int w,int h) {
        juce::Graphics::ScopedSaveState protectedFace(g);g.excludeClipRegion(instrumentArea(w,h).toNearestInt());
        juce::Path shell;shell.setUsingNonZeroWinding(false);
        shell.addRoundedRectangle(2,2,w-4.f,h-4.f,17);
        shell.addRoundedRectangle(68,38,w-136.f,h-90.f,15);
        g.setTiledImageFill(carbon_,0,0,1);g.fillPath(shell);
        g.setColour(juce::Colour(0xff06080b));g.drawRoundedRectangle({1.5f,1.5f,w-3.f,h-3.f},17,3);
        juce::ColourGradient rim(juce::Colour(0xff8b969e),0,0,juce::Colour(0xff151a21),0,(float)h,false);
        rim.addColour(.05,juce::Colour(0xff303b43));rim.addColour(.94,juce::Colour(0xff64727b));
        g.setGradientFill(rim);g.drawRoundedRectangle({3.5f,3.5f,w-7.f,h-7.f},15,1.1f);
        g.setColour(juce::Colour(0xff02070b));g.drawRoundedRectangle({66,36,w-132.f,h-86.f},16,4);
        g.setColour(juce::Colour(0xff48565f));g.drawRoundedRectangle({68,38,w-136.f,h-90.f},15,1);
        g.setColour(juce::Colour(0xff14252d));g.drawRoundedRectangle({70,40,w-140.f,h-94.f},14,.7f);
        chamber(g,top(w),false,juce::Colour(0xff21bfff));
        chamber(g,side(w,h,false),true,juce::Colour(0xff078dff));
        chamber(g,side(w,h,true),true,juce::Colour(0xfff32baf));
        for(bool right:{false,true})for(bool bottom:{false,true})hardware(g,right?w-23.f:23.f,bottom?h-23.f:23.f,(right?1:0)+(bottom?2:0));
        g.setFont(juce::Font(juce::FontOptions(9.f,juce::Font::bold)));
        g.setColour(juce::Colour(0xff82939f).withAlpha(.68f));
        g.drawText("SPIRAL ETHER TECH   /   JUKE JUICE",76,h-39,w-152,13,juce::Justification::centred);
        g.setFont(juce::Font(juce::FontOptions(8.f)));
        g.setColour(juce::Colour(0xff657580));
        g.drawText("E(t) = .30L + .35B + .20F + .15W    /    G = .012 + .988 E^1.35    /    DECAY tau",76,h-24,w-152,11,juce::Justification::centred);
    }
    void paintLiquid(juce::Graphics& g,int w,int h) {
        if(!g.clipRegionIntersects(top(w).expanded(6).toNearestInt())&&!g.clipRegionIntersects(side(w,h,false).expanded(6).toNearestInt())&&!g.clipRegionIntersects(side(w,h,true).expanded(6).toNearestInt()))return;
        juce::Graphics::ScopedSaveState protectedFace(g);g.excludeClipRegion(instrumentArea(w,h).toNearestInt());
        if(maskWidth_!=w||maskHeight_!=h){
            maskWidth_=w;maskHeight_=h;int offset=0;
            for(int index=0;index<3;++index){const auto r=index==0?top(w):side(w,h,index==2);const auto& image=index==0?topLiquid_:index==1?leftLiquid_:rightLiquid_;
                const float radius=std::min(r.getWidth(),r.getHeight())*.5f,pixel=std::max(r.getWidth()/image.getWidth(),r.getHeight()/image.getHeight());
                for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x){
                    const float px=(x+.5f)*r.getWidth()/image.getWidth(),py=(y+.5f)*r.getHeight()/image.getHeight();
                    const float dx=std::max({radius-px,0.f,px-(r.getWidth()-radius)}),dy=std::max({radius-py,0.f,py-(r.getHeight()-radius)});
                    masks_[(size_t)offset++]=(std::uint8_t)std::round(255*weather::unit(.5f-(std::sqrt(dx*dx+dy*dy)-radius)/pixel));
                }
            }
            dirty_=true;
        }
        if(dirty_){activity_=0;visiblePixels_=0;chamberVisible_.fill(0);colourFamilies_={};render(topLiquid_,0);render(leftLiquid_,1);render(rightLiquid_,2);dirty_=false;pendingDt_=0;}
        // Rounded alpha is precomputed only when bounds change. Chrome/glass
        // reflections are static-cache work; no per-frame vector path clips.
        g.setOpacity(1);g.drawImage(topLiquid_,top(w));g.drawImage(leftLiquid_,side(w,h,false));g.drawImage(rightLiquid_,side(w,h,true));
    }
private:
    int mode_=2;
    struct Personality {float body,gain,speed,turbulence,core;};
    static Personality personality(int mode) noexcept {
        constexpr std::array<Personality,6> modes{{{1.10f,1.08f,.75f,.65f,1.8f},{.94f,1.12f,1.30f,1.9f,2.2f},{1,1,1,1,1},{.78f,1.15f,1.4f,.3f,2.5f},{1.15f,.35f,.15f,.12f,.5f},{1,0,0,0,0}}};
        return modes[(size_t)std::clamp(mode,0,5)];
    }
    State state_{};bool dirty_=true,enabled_=false,frozen_=false,reduced_=true;
    float colourBias_=0;
    double pendingDt_=0,decay_=.18;float activity_=0;int visiblePixels_=0;
    std::array<float,30720> density_{};
    std::array<OpticalCell,30720> optics_{};
    std::array<int,3> chamberVisible_{};std::array<std::array<int,3>,3> colourFamilies_{};
    std::array<std::uint8_t,30720> masks_{};int maskWidth_=0,maskHeight_=0;
    juce::Image topLiquid_{juce::Image::ARGB,512,24,false};
    juce::Image leftLiquid_{juce::Image::ARGB,24,384,false},rightLiquid_{juce::Image::ARGB,24,384,false};
    std::array<juce::Image,4> screws_;
    juce::Image carbon_=makeCarbon();
    static juce::Image makeCarbon() {
        juce::Image image(juce::Image::RGB,64,64,false,juce::SoftwareImageType{});
        juce::Image::BitmapData pixels(image,juce::Image::BitmapData::writeOnly);
        for(int y=0;y<64;++y)for(int x=0;x<64;++x){
            const int a=(x+y)%16,b=(x-y+128)%16;
            const bool over=((x/8+y/8)&1)==0;
            const float fibre=(over?(a<8?a:15-a):(b<8?b:15-b))/7.f;
            const float v=.035f+.075f*fibre+((over?x:y)%2?.012f:0);
            pixels.setPixelColour(x,y,juce::Colour::fromFloatRGBA(v,v*1.10f,v*1.23f,1));
        }
        return image;
    }
    static juce::Colour plasmaColour(float balance) {
        const auto c=synesthesia::palette(balance);return juce::Colour::fromFloatRGBA(c.r,c.g,c.b,1);
    }
    // A continuous broad river with near-straight banks. Motion is internal
    // advection/shear and four submerged streams, never a travelling spine.
    void render(juce::Image& image,int orientation) {
        juce::Image::BitmapData output(image,juce::Image::BitmapData::writeOnly);
        const int length=orientation?image.getHeight():image.getWidth(),breadth=orientation?image.getWidth():image.getHeight();
        const int offset=orientation==0?0:orientation==1?12288:21504;
        const float phase=(float)state_.phase,phi=phase*juce::MathConstants<float>::twoPi;
        const float energy=(float)(state_.energy*state_.level),bass=(float)state_.bass,flux=(float)state_.flux,width=(float)state_.width,centroid=(float)state_.temperature;
        const auto profile=personality(mode_);
        const float spread=.06f+.07f*width,bodyWidth=(.135f+.035f*bass)*profile.body,coreWidth=.032f+.012f*bass;
        const float agitation=flux*profile.turbulence*(reduced_?.0015f:.012f),bias=orientation==1?-.045f:orientation==2?.045f:0;
        const double dt=frozen_?0:pendingDt_,attack=std::exp(-dt/.035),release=std::exp(-dt/decay_);
        std::array<float,24> reflection{};
        for(int j=0;j<breadth;++j){const float v=j/(float)(breadth-1),a=(v-.18f)/.035f,b=(v-.80f)/.025f;reflection[(size_t)j]=.12f/(1+a*a)+.05f/(1+b*b);}
        const int currents=mode_==2?4:std::clamp(2+(int)std::round(width*2),2,4);
        for(int i=0;i<length;++i){
            const float u=i/(float)(length-1),bodyCentre=.5f+.012f*std::sin(u*19-phi*2);
            std::array<float,4> centre{},pulse{},weight{};std::array<juce::Colour,4> colours{};
            const std::array<float,4> hue{.08f,.40f,.70f,.96f};
            for(int strand=0;strand<currents;++strand){
                centre[(size_t)strand]=bodyCentre+spread*((strand-(currents-1)*.5f)/std::max(.5f,(currents-1)*.5f))+(.008f+agitation)*std::sin(u*(19+strand*2.f)-phi*(1+strand*.5f)+strand*1.8f);
                const float position=std::fmod(phase*(.75f+strand*.125f)+strand*.25f+orientation*.07f,1.f);
                const float raw=std::abs(u-position),distance=std::min(raw,1-raw),q=distance/(.095f+.07f*bass),z=1+q*q;
                pulse[(size_t)strand]=.65f+.35f/(z*z);
                weight[(size_t)strand]=strand==0?.42f:strand==1?.35f+.15f*(1-std::abs(centroid-.5f)*2):strand==2?.30f+.20f*centroid:.30f+.25f*flux+.15f*centroid;
                colours[(size_t)strand]=plasmaColour(hue[(size_t)strand]+(centroid-.5f)*.16f-.08f*bass+.10f*flux+bias);
            }
            const auto bodyColour=plasmaColour(.45f+.35f*std::sin(u*14-phi)+(centroid-.5f)*.23f-.15f*bass+bias+colourBias_);
            const auto hazeColour=plasmaColour(.42f+.30f*centroid+.1f*std::sin(u*5-phi));
            const float pressure=1+(mode_==2?0:.10f)*bass*std::sin(u*18-phi*2);
            const float rail=mode_==3?.55f+.45f*std::pow(.5f+.5f*std::sin(u*22-phi*3),2.f):1;
            const float bodyPulse=(.78f+.22f*pulse[0])*rail,flashQ=(u-(float)state_.flashPosition)/.07f,flash=(float)state_.flash/(1+flashQ*flashQ);
            for(int j=0;j<breadth;++j){
                const float v=j/(float)(breadth-1),d=(v-bodyCentre)/(bodyWidth*pressure),h=(v-bodyCentre)/(.18f+.06f*bass),hz=1+h*h;
                const float shear=.80f+.20f*std::sin(u*31-phi*2+v*17+agitation*10*std::sin(u*43-phi*3));
                const float body=1.55f*bodyPulse*shear/(1+d*d*d*d*d*d),haze=.055f*bodyPulse/(hz*hz);
                float mass=body+haze,red=body*bodyColour.getFloatRed()+haze*hazeColour.getFloatRed(),green=body*bodyColour.getFloatGreen()+haze*hazeColour.getFloatGreen(),blue=body*bodyColour.getFloatBlue()+haze*hazeColour.getFloatBlue(),core=0;
                for(int strand=0;strand<currents;++strand){
                    const float radius=coreWidth,q=(v-centre[(size_t)strand])/radius,cz=1+q*q;
                    const float ribbon=profile.core*weight[(size_t)strand]*pulse[(size_t)strand]/(cz*cz*cz);
                    mass+=ribbon;core+=ribbon;red+=ribbon*colours[(size_t)strand].getFloatRed();green+=ribbon*colours[(size_t)strand].getFloatGreen();blue+=ribbon*colours[(size_t)strand].getFloatBlue();
                }
                const float target=enabled_?weather::unit(std::min(.60f,profile.gain*energy*(1.05f+.45f*bass+.55f*flux))*mass):0;
                const int ox=orientation?j:i,oy=orientation?i:j,index=offset+oy*image.getWidth()+ox;
                auto& density=density_[(size_t)index];auto& optical=optics_[(size_t)index];
                const double response=target>density?attack:release;
                auto follow=[&](float& shown,float goal){shown=(float)(goal+(shown-goal)*response);if(shown<1.e-6f)shown=0;};
                follow(density,target);const float scale=mass>0?target/mass:0;
                follow(optical.r,red*scale);follow(optical.g,green*scale);follow(optical.b,blue*scale);follow(optical.core,core*scale);
                activity_=std::max(activity_,density);
                const float illumination=enabled_?std::max(0.f,density-.015f)*(.8f+.6f*(float)state_.glow)*(1+flash*2):0;
                const float alpha=masks_[(size_t)index]/255.f*(illumination*4.8f/(1+illumination*4.8f));
                if(alpha>.10f){++visiblePixels_;++chamberVisible_[(size_t)orientation];}
                const float denominator=std::max(1.e-6f,density),white=weather::unit(optical.core*(.18f+.15f*flux)+flash*optical.core*2);
                const float gloss=reflection[(size_t)j];
                auto channel=[&](float c,float glass){c=weather::unit(c/denominator);return weather::unit((c+(1-c)*white)*(1-gloss)+glass*gloss);};
                const float r=channel(optical.r,.85f),g=channel(optical.g,.95f),b=channel(optical.b,1);
                if(alpha>.15f){auto& family=colourFamilies_[(size_t)orientation];if(b>r*1.2f&&g>r)++family[0];if(b>g*1.15f&&r>g*1.15f)++family[1];if(r>g*1.3f&&r>b*.8f)++family[2];}
                output.setPixelColour(ox,oy,juce::Colour::fromFloatRGBA(r,g,b,alpha));
            }
        }
    }
    void hardware(juce::Graphics& g,float x,float y,int index) {
        juce::Graphics::ScopedSaveState saved(g);juce::Path clip;clip.addEllipse(x-19,y-19,38,38);g.reduceClipRegion(clip);
        g.setOpacity(1);g.drawImage(screws_[(size_t)index],{x-19,y-19,38,38});
    }
    static void chamber(juce::Graphics& g,juce::Rectangle<float> r,bool vertical,juce::Colour accent) {
        const float radius=std::min(r.getWidth(),r.getHeight())*.5f;
        g.setColour(juce::Colour(0xff010308));g.fillRoundedRectangle(r.expanded(5),radius+5);
        juce::ColourGradient rim(juce::Colour(0xff9cabb5),r.getX(),r.getY(),juce::Colour(0xff13212d),r.getRight(),r.getBottom(),false);
        rim.addColour(.3,juce::Colour(0xff273846));rim.addColour(.6,juce::Colour(0xff78909e));rim.addColour(.8,juce::Colour(0xff111c25));
        g.setGradientFill(rim);g.drawRoundedRectangle(r.expanded(3),radius+3,1.5f);
        g.setColour(accent.withAlpha(.10f));g.drawRoundedRectangle(r.expanded(1),radius+1,1.2f);
        g.setColour(juce::Colour(0xff02080d).withAlpha(.72f));g.fillRoundedRectangle(r,radius);
        // Contained cylindrical glass: shaded edges, polished seams and a
        // narrow reflection. No bloom may escape into the instrument face.
        juce::ColourGradient glass(juce::Colours::black.withAlpha(.64f),r.getX(),r.getY(),juce::Colours::black.withAlpha(.62f),vertical?r.getRight():r.getX(),vertical?r.getY():r.getBottom(),false);
        glass.addColour(.16,juce::Colours::white.withAlpha(.075f));glass.addColour(.25,juce::Colours::white.withAlpha(.02f));glass.addColour(.5,juce::Colours::transparentBlack);glass.addColour(.80,juce::Colours::white.withAlpha(.04f));
        g.setGradientFill(glass);g.fillRoundedRectangle(r,radius);
        g.setColour(juce::Colour(0xffe6f8ff).withAlpha(.28f));g.drawRoundedRectangle(r.reduced(2),radius-2,.85f);
        g.setColour(accent.withAlpha(.045f));g.drawRoundedRectangle(r.reduced(4),radius-4,.7f);
    }
};
} // namespace surgery::rack

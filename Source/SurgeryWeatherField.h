#pragma once
#include "SurgeryWeatherInterpretation.h"
#include "SurgerySynesthesia.h"
namespace surgery::weather {
struct LinearRGB {float r=0,g=0,b=0;};
struct Pixel {std::uint8_t r=0,g=0,b=0;};
inline float linear(float c) noexcept {return c<=.04045f?c/12.92f:(float)std::pow((c+.055)/1.055,2.4);}
inline float srgb(float c) noexcept {c=unit(c);return c<=.0031308f?c*12.92f:(float)(1.055*std::pow(c,1/2.4)-.055);}
inline LinearRGB fromHex(unsigned rgb) noexcept {return {linear(((rgb>>16)&255)/255.f),linear(((rgb>>8)&255)/255.f),linear((rgb&255)/255.f)};}
inline LinearRGB pastel(float feature) noexcept {
    static const std::array<LinearRGB,7> palette{fromHex(0xf6c18b),fromHex(0xf3b1c8),fromHex(0xf4e48b),fromHex(0xbee8c8),fromHex(0x8fe7d4),fromHex(0x9ed8f7),fromHex(0xc8b6f2)};
    const float u=unit(feature)*6;const int a=std::min(5,(int)u);const float t=u-a,s=t*t*(3-2*t);
    const auto x=palette[(size_t)a],y=palette[(size_t)a+1];return {x.r+(y.r-x.r)*s,x.g+(y.g-x.g)*s,x.b+(y.b-x.b)*s};
}
class Field final {
public:
    static constexpr int width=128,height=64,count=width*height;
    struct Cell {float density=0,illumination=0,vx=0,vy=0,x=0,y=0,seed=0;};
    Field() noexcept {
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            const double a=x/(double)(width-1),b=y/(double)(height-1);
            cells_[(size_t)(y*width+x)].seed=unit(.5+.23*std::sin(a*18.85+std::sin(b*6.28))+.17*std::sin(b*25.13+a*5));
        }
        pixels_.fill({11,17,24});
    }
    bool update(Interpretation& interpretation,double elapsed,double memory,bool reduced,bool advance=true) noexcept {
        // Elapsed-time analytic damping; long UI stalls are bounded and aged
        // snapshots naturally release to silence. No frame-count physics.
        const double dt=std::clamp(std::isfinite(elapsed)?elapsed:0.,0.,60.);
        const auto f=advance?interpretation.update(dt,memory,reduced):interpretation.state();
        if(f.level<1.e-5f&&activity_<1.e-5f)return false;
        activity_=0;
        const auto& input=interpretation.measured();const auto& rootsU=interpretation.rootIntensity();
        const double release=.5+1.5*Interpretation::memoryUnit(memory),decay=std::exp(-dt/release),attack=std::exp(-dt/.035),damping=std::exp(-3*dt);
        std::array<float,width> column{},lateral{},flash{},ripple{};std::array<LinearRGB,width> colours{};std::array<float,height> cloud{};
        const double high=std::max(21.,std::min(20000.,(double)input.bandwidthHz));
        for(int x=0;x<width;++x){
            const double u=x/(double)(width-1),hz=20*std::pow(high/20,u);double local=0;int covering=0;
            for(int r=0;r<7;++r){const auto a=input.boundaries[(size_t)r*2],b=input.boundaries[(size_t)r*2+1];if(hz>=std::min(a,b)&&hz<=std::max(a,b)){local+=rootsU[(size_t)r];++covering;}}
            column[(size_t)x]=f.level*(.25f+.75f*(covering?(float)(local/covering):f.level*.15f));
            const double span=.15+.65*f.width;lateral[(size_t)x]=(float)std::exp(-std::pow((u-.5)/span,2));
            ripple[(size_t)x]=(float)std::exp(-std::pow((u-input.fluxPosition)/.14,2));
            flash[(size_t)x]=f.flash*(float)std::exp(-std::pow((u-f.flashPosition)/.12,2));
            const auto tint=synesthesia::read(f,(float)u,.10f);colours[(size_t)x]={linear(tint.r),linear(tint.g),linear(tint.b)};
        }
        for(int y=0;y<height;++y){const double v=y/(double)(height-1),centre=.76-.24*f.bass,spread=.14+.23*std::pow(f.bass,1.5);cloud[(size_t)y]=(float)std::exp(-std::pow((v-centre)/spread,2));}
        const float mass=.30f+.70f*std::pow(f.bass,1.5f);
        static const auto base=fromHex(0x0b1118),ivory=fromHex(0xf4eedf);
        static const auto bytes=[](){std::array<std::uint8_t,4097> table{};for(int i=0;i<=4096;++i)table[(size_t)i]=(std::uint8_t)std::round(srgb(i/4096.f)*255);return table;}();
        auto encode=[&](float value){return bytes[(size_t)std::clamp((int)std::round(unit(value)*4096),0,4096)];};
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            auto& c=cells_[(size_t)(y*width+x)];
            if(reduced){c.vx=c.vy=c.x=c.y=0;}
            else {
                const float ax=f.level*(.08f*(f.width-.5f)+.035f*(c.seed-.5f)),ay=f.level*(.015f*(f.centroid-.5f)-.025f*f.bass+.02f*f.flux*(c.seed-.5f));
                auto integrate=[&](float& v,float& p,float force){const double equilibrium=force/3.;p=(float)std::clamp(p+(v-equilibrium)*(1-damping)/3+equilibrium*dt,-.08,.08);v=(float)(v*damping+equilibrium*(1-damping));};
                integrate(c.vx,c.x,ax);integrate(c.vy,c.y,ay);
            }
            const float texture=sampleSeed(x+c.x*width,y+c.y*height);
            const float grain=std::max(0.f,(texture-.65f)/.35f);
            const float rain=.035f*f.flux*f.centroid*f.level*grain*grain*grain*grain;
            const float target=unit(column[(size_t)x]*lateral[(size_t)x]*cloud[(size_t)y]*(.45f+.7f*texture)*mass*(.85f+.15f*f.storm)*(1+.2f*f.flux*ripple[(size_t)x]*(texture-.5f))+rain);
            c.density=unit(target+(c.density-target)*(target>c.density?attack:decay));
            c.illumination=reduced?0:std::min(.10f,flash[(size_t)x]*cloud[(size_t)y]);
            activity_=std::max(activity_,c.density+c.illumination+std::abs(c.vx)+std::abs(c.vy));
            const float alpha=std::min(.28f,.28f*c.density),light=c.illumination;
            const auto colour=colours[(size_t)x];
            pixels_[(size_t)(y*width+x)]={encode(base.r*(1-alpha-light)+colour.r*alpha+ivory.r*light),encode(base.g*(1-alpha-light)+colour.g*alpha+ivory.g*light),encode(base.b*(1-alpha-light)+colour.b*alpha+ivory.b*light)};
        }
        return true;
    }
    void clear() noexcept {for(auto& c:cells_)c.density=c.illumination=c.vx=c.vy=c.x=c.y=0;activity_=0;pixels_.fill({11,17,24});}
    const auto& pixels() const noexcept {return pixels_;}
    const auto& cells() const noexcept {return cells_;}
private:
    float sampleSeed(float x,float y) const noexcept {
        x=std::clamp(x,0.f,(float)width-1.001f);y=std::clamp(y,0.f,(float)height-1.001f);
        const int a=(int)x,b=(int)y;const float u=x-a,v=y-b;
        auto seed=[&](int xx,int yy){return cells_[(size_t)(yy*width+xx)].seed;};
        return (1-v)*((1-u)*seed(a,b)+u*seed(a+1,b))+v*((1-u)*seed(a,b+1)+u*seed(a+1,b+1));
    }
    float activity_=0;
    std::array<Cell,count> cells_{};
    std::array<Pixel,count> pixels_{};
};
} // namespace surgery::weather

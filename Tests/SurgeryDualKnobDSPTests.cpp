#include "../Source/SurgeryKnobCalibration.h"
#include "../Source/RootsAudioEngine.h"
#include "../Source/SurgeryCarveEngine.h"
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <limits>
using namespace surgery::calibration;
struct Result {double power=0,peak=0,jump=0,gr=0;bool finite=true;};
static Result compressor(int mode,double threshold,double sr){
    auto engine=std::make_unique<prism::roots::v2::Engine>(sr);
    auto c=engine->get(8);c.comp=(prism::roots::v2::Compressor)mode;c.thresholdDb=threshold;engine->config(8,c);
    engine->prepare(sr);engine->setSoloRoot(4);
    std::array<float,256> l{},r{};Result result;double last=0;int count=0;
    for(int offset=0;offset<(int)(sr*.65);offset+=256){
        for(int i=0;i<256;++i)l[(size_t)i]=r[(size_t)i]=(float)(.25*std::sin(2*std::numbers::pi*1000*(offset+i)/sr));
        engine->process(l.data(),r.data(),256);
        for(auto value:l){result.finite&=std::isfinite(value);result.peak=std::max(result.peak,std::abs((double)value));result.jump=std::max(result.jump,std::abs(value-last));last=value;
            if(offset>sr*.4){result.power+=value*value;++count;}}
        result.gr=std::max(result.gr,engine->meter().largestGrDb);
    }
    result.power/=std::max(1,count);return result;
}
static Result carve(double q,double frequency,double sr,bool sweep=false){
    auto engine=std::make_unique<surgery::CarveEngine>();
    surgery::CarveConfig c;c.enabled=true;c.hz=1000;c.depthDb=24;c.q=q;
    engine->configure(4,0,c);engine->prepare(sr);
    std::array<float,64> l{},r{};Result result;double last=0;int count=0;
    for(int offset=0;offset<(int)(sr*.65);offset+=64){
        if(sweep){c.q=qFromPosition((double)offset/(sr*.65));engine->configure(4,0,c);}
        for(int i=0;i<64;++i)l[(size_t)i]=r[(size_t)i]=(float)(.25*std::sin(2*std::numbers::pi*frequency*(offset+i)/sr));
        engine->process(l.data(),r.data(),64);
        for(auto value:l){result.finite&=std::isfinite(value);result.peak=std::max(result.peak,std::abs((double)value));result.jump=std::max(result.jump,std::abs(value-last));last=value;
            if(offset>sr*.4){result.power+=value*value;++count;}}
    }
    result.power/=std::max(1,count);return result;
}
int main(){
    const auto fail=[](int line){std::cerr<<"Dual knob DSP calibration failed line "<<line<<'\n';return 1;};
    for(int i=0;i<=1000;++i){double p=i/1000.;
        if(std::abs(thresholdPosition(thresholdFromPosition(p))-p)>1e-12||std::abs(qPosition(qFromPosition(p))-p)>1e-12)return fail(__LINE__);}
    if(thresholdFromPosition(0)!=0||thresholdFromPosition(.5)!=-30||thresholdFromPosition(1)!=-60||
       qFromPosition(0)!=.3||std::abs(qFromPosition(.5)-std::sqrt(.3*96))>1e-12||qFromPosition(1)!=96)return fail(__LINE__);
    if(qFromPosition(-2)!=.3||qFromPosition(2)!=96||thresholdFromPosition(2)!=-60||
       !std::isfinite(qFromPosition(std::numeric_limits<double>::quiet_NaN())))return fail(__LINE__);
    std::ofstream csv("surgery-dual-knob-dsp.csv");csv<<"test,rate,profile,value,power,peak,max_sample_delta,gain_reduction_db\n";
    for(double sr:{44100.,48000.,96000.}){
        for(int mode=0;mode<5;++mode){
            std::array<Result,3> measured{};int i=0;
            for(double db:{0.,-30.,-60.}){auto result=compressor(mode,db,sr);measured[(size_t)i++]=result;
                csv<<"compression,"<<sr<<','<<mode<<','<<db<<','<<result.power<<','<<result.peak<<','<<result.jump<<','<<result.gr<<'\n';
                if(!result.finite||result.peak>.55||result.jump>.15)return fail(__LINE__);}
            if(mode==0){if(measured[0].power!=measured[1].power||measured[0].power!=measured[2].power||measured[2].gr!=0)return fail(__LINE__);}
            else if(!(measured[1].power<measured[0].power*.9&&measured[2].power<measured[1].power*.2&&measured[1].gr>.1))return fail(__LINE__);
        }
        double previous=0;
        for(double p:{0.,.5,1.}){const double q=qFromPosition(p);auto side=carve(q,1300,sr),center=carve(q,1000,sr);
            csv<<"carve_1300,"<<sr<<",0,"<<q<<','<<side.power<<','<<side.peak<<','<<side.jump<<",0\n";
            if(!side.finite||!center.finite||side.power<=previous||side.peak>.55||center.peak>.55)return fail(__LINE__);
            const double cut=-10*std::log10(center.power/(.25*.25*.5));if(std::abs(cut-24)>1)return fail(__LINE__);
            previous=side.power;
        }
        const auto swept=carve(.3,1000,sr,true);csv<<"carve_sweep,"<<sr<<",0,0,"<<swept.power<<','<<swept.peak<<','<<swept.jump<<",0\n";
        if(!swept.finite||swept.peak>.55||swept.jump>.15)return fail(__LINE__);
    }
    std::cout<<"DUAL KNOB DSP: 0/-30/-60 threshold; 0/50/100 logarithmic Q; all five compressor profiles at 44.1/48/96k; OFF threshold-independent; narrower CARVEs; bounded finite sweeps passed\n";
}

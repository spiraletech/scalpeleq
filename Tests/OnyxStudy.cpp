#include "../Source/SurgeryOnyx.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <vector>

// Exact 1.0.1 standalone transfer, at settled parameter values.
struct Baseline {
    double air=0,presence=0,soft=0,dc=0,sr=48000;
    double sample(double x,const std::array<double,4>& a){
        auto alpha=[&](double f){return 1-std::exp(-2*std::numbers::pi*f/sr);};
        air+=alpha(4200)*(x-air);double y=x;
        if(a[0]>1e-10)y+=a[0]*.20*std::tanh((x-air)*(1+a[0]));
        presence+=alpha(1600)*(y-presence);y+=a[1]*.30*(y-presence);
        soft+=alpha(16000-13400*a[2])*(y-soft);y+=a[2]*.88*(soft-y);
        if(a[3]>0){double d=1+3.5*a[3];y+=a[3]*(std::tanh(d*y)/std::tanh(d)-y);dc+=alpha(8)*(y-dc);y-=dc;}
        return y;
    }
};
int main(int argc,char** argv){
    const bool render=argc>1;
    std::ofstream manifest;
    if(render){manifest.open(std::string(argv[1])+"/manifest.csv");manifest<<"file,sr,knob,amount,level,hz,version\n";}
    double maxNeutral=0,maxPeak=0;int index=0;
    for(double sr:{44100.,48000.,96000.}){
        surgery::Onyx zero;zero.prepare(sr);
        for(int n=0;n<(int)sr;++n){double x=.7*std::sin(2*std::numbers::pi*997*n/sr);maxNeutral=std::max(maxNeutral,std::abs(x-zero.sample(x,{})));}
        for(int knob=0;knob<4;++knob)for(double amount:{.2,1.})for(double level:{.1,.7})for(double hz:{1000.,8000.}){
            surgery::Onyx fx;fx.prepare(sr);Baseline old;old.sr=sr;
            std::array<double,4>a{};a[(size_t)knob]=amount;
            std::vector<float> before((size_t)sr),after((size_t)sr);
            for(int n=0;n<2*(int)sr;++n){
                double x=level*std::sin(2*std::numbers::pi*hz*n/sr);
                double y=fx.sample(x,a),z=old.sample(x,a);
                if(!std::isfinite(y)||std::abs(y)>2)throw std::runtime_error("ONYX unstable");
                maxPeak=std::max(maxPeak,std::abs(y));
                if(n>=(int)sr){after[(size_t)(n-(int)sr)]=(float)y;before[(size_t)(n-(int)sr)]=(float)z;}
            }
            if(sr==44100&&amount==1&&level==.7&&hz==8000){
                double real=0,imag=0;
                for(size_t n=0;n<after.size();++n){
                    const double phase=2*std::numbers::pi*20100*(double)n/sr;
                    real+=after[n]*std::cos(phase);imag+=after[n]*std::sin(phase);
                }
                const double foldedThird=2*std::hypot(real,imag)/(sr*level);
                const double limit=knob==1?1e-5:knob==2?1e-4:.0032;
                if(foldedThird>limit)throw std::runtime_error("ONYX folded-third alias regression");
            }
            if(render)for(int v=0;v<2;++v){
                auto name="tone-"+std::to_string(index)+"-"+std::to_string(v)+".f32";
                const auto& data=v==0?before:after;
                std::ofstream out(std::string(argv[1])+"/"+name,std::ios::binary);
                out.write((const char*)data.data(),(std::streamsize)(data.size()*sizeof(float)));
                manifest<<name<<","<<sr<<","<<knob<<","<<amount<<","<<level<<","<<hz<<","<<v<<"\n";
            }
            ++index;
        }
        // Automation back to zero, silence decay and finite/DC regression.
        surgery::Onyx fx;fx.prepare(sr);double tail=0;
        for(int n=0;n<3*(int)sr;++n){double x=n<(int)sr?.8:0;tail=fx.sample(x,{1,1,1,1});}
        if(std::abs(tail)>1e-8)throw std::runtime_error("ONYX DC tail");
    }
    if(maxNeutral!=0)throw std::runtime_error("ONYX zero must null exactly");
    if(render){
        // Repeatable synthetic pluck/bass/noise percussion; no copyrighted source.
        unsigned random=12345;
        for(int knob=0;knob<4;++knob){
            surgery::Onyx fx;fx.prepare(48000);Baseline old;
            std::array<double,4>a{};a[(size_t)knob]=1;
            std::ofstream pulses(std::string(argv[1])+"/pulse-"+std::to_string(knob)+".f32",std::ios::binary);
            for(int n=0;n<48000;++n){
                double t=n/48000.;double x=(t>.1&&t<.15)?.7*std::sin(2*std::numbers::pi*6000*t):0;
                float values[]{(float)x,(float)old.sample(x,a),(float)fx.sample(x,a)};
                pulses.write((char*)values,sizeof(values));
            }
        }
        std::ofstream cpu(std::string(argv[1])+"/cpu.csv");cpu<<"version,seconds,audio_seconds,checksum\n";
        for(int v=0;v<2;++v){
            surgery::Onyx fx;fx.prepare(48000);Baseline old;double check=0;
            auto started=std::chrono::steady_clock::now();
            for(int n=0;n<48000*10;++n){double x=.7*std::sin(n*.13);check+=v==0?old.sample(x,{1,1,1,1}):fx.sample(x,{1,1,1,1});}
            double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
            cpu<<v<<","<<elapsed<<",10,"<<check<<"\n";
        }
        for(int knob=0;knob<4;++knob){
            surgery::Onyx fx;fx.prepare(48000);Baseline old;
            std::array<double,4>a{};a[(size_t)knob]=.5;
            std::ofstream dry(std::string(argv[1])+"/music-"+std::to_string(knob)+"-dry.f32",std::ios::binary);
            std::ofstream before(std::string(argv[1])+"/music-"+std::to_string(knob)+"-old.f32",std::ios::binary);
            std::ofstream after(std::string(argv[1])+"/music-"+std::to_string(knob)+"-new.f32",std::ios::binary);
            for(int n=0;n<48000*6;++n){
                double t=n/48000.,beat=std::fmod(t,.5),pluck=std::exp(-beat*10);
                random=1664525*random+1013904223;double noise=(random/4294967295.)*2-1;
                double x=.18*std::sin(2*std::numbers::pi*82.4069*t)*std::exp(-beat*4)
                    +pluck*(.12*std::sin(2*std::numbers::pi*329.6276*t)+.05*std::sin(2*std::numbers::pi*1977.7656*t))
                    +.10*noise*std::exp(-std::fmod(t,.125)*110);
                for(int channel=0;channel<2;++channel){
                    float d=(float)x;
                    // Mono reference is deliberate: stereo coherence is tested by the full engine suites.
                    if(channel==0){float b=(float)old.sample(x,a),v=(float)fx.sample(x,a);before.write((char*)&b,4);after.write((char*)&v,4);}
                    if(channel==0)dry.write((char*)&d,4);
                }
            }
        }
        std::ofstream transfer(std::string(argv[1])+"/transfer.csv");
        transfer<<"x,old_tape,new_tape\n";
        for(int n=0;n<=400;++n){
            double x=(n-200)/200.;
            // DC-free static curves of the nonlinear part, before filters/DC removal.
            transfer<<x<<","<<std::tanh(4.5*x)/std::tanh(4.5)<<","<<x+.6*(std::tanh(1.7*x)/1.7-x)<<"\n";
        }
    }
    std::cout<<"ONYX: 44.1/48/96k; four controls at 20/100%; 0.1/0.7 input; 1/8 kHz. Exact zero null="<<maxNeutral<<" max peak="<<maxPeak<<"; DC tail passed\n";
}

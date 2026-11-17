#include "../Source/RootsAudioEngine.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>

static bool audit=false;
static std::size_t allocations=0;
void* operator new(std::size_t n){
    if(audit)++allocations;
    if(auto* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}

int main(){
    using namespace prism::roots::v2;
    constexpr int block=256;
    double processedSeconds=0.0;
    const auto started=std::chrono::steady_clock::now();
    int failures=0;
    for(int sr:{44100,48000,96000,192000}){
        Engine engine(sr);
        for(int root=0;root<kTerritories;++root){
            auto left=engine.get(root*2),right=engine.get(root*2+1);
            left.enabled=right.enabled=false;
            engine.config(root*2,left);engine.config(root*2+1,right);
        }
        std::array<float,block> left{},right{};
        for(int i=0;i<block;++i){
            left[(std::size_t)i]=(float)(.17*std::sin(i*.071));
            right[(std::size_t)i]=(float)(.13*std::cos(i*.113));
        }
        allocations=0;audit=true;
        for(int offset=0;offset<sr;offset+=block){
            const int count=std::min(block,sr-offset);
            engine.process(left.data(),right.data(),count);
        }
        audit=false;
        processedSeconds+=1.0;
        bool finite=true;
        for(int i=0;i<block;++i)
            finite=finite&&std::isfinite(left[(std::size_t)i])&&std::isfinite(right[(std::size_t)i]);
        if(allocations!=0||!finite)++failures;
        std::cout<<"sr="<<sr<<" allocations="<<allocations<<" finite="<<finite<<'\n';
    }
    const double elapsed=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-started).count();
    const double realtimeRatio=elapsed/processedSeconds;
    std::cout<<"worst_case_all_7_off_elapsed_seconds="<<elapsed
             <<" processed_seconds="<<processedSeconds
             <<" realtime_ratio="<<realtimeRatio<<'\n';
    // A deliberately conservative gate: worst-case all-seven removal must
    // remain faster than real time on the CI reference host.
    if(realtimeRatio>=1.0)++failures;
    return failures?1:0;
}

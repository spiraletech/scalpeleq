#include "../Source/RootsSpectrumColours.h"
#include "../Source/RootsAudioEngine.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
namespace V=prism::roots::visual;
using prism::roots::v2::Engine;
int main(){
    // PROOF 1: Display Hz is exact and fully occupied, from 20 to 20000.
    if(V::graphUnit(20.0)!=0.0||
       std::abs(V::graphUnit(20000.0)-1.0)>1e-12||
       V::graphFrequency(0.0)!=20.0||
       std::abs(V::graphFrequency(1.0)-20000.0)>1e-7||
       std::abs(V::graphFrequency(V::graphUnit(37.0))-37.0)>1e-7)
       return 1;
    std::cout<<"PROOF 1 PASS: 20-20000 Hz axis calibrated\n";
    // PROOF 2: both children define ONE region, even if their handles cross.
    Engine engine(48000.0);
    if(engine.get(0).hz!=20.0||engine.get(1).hz!=60.0)return 2;
    auto a=engine.get(0);a.hz=4.0;engine.config(0,a);
    if(engine.get(0).hz!=20.0)return 2;
    std::array<V::Node,V::kNodes> nodes{};
    for(auto& n:nodes)n.enabled=false;
    nodes[0]={100.0,1.0,true};
    nodes[1]={40.0,10.0,true};
    auto bounds=V::region(0,nodes);
    if(bounds.lowHz!=40.0||bounds.highHz!=100.0||
       V::rootOfNode(0)!=0||V::rootOfNode(1)!=0||
       V::rootOfNode(2)!=1)return 2;
    std::cout<<"PROOF 2 PASS: two handles form one 20Hz-clamped root region\n";
    // PROOF 3: bar ownership changes ONLY when an actual boundary crosses.
    nodes[2]={1000,0.15,true};nodes[3]={2000,24.0,true};
    if(V::at(70,nodes).strongest!=0||V::at(900,nodes).strongest!=-1||
       V::at(1500,nodes).strongest!=1)return 3;
    nodes[0].hz=5000;nodes[1].hz=7000; // move both handles away
    if(V::at(70,nodes).strongest!=-1)return 3;
    nodes[2].hz=50;nodes[3].hz=100; // move second root across 70
    if(V::at(70,nodes).strongest!=1)return 3;
    // Q changes cannot spuriously recolour outside declared ranges.
    nodes[2].q=24.0;nodes[3].q=0.15;
    if(V::at(150,nodes).strongest!=-1)return 3;
    std::cout<<"PROOF 3 PASS: exact, movable boundary-based bar colours\n";
    // PROOF 4: overlapping root intervals blend evenly, no fake ownership;
    // stable peaking EQ and bandpass at the low frequency boundary.
    nodes[0]={20,1,true};nodes[1]={100,1,true};
    nodes[2]={50,1,true};nodes[3]={150,1,true};
    auto overlap=V::at(75,nodes);
    if(overlap.contributors!=2||
       std::abs(overlap.weight[0]-0.5)>1e-9||
       std::abs(overlap.weight[1]-0.5)>1e-9)return 4;
    auto n0=engine.get(0);n0.hz=20;n0.gainDb=9;
    engine.config(0,n0);engine.setSoloRoot(0);
    float l[256]{},r[256]{};
    l[0]=r[0]=0.1f;
    for(int block=0;block<100;++block){
        if(block>0)for(int k=0;k<256;++k)l[k]=r[k]=0.0f;
        engine.process(l,r,256);
        for(int k=0;k<256;++k)
            if(!std::isfinite(l[k])||!std::isfinite(r[k]))return 4;
    }
    std::cout<<"PROOF 4 PASS: overlap blending and bounded-filter numerical stability\n";
    // PROOF 5: stationary heatmap columns are fixed logarithmic Hz addresses,
    // not scrolling time pixels. All columns map monotonically across 20-20k.
    constexpr int columns=256;
    const double first=V::heatmapFrequencyAt(0,columns);
    const double second=V::heatmapFrequencyAt(1,columns);
    const double last=V::heatmapFrequencyAt(columns-1,columns);
    if(!(first>=20.0&&second>first&&last<20000.0)||
       std::abs(second/first-std::pow(1000.0,1.0/columns))>1e-9||
       std::abs(V::graphUnit(V::heatmapFrequencyAt(127,columns))-127.5/columns)>1e-12||
       V::heatmapFrequencyAt(-5,columns)!=first||
       V::heatmapFrequencyAt(400,columns)!=last)return 5;
    std::cout<<"PROOF 5 PASS: immovable log-frequency heat columns\n";
    // PROOF 6: silence never draws false energy; actual measured intensity
    // rises monotonically. No presentation tilt or time history is involved.
    const float silent=V::heatmapStrength(-144.0f);
    const float floor=V::heatmapStrength(-60.0f);
    const float low=V::heatmapStrength(-40.0f);
    const float loud=V::heatmapStrength(-12.0f);
    const float full=V::heatmapStrength(0.0f);
    if(silent!=0.f||floor!=0.f||!(low>0.f&&loud>low&&full>loud)||
       V::heatmapStrength(12.f)!=1.f||
       V::heatmapStrength(std::numeric_limits<float>::quiet_NaN())!=0.f)return 6;
    std::cout<<"PROOF 6 PASS: metered heat intensity and silence gating\n";
    // PROOF 7: glow is bounded within its stationary bottom band.
    if(V::heatmapVerticalWeight(-1.f)!=0.f||
       V::heatmapVerticalWeight(0.f)!=0.f||
       std::abs(V::heatmapVerticalWeight(.5f)-1.f)>1e-6f||
       V::heatmapVerticalWeight(1.f)!=0.f||
       V::heatmapVerticalWeight(5.f)!=0.f)return 7;
    std::cout<<"PROOF 7 PASS: non-scrolling bottom heat strip\n";
    return 0;
}

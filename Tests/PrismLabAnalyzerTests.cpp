#include "../Source/PrismLabAnalyzer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>
namespace {
using prism::analysis::LabBands;
constexpr int n=4096;
constexpr double sr=48000.0;
std::array<float,n/2+1> fakeBins(float amplitude,int index) {
    std::array<float,n/2+1> x{};
    x[static_cast<std::size_t>(index)]=amplitude*(n-1)*0.25f;
    return x;
}
int fail(int which,const char* message) {
    std::cerr<<"PRISM LAB PROOF "<<which<<" FAILED: "<<message<<"\n";
    return which;
}
}
int main() {
    // Proof 1: A bin-centred 0.5 peak sinusoid corresponds to -6.0206 dBFS.
    {
        LabBands lab;
        const int bin=85;
        const auto x=fakeBins(0.5f,bin);
        lab.processMagnitudes(x.data(),(int)x.size(),n,sr);
        const float hz=static_cast<float>(bin*sr/n);
        const auto& b=lab.frame()[(size_t)LabBands::indexForFrequency(hz)];
        if(std::abs(b.db+6.0206f)>0.08f || std::abs(b.binHz-hz)>0.1f)
            return fail(1,"Hann amplitude / peak Hz calibration");
        std::cout<<"PROOF 1 PASS: bin-centered amplitude and bin Hz calibrated\n";
    }
    // Proof 2: 256 logarithmic bars monotonic over 20..20k at edges.
    {
        if(LabBands::indexForFrequency(20.0f)!=0
          ||LabBands::indexForFrequency(20000.0f)!=255)
            return fail(2,"frequency range edges");
        for(int i=1;i<LabBands::count;++i){
            if(!(LabBands::centreFrequency(i)>LabBands::centreFrequency(i-1))
               || LabBands::indexForFrequency(LabBands::centreFrequency(i))!=i)
                return fail(2,"bin mapping not monotonic/invertible");
        }
        std::cout<<"PROOF 2 PASS: calibrated 256-band frequency mapping\n";
    }
    // Proof 3: digital silence never creates non-finite / positive readouts.
    {
        LabBands lab;
        std::array<float,n/2+1> zeros{};
        lab.processMagnitudes(zeros.data(),(int)zeros.size(),n,sr);
        for(const auto& b:lab.frame())
            if(!std::isfinite(b.db)||b.db!=LabBands::floorDb||
               b.peakDb!=LabBands::floorDb)
                return fail(3,"silence floor or non-finite amplitude");
        std::cout<<"PROOF 3 PASS: silent frame floor\n";
    }
    // Proof 4: meter smoothing separates measurement from display and decay.
    {
        LabBands lab;
        const int bin=85;
        auto x=fakeBins(0.5f,bin);
        std::array<float,n/2+1> zeros{};
        lab.processMagnitudes(x.data(),(int)x.size(),n,sr);
        const int idx=LabBands::indexForFrequency(static_cast<float>(bin*sr/n));
        const float first=lab.frame()[(size_t)idx].db;
        lab.processMagnitudes(zeros.data(),(int)zeros.size(),n,sr);
        const float following=lab.frame()[(size_t)idx].db;
        const float hold=lab.frame()[(size_t)idx].peakDb;
        if(!(following<first&&following>LabBands::floorDb && hold>following))
            return fail(4,"release and peak-hold behavior");
        std::cout<<"PROOF 4 PASS: independent release and peak hold\n";
    }
    std::cout<<"PRISM LAB -> ROOTS FOUR ANALYZER PROOFS PASSED\n";
    return 0;
}

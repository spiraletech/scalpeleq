#include "../Source/SurgeryAnalysis.h"
#include <array>
#include <cmath>
int main(){
    std::array<float,256> bins{};bins.fill(-144.0f);
    auto a=surgery::summarizeSpectrum(bins);
    if(a.signalPresent)return 1;
    bins[110]=-15.0f;
    a=surgery::summarizeSpectrum(bins);
    if(!a.signalPresent||a.loudestHz<100||a.loudestDb!=-15)return 2;
    if(std::abs(a.approximateCentroidHz-a.loudestHz)>1.0f)return 3;
    return 0;
}

#include "../Source/ScalpelDbScale.h"
#include "../Source/SurgeryResponse.h"
#include <cmath>
#include <iostream>

int main(){
    using namespace scalpel::dbaxis;
    constexpr std::array<float,11> expectedNativeY{183,200,217,234,251,268,285,302,319,353,387};
    for(size_t i=0;i<labels.size();++i){
        if(std::abs(toY((float)labels[i],183,204)-expectedNativeY[i])>.0001f)return 1;
        if(i&&labels[i]>=labels[i-1])return 2;
    }
    // Endpoint, midpoint and all intermediate levels at supported editor and
    // fractional DPI scales: a fixed dB interval must stay proportional.
    for(float scale:{2.f/3.f,1.f,4.f/3.f,1.25f,1.5f,2.f}){
        float previous=183*scale-1;
        for(int tenth=0;tenth<=600;++tenth){
            const float db=-tenth*.1f;
            const float expected=(183.f+tenth*.34f)*scale;
            const float y=toY(db,183*scale,204*scale);
            if(std::abs(y-expected)>.0002f||y<=previous)return 3;
            previous=y;
        }
    }
    // Known amplitude ratios, not 10*log10 power values. Existing FFT tests
    // separately verify actual analyzer calibration at four sample rates.
    constexpr std::array<float,7> amplitudes{1,.5f,.25f,.125f,.1f,.01f,.001f};
    constexpr std::array<float,7> measuredDb{0,-6.0206f,-12.0412f,-18.0618f,-20,-40,-60};
    for(size_t i=0;i<amplitudes.size();++i){
        const float display=surgery::displayDb(20.f*std::log10(amplitudes[i]),1000,0,rangeDb);
        if(std::abs(display-measuredDb[i])>.0001f)return 4;
        if(std::abs(toY(display,183,204)-(183.f-measuredDb[i]*3.4f))>.0002f)return 5;
    }
    // Display tilt and floor remain authoritative; PEAK(dBFS) is separate.
    if(surgery::displayDb(-30,2000,3,rangeDb)!=-27)return 6;
    if(toY(surgery::displayDb(-144,1000,0,rangeDb),183,204)!=387)return 7;
    if(toY(surgery::displayDb(6,1000,0,rangeDb),183,204)!=183)return 8;
    std::cout<<"dB axis: all 11 reference rows, -30 midpoint, -35 uniqueness, 6 scales, amplitude calibration, tilt and floor passed\n";
}

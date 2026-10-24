#pragma once
// Read-only post-EQ spectral snapshot; never a claim of actual stem separation.
// Log-band-weighted centroid is approximate, not a standardized LUFS metric.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
namespace surgery {
struct SpectralSnapshot {
    float loudestHz=0.0f;
    float loudestDb=-144.0f;
    float approximateCentroidHz=0.0f;
    float averageEnergyDb=-144.0f;
    bool signalPresent=false;
};
template<std::size_t N> SpectralSnapshot summarizeSpectrum(
    const std::array<float,N>& db) noexcept {
    SpectralSnapshot s;
    double energy=0.0,weighted=0.0;
    for(std::size_t i=0;i<N;++i){
        const float v=db[i];
        if(!std::isfinite(v))continue;
        const float hz=20.0f*std::pow(1000.0f,
                   ((float)i+0.5f)/(float)N);
        if(v>s.loudestDb){s.loudestDb=v;s.loudestHz=hz;}
        if(v<=-125.0f)continue;
        const double e=std::pow(10.0,std::min(0.0f,v)/10.0);
        energy+=e;weighted+=e*hz;
    }
    s.signalPresent=energy>1.e-9;
    if(s.signalPresent){
        s.approximateCentroidHz=(float)(weighted/energy);
        s.averageEnergyDb=(float)(10.0*std::log10(energy/(double)N));
    }
    return s;
}
}

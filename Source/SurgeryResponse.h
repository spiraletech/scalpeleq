#pragma once
// SURGERY EQ: analytic magnitude of the same RBJ peaking biquad used by CARVE.
// Mathematical response overlay only; NOT a measured pre/post audio difference.
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include "ScalpelBandGain.h"
namespace surgery {
// Display-only prepared response: compute coefficients once per changed
// CARVE, then evaluate the unchanged 2049-point contour with cached probes.
class PreparedCarveResponse {
public:
    PreparedCarveResponse(double sampleRate,double centerHz,double q,double cutDb,bool signedGain=false) noexcept {
        const double sr=std::max(8000.0,sampleRate);
        const double fc=std::clamp(centerHz,20.0,sr*.45);
        const double gain=signedGain?std::clamp(cutDb,-40.0,40.0):-std::clamp(cutDb,0.0,36.0);
        const double w=2.0*std::numbers::pi*fc/sr;
        response_=scalpel::bands::GainResponse(
            {fc,std::cos(w),std::sin(w)/(2.0*std::clamp(q,.3,96.0))},gain);
    }
    std::complex<double> transfer(std::complex<double> z1) const noexcept {
        return response_.transfer(z1);
    }
    double magnitudeDb(std::complex<double> z1) const noexcept {
        const double ratio=std::abs(transfer(z1));
        return std::clamp(20.0*std::log10(std::max(1.e-12,ratio)),-40.0,40.0);
    }
private:
    scalpel::bands::GainResponse response_;
};
inline double carveMagnitudeDb(double sampleRate,double centerHz,double q,
                               double cutDb,double probeHz) noexcept {
    const double sr=std::max(8000.0,sampleRate);
    const double hz=std::clamp(probeHz,10.0,sr*0.49);
    const double phase=-2.0*std::numbers::pi*hz/sr;
    return std::clamp(PreparedCarveResponse(sr,centerHz,q,cutDb).magnitudeDb(std::polar(1.0,phase)),
                      -36.0,3.0);
}
inline float displayDb(float measuredDb,float frequencyHz,float tiltPerOctave,
                       float rangeDb) noexcept {
    const float pivot=tiltPerOctave*std::log2(std::max(20.0f,frequencyHz)/1000.0f);
    return std::clamp(measuredDb+pivot,-rangeDb,0.0f);
}
} // namespace surgery

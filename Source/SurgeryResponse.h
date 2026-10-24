#pragma once
// SURGERY EQ: analytic magnitude of the same RBJ peaking biquad used by CARVE.
// Mathematical response overlay only; NOT a measured pre/post audio difference.
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
namespace surgery {
inline double carveMagnitudeDb(double sampleRate,double centerHz,double q,
                               double cutDb,double probeHz) noexcept {
    const double sr=std::max(8000.0,sampleRate);
    const double fc=std::clamp(centerHz,20.0,sr*0.45);
    const double hz=std::clamp(probeHz,10.0,sr*0.49);
    const double qq=std::clamp(q,0.3,96.0);
    const double gain=-std::clamp(cutDb,0.0,36.0);
    if(gain==0.0)return 0.0;
    const double w=2.0*std::numbers::pi*fc/sr;
    const double A=std::pow(10.0,gain/40.0);
    const double alpha=std::sin(w)/(2.0*qq);
    const double c=std::cos(w),norm=1.0/(1.0+alpha/A);
    const double b0=(1.0+alpha*A)*norm;
    const double b1=-2.0*c*norm;
    const double b2=(1.0-alpha*A)*norm;
    const double a1=-2.0*c*norm;
    const double a2=(1.0-alpha/A)*norm;
    const double phase=-2.0*std::numbers::pi*hz/sr;
    const std::complex<double> z1=std::polar(1.0,phase);
    const auto z2=z1*z1;
    const auto numerator=b0+b1*z1+b2*z2;
    const auto denominator=1.0+a1*z1+a2*z2;
    const double ratio=std::abs(numerator)/std::max(1.e-12,std::abs(denominator));
    return std::clamp(20.0*std::log10(std::max(1.e-12,ratio)),-36.0,3.0);
}
inline float displayDb(float measuredDb,float frequencyHz,float tiltPerOctave,
                       float rangeDb) noexcept {
    const float pivot=tiltPerOctave*std::log2(std::max(20.0f,frequencyHz)/1000.0f);
    return std::clamp(measuredDb+pivot,-rangeDb,0.0f);
}
} // namespace surgery

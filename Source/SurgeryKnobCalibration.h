#pragma once
#include <algorithm>
#include <cmath>
// Local control coordinates only. Host parameter ranges/IDs remain unchanged.
namespace surgery::calibration {
inline double position(double p) noexcept {return std::isfinite(p)?std::clamp(p,0.0,1.0):0.0;}
inline double thresholdFromPosition(double p) noexcept {return -60.0*position(p);}
inline double thresholdPosition(double db) noexcept {return position(-db/60.0);}
inline double qFromPosition(double t) noexcept {return .3*std::pow(96.0/.3,position(t));}
inline double qPosition(double q) noexcept {
    if(!std::isfinite(q))return 0;
    return position(std::log(std::clamp(q,.3,96.0)/.3)/std::log(96.0/.3));
}
}

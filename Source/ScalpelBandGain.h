#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace scalpel::bands {
struct GainBand {
    double centerHz=1000.0;
    double cosine=0.0;
    double bandwidth=0.0;

    static GainBand fromEdges(double rate,double low,double high) noexcept {
        const double limit=std::min(20000.0,rate*0.49);
        const auto warp=[rate,limit](double hz){
            return std::tan(std::numbers::pi*std::clamp(hz,20.0,limit)/rate);
        };
        const double lower=warp(low),upper=warp(high);
        const double center=std::sqrt(lower*upper);
        const double w=2.0*std::atan(center);
        return {rate*w/(2.0*std::numbers::pi),std::cos(w),
            std::sin(w)*std::max(0.0,upper-lower)/(2.0*center)};
    }
};

struct GainResponse {
    double b0=1.0,b1=0.0,b2=0.0,a1=0.0,a2=0.0;

    GainResponse() noexcept = default;
    GainResponse(GainBand band,double db) noexcept {
        if(db==0.0||band.bandwidth==0.0)return;
        const double a=std::pow(10.0,std::clamp(db,-60.0,60.0)/40.0);
        const double norm=1.0/(1.0+band.bandwidth/a);
        b0=(1.0+band.bandwidth*a)*norm;
        b1=a1=-2.0*band.cosine*norm;
        b2=(1.0-band.bandwidth*a)*norm;
        a2=(1.0-band.bandwidth/a)*norm;
    }
    std::complex<double> transfer(std::complex<double> z) const noexcept {
        const auto zz=z*z;
        return (b0+b1*z+b2*zz)/(1.0+a1*z+a2*zz);
    }
};
}

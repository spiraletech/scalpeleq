#pragma once

#include <algorithm>

namespace scalpel::dynamics {
inline constexpr double referenceThresholdDb=-18.0;
inline constexpr double threshold(double trimDb) noexcept {
    return referenceThresholdDb+std::clamp(trimDb,-9.0,9.0);
}

struct Gain {
    double reductionDb=0.0;
    double liftDb=0.0;
};

inline Gain transfer(double detectorDb,double trimDb,double slope,bool optical) noexcept {
    const double trim=std::clamp(trimDb,-9.0,9.0);
    const double knee=optical?6.0:3.0;
    const double over=detectorDb-threshold(trim);
    const double kneeDistance=std::max(0.0,over+knee*0.5);
    const double reduction=over>=knee*0.5?over:kneeDistance*kneeDistance/(2*knee);

    // The positive side expands the lift range for quiet material. Gate
    // below the signal floor and close the lift before the upper knee.
    const double normalLift=optical?6.0:3.0;
    const double maximum=trim>=0.0?normalLift+(9.0-normalLift)*trim/9.0
                                   :normalLift*(1.0+trim/9.0);
    const double lowerKnee=-26.0+std::min(0.0,trim)+std::max(0.0,trim)*0.55;
    const double floor=std::clamp((detectorDb+80.0)/18.0,0.0,1.0);
    const double quiet=std::clamp((lowerKnee-detectorDb)*(optical?0.45:0.30),0.0,maximum);
    return {slope*reduction,quiet*floor*floor*(3.0-2.0*floor)};
}
}

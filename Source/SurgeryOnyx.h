#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace surgery {
// Scalpel COLOR: five independent ROOT-local processors. The historical
// Onyx class/parameter IDs are retained only for preset/automation migration.
// Dry audio is not delayed; nonlinear residuals use first-order ADAA.
class Onyx {
public:
    void prepare(double sampleRate) noexcept {
        sr=std::max(8000.0,sampleRate);
        airA=alpha(std::min(4200.0,sr*.12));
        bandA=alpha(std::min(6500.0,sr*.12));
        presenceA=alpha(std::min(1600.0,sr*.08));
        dcA=alpha(8.0);
        attack=1.0-std::exp(-1.0/(.0015*sr));
        release=1.0-std::exp(-1.0/(.055*sr));
        transientFastA=1.0-std::exp(-1.0/(.003*sr));
        transientSlowA=1.0-std::exp(-1.0/(.075*sr));
        *this=resetState(*this);
    }
    void setRootBand(double low,double high) noexcept {
        detailA=alpha(std::clamp(std::sqrt(std::max(1.0,low*high))*.3,30.0,std::min(4200.0,sr*.12)));
        exciteA=alpha(std::clamp(high*1.5,120.0,std::min(6500.0,sr*.12)));
    }
    // Five controls, ordered exactly as displayed: DYNAMIC, SPARKLE, SHINE,
    // SMOOTHENING, VINTAGE TAPE. Each amount is normalized to [0,1].
    double sampleColor(double x,const std::array<double,5>& amounts) noexcept {
        if(!std::isfinite(x))x=0;
        const double dy=law(amounts[0]),sp=law(amounts[1]),sh=law(amounts[2]);
        const double sm=law(amounts[3]),tp=law(amounts[4]);
        // Transient contrast operates from its OWN dual RMS envelopes. Its
        // control never changes the coefficients/amounts of the other colors.
        // 3 ms/75 ms windows, peak boost <= +3 dB, sustain trim >= -2 dB.
        const double power=x*x;
        transientFastPower+=transientFastA*(power-transientFastPower);
        transientSlowPower+=transientSlowA*(power-transientSlowPower);
        const double fastRms=std::sqrt(std::max(0.0,transientFastPower));
        const double slowRms=std::sqrt(std::max(0.0,transientSlowPower));
        const double contrast=std::clamp((fastRms-slowRms)/(slowRms+0.015),-1.0,1.0);
        const double contrastDb=dy*(contrast>=0.0?3.0*contrast:2.0*contrast);
        const double dynamicsGain=dy>1e-9?std::pow(10.0,contrastDb/20.0):1.0;
        air+=airA*(x-air);
        presence+=presenceA*(x-presence);
        // Two low-pass poles attenuate the exciter's input near Nyquist.
        detail+=detailA*(x-detail);
        band1+=exciteA*((x-detail)-band1);
        band2+=exciteA*(band1-band2);
        const double edge=std::abs(x-air);
        fast+=attack*(edge-fast);
        slow+=(edge>slow?attack*.08:release)*(edge-slow);
        const double transient=std::clamp((fast-slow)/(slow+.015),0.0,1.0);
        double sparkle=0,tape=0;
        if(sp>1e-9)sparkle=-residual(band2,previousSparkle,2.5);
        previousSparkle=band2;
        // Always-running tape input/DC states prevent switched filter history.
        tape1+=bandA*(x-tape1);tape2+=bandA*(tape1-tape2);
        if(tp>1e-9)tape=residual(tape2,previousTape,1.7);
        previousTape=tape2;
        sparkleDC+=dcA*(sparkle-sparkleDC);
        tapeDC+=dcA*(tape-tapeDC);
        // A bounded linear shelf, no nonlinear transfer in SHINE.
        double y=x*dynamicsGain+sh*.33*(x-presence);
        // Dynamic, frequency-weighted edge reduction (max 35% HF subtraction).
        // Use the smoothed envelope, never instantaneous rectified samples:
        // sample-wise gain modulation would create unnecessary HF aliases.
        const double activity=slow/(slow+.06);
        y-=sm*.28*activity*(.65+.35*transient)*(x-air);
        // Sparkle boosts transient harmonics, rather than duplicating a shelf.
        const double nyquistGuard=std::min(1.0,sr/48000.0);
        y+=sp*.75*nyquistGuard*(.35+.65*transient)*(sparkle-sparkleDC);
        // Low-drive tape residue with restrained HF rounding; unity small-signal gain.
        y+=tp*(.60*(tape-tapeDC)-.045*(x-air));
        return std::isfinite(y)?y:0.0;
    }
    // Compatibility for historical direct Onyx callers and smoke tests.
    double sample(double x,const std::array<double,4>& oldAmounts) noexcept {
        return sampleColor(x,std::array<double,5>{0.0,oldAmounts[0],oldAmounts[1],oldAmounts[2],oldAmounts[3]});
    }
    static double law(double amount) noexcept {
        const double v=std::isfinite(amount)?std::clamp(amount,0.0,1.0):0;
        return v*(.45+.55*v); // fine low-end travel, 20% => 11.2% internal depth
    }
private:
    static Onyx resetState(const Onyx& coeff) noexcept {
        Onyx result;
        result.sr=coeff.sr;result.airA=coeff.airA;result.bandA=coeff.bandA;
        result.presenceA=coeff.presenceA;result.dcA=coeff.dcA;
        result.attack=coeff.attack;result.release=coeff.release;
        result.transientFastA=coeff.transientFastA;
        result.transientSlowA=coeff.transientSlowA;
        result.detailA=coeff.airA;result.exciteA=coeff.bandA;
        return result;
    }
    double alpha(double hz) const noexcept {return 1.0-std::exp(-2*std::numbers::pi*hz/sr);}
    static double logCosh(double v) noexcept {
        const double a=std::abs(v);
        return a+std::log1p(std::exp(-2*a))-std::numbers::ln2;
    }
    // Integral of tanh(d*x)/d - x. Subtract only nonlinear residual,
    // avoiding ADAA's half-sample delay on the original signal.
    static double residual(double x,double previous,double d) noexcept {
        const double delta=x-previous;
        if(std::abs(delta)<1e-5){
            const double mid=.5*(x+previous);
            return std::tanh(d*mid)/d-mid;
        }
        return (logCosh(d*x)-logCosh(d*previous))/(d*d*delta)-.5*(x+previous);
    }
    double sr=48000,airA=.4,bandA=.5,presenceA=.2,dcA=.001,attack=.01,release=.001;
    double transientFastA=.007,transientSlowA=.00028;
    double transientFastPower=0,transientSlowPower=0;
    double detailA=.4,exciteA=.5,detail=0;
    double air=0,presence=0,band1=0,band2=0,fast=0,slow=0;
    double tape1=0,tape2=0,previousTape=0,previousSparkle=0,tapeDC=0,sparkleDC=0;
};
}

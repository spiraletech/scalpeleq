#pragma once
#include <algorithm>
#include <cmath>

// ET-MOTION-016: presentation only. No audio parameters or selection authority.
namespace scalpel::omnis {
inline constexpr double pi=3.14159265358979323846;
// ET-MOTION-017: the 105% material enlargement is applied only here/rotary().
struct RotaryGeometry {
    static constexpr float enlargement=1.05f;
    static constexpr float outer=59.f*enlargement,inner=49.f*enlargement;
    static constexpr float thickness=3.5f*enlargement,pointerInner=39.f*enlargement,
        pointerOuter=60.f*enlargement,pointerThickness=1.5f*enlargement;
    static constexpr int segments=33;
};

inline double unit(double x) noexcept {return std::isfinite(x)?std::clamp(x,0.0,1.0):0.0;}
inline double smooth(double x) noexcept {x=unit(x);return x*x*(3.-2.*x);}
inline double angle(double u) noexcept {return (135.+unit(u)*270.)*pi/180.;}
// Segment zero represents the minimum endpoint. Minimum is unlit; maximum
// includes the 33rd segment. Nearest endpoint rounding is deterministic.
inline int litSegments(double u) noexcept {u=unit(u);return u==0?0:std::clamp(1+(int)std::floor(u*32.+.5),1,33);}
struct Trajectory {
    double from=1,to=1,started=0,duration=260;
    double sample(double now) const noexcept {return from+(to-from)*smooth((now-started)/duration);}
    bool active(double now) const noexcept {return from!=to&&now<started+duration;}
    void target(double v,double now,bool reduced=false) noexcept {
        v=unit(v);if(reduced){from=to=v;started=now;return;}
        if(v==to)return;
        from=sample(now);to=v;started=now;
    }
    void settle() noexcept {from=to;}
};
struct CrtExtent {double width,height,phosphor;};
inline CrtExtent crt(double visible) noexcept {
    visible=unit(visible);
    // Reversed shutdown: at 65% elapsed, only the horizontal line remains.
    const double line=smooth(.35); // 65% of elapsed shutdown with smoothstep travel.
    return {visible<line?visible/line:1.,visible<line?.004:smooth((visible-line)/(1.-line)),
            visible>0&&visible<1?.60*(1.-visible):0.};
}
struct Shear {
    double started=-1000; int targetMode=0;
    bool running=false;
    static constexpr double collapse=260,exchange=105,expansion=260,total=625;
    double elapsed(double now) const noexcept {return std::max(0.,now-started);}
    double extent(double now) const noexcept {
        if(!running)return 1.;const auto t=elapsed(now);
        if(t<collapse)return 1.-smooth(t/collapse);
        if(t<collapse+exchange)return 0.;
        return smooth((t-collapse-exchange)/expansion);
    }
    bool incoming(double now) const noexcept {return elapsed(now)>=collapse+exchange;}
    // During interruption the caller freezes the currently presented content;
    // a single trajectory restarts from that image, never a queued callback.
    void request(int mode,double now,bool reduced) noexcept {targetMode=mode;started=now;running=!reduced;}
    void tick(double now,bool reduced) noexcept {if(reduced||elapsed(now)>=total)running=false;}
};
}

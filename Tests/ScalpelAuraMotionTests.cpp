#include "../Source/ScalpelAuraMotion.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main(){
    using namespace scalpel::aura;
    for(int bits=0;bits<16;++bits){
        const bool enabled=bits&1,down=bits&2,selected=bits&4,over=bits&8;
        const auto expected=!enabled?State::disabled:down?State::pressed:selected?State::selected:over?State::hover:State::idle;
        assert(state(enabled,down,selected,over)==expected);
    }
    Travel t;
    assert(t.retarget(referenceTravel,1000));
    constexpr double ms[]{0,40,80,120,160};
    constexpr float distances[]{0,.2734375f,.875f,1.4765625f,1.75f};
    for(int i=0;i<5;++i)assert(std::abs(t.sample(1000+ms[i])-distances[i])<.000001f);
    // Same-target polls and repeated presses cannot postpone completion.
    assert(!t.retarget(referenceTravel,1080));
    assert(t.sample(1160)==referenceTravel&&!t.active(1160));
    const float before=t.sample(1200);assert(t.retarget(0,1200));
    assert(t.sample(1200)==before);
    const float midway=t.sample(1261.25);assert(t.retarget(referenceTravel,1261.25));
    assert(t.sample(1261.25)==midway);
    for(int i=0;i<5000;++i){
        const double now=1300+i*3.14159;
        const float previous=t.sample(now);
        t.retarget(i%2?0.f:referenceTravel,now);
        assert(t.sample(now)==previous);
        assert(previous>=0&&previous<=referenceTravel);
    }
    t.retarget(0,20000);assert(t.sample(20160)==0&&!t.active(20160));
    const auto f=fit(143,45);
    assert(std::abs(f.scale-143.f/135.f)<.000001f);
    assert(std::abs(f.textureX+4*f.scale)<.000001f);
    assert(std::abs(f.textureY+4*f.scale-.25555556f)<.00001f);
    assert(std::abs((45.4f*surfaceScale(referenceTravel)+surfaceOffset(referenceTravel))-45.4f)<.00001f);
    assert(std::abs(referenceTravel*f.scale-1.8537037f)<.000001f);
    std::cout<<"PASS: 16 state combinations; 160ms samples; 5000 continuous retargets; unchanged-target completion; stationary bottom; material-only fit\n";
}

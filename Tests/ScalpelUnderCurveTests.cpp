#include "../Source/RootsSpectrumColours.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using prism::roots::visual::underCurveAlpha;
    for(float y=0.f;y<=1.f;y+=.005f) {
        assert(underCurveAlpha(-144.f,-12.f,y)==0.f);
        assert(underCurveAlpha(-60.f,-60.f,y)==0.f);
        assert(underCurveAlpha(-12.f,-12.f,y)==0.f || y>=.2f);
    }
    assert(underCurveAlpha(-12.f,-12.f,.1f)==0.f);
    assert(underCurveAlpha(-12.f,-12.f,.3f)>0.f);
    assert(underCurveAlpha(-12.f,-12.f,.3f)>underCurveAlpha(-12.f,-12.f,.9f));
    assert(underCurveAlpha(-12.f,-12.f,1.f)==0.f);
    assert(underCurveAlpha(-12.f,-12.f,NAN)==0.f);
    assert(underCurveAlpha(-12.f,NAN,.5f)==0.f);
    std::cout<<"X-RAY under-curve mask: silence, contour, falloff, floor PASS\n";
}

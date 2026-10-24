#include "../Source/SurgeryPrecision.h"
#include <cmath>
#include <iostream>
int main(){
    using namespace surgery::precision;
    if(rootBounds(600.0f,250.0f).lo!=250.0f)return 1;
    if(boundedFrequency(100.0f,250.0f,600.0f)!=250.0f)return 2;
    if(boundedFrequency(4000.0f,250.0f,600.0f)!=600.0f)return 3;
    if(scrubFrequency(400.0f,800.0f,10000.0f,true,250,600)>600)return 4;
    if(scrubFrequency(400.0f,100.0f,10000.0f,true,250,600)>=420)return 5;
    if(scrubFrequency(400.0f,100.0f,10000.0f,false,250,600)!=600)return 6;
    if(scrubWheelFrequency(250,1000,250,600)!=600.0f)return 7;
    if(adjustQ(4.0f,0.1f,true)<=4.0f)return 8;
    if(adjustQ(96.0f,10,false)!=96.0f)return 9;
    if(bandwidthOctaves(15.0f)>=bandwidthOctaves(1.0f))return 10;
    if(tightnessPercent(0.3f)!=0.0f||
       tightnessPercent(96.0f)!=100.0f)return 11;
    if(!(tightnessPercent(24.0f)>tightnessPercent(2.0f)))return 12;
    std::cout<<"ROOT frequency clamp, CTRL scrub and numerical Q proofs passed\n";
}

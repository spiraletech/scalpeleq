#include "../Source/SurgerySpectrogram.h"
#include <array>
#include <cmath>
#include <iostream>
int main(){
    surgery::SpectrogramFifo<8,4> fifo;
    std::array<float,8> a{},b{};
    a.fill(-100.0f);a[2]=-8.0f;
    if(!fifo.push(a)||!fifo.pop(b)||b[2]!=-8.0f)return 1;
    if(fifo.pop(b))return 2;
    for(int i=0;i<3;++i){a[2]=(float)i;if(!fifo.push(a))return 3;}
    if(fifo.push(a))return 4;
    for(int i=0;i<3;++i){if(!fifo.pop(b)||b[2]!=(float)i)return 5;}
    if(fifo.pop(b))return 6;
    a[2]=55.0f;
    if(!fifo.push(a)||!fifo.pop(b)||b[2]!=55.0f)return 7;
    if(surgery::heatIntensity(-144)!=0.0f)return 8;
    if(surgery::heatIntensity(0)!=1.0f)return 9;
    if(!(surgery::heatIntensity(-20)>surgery::heatIntensity(-60)))return 10;
    if(surgery::tightnessPercent(0.3f)!=0||surgery::tightnessPercent(96.0f)!=100)return 11;
    if(!(surgery::tightnessPercent(72.0f)>surgery::tightnessPercent(24.0f)))return 12;
    std::cout<<"SPSC waterfall history and ultra Q tightness proofs passed\n";
}

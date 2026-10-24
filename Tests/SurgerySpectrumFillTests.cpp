#include "../Source/SurgerySpectrumFill.h"
#include <array>
#include <cmath>
#include <iostream>
int main(){
    surgery::LiveSpectrumEnvelope<8> spectrum;
    std::array<float,8> frame{};
    frame.fill(-144.0f);
    if(spectrum.frame()[2]!=-144.0f)return 1;
    frame[2]=-12.0f;
    spectrum.update(frame,1.0/30.0,0.4);
    if(std::abs(spectrum.frame()[2]+12.0f)>0.001f)return 2;
    frame[2]=-96.0f;
    spectrum.update(frame,1.0/30.0,0.4);
    const float afterOne=spectrum.frame()[2];
    if(!(afterOne< -12.0f&&afterOne> -96.0f))return 3;
    for(int i=0;i<500;++i)spectrum.update(frame,1.0/30.0,0.4);
    if(std::abs(spectrum.frame()[2]+96.0f)>0.05f)return 4;
    frame[2]=4.0f;spectrum.update(frame,1.0/30.0,0.4);
    if(spectrum.frame()[2]!=0.0f)return 5;
    frame[2]=NAN;spectrum.update(frame,1.0/30.0,0.4);
    if(!std::isfinite(spectrum.frame()[2]))return 6;
    std::cout<<"Single-spectrum fast attack, finite input and decay proofs passed\n";
}

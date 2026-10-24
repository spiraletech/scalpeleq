#include "../Source/SurgeryResponse.h"
#include <cmath>
#include <iostream>
int main(){
    using surgery::carveMagnitudeDb;
    if(std::abs(carveMagnitudeDb(48000,1000,4,0,1000))>1.e-9)return 1;
    if(std::abs(carveMagnitudeDb(48000,1000,4,12,1000)+12.0)>0.02)return 2;
    if(std::abs(carveMagnitudeDb(48000,1000,15,12,600))>=
       std::abs(carveMagnitudeDb(48000,1000,1,12,600)))return 3;
    if(std::abs(carveMagnitudeDb(48000,1000,4,12,15000))>0.15)return 4;
    for(double f=20;f<=20000;f*=1.17)
        if(!std::isfinite(carveMagnitudeDb(44100,700,16,36,f)))return 5;
    if(surgery::displayDb(-30,1000,4.5f,96)!=-30)return 6;
    if(std::abs(carveMagnitudeDb(48000,1000,96,24,1000)+24.0)>0.035)return 7;
    if(!(std::abs(carveMagnitudeDb(48000,1000,96,24,995))<
          std::abs(carveMagnitudeDb(48000,1000,24,24,995))))return 8;
    std::cout<<"Measured/display separation and exact biquad response proofs passed\n";return 0;
}

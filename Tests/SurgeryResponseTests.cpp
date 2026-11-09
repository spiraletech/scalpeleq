#include "../Source/SurgeryResponse.h"
#include <cmath>
#include <iostream>
#include <array>
#include <chrono>
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
    // Compare the optimized display curve with the established analytical
    // response, including high Q, low centres, and Nyquist-clamped probes.
    for(double sr:{8000.,44100.,48000.,96000.})
        for(double fc:{20.,35.,1000.,18000.})
            for(double q:{.3,2.,24.,96.})
                for(double depth:{0.,.01,12.,36.}){
                    const surgery::PreparedCarveResponse prepared(sr,fc,q,depth);
                    for(int i=0;i<=2048;++i){
                        const double f=20.*std::pow(1000.,i/2048.);
                        const auto z=std::polar(1.,-2.*std::numbers::pi*std::clamp(f,10.,sr*.49)/sr);
                        if(std::abs(prepared.magnitudeDb(z)-carveMagnitudeDb(sr,fc,q,depth,f))>1.e-9)return 9;
                    }
                }
    std::array<double,2049> frequencies;
    std::array<std::complex<double>,2049> probes;
    for(size_t i=0;i<probes.size();++i){
        frequencies[i]=20.*std::pow(1000.,i/2048.);
        probes[i]=std::polar(1.,-2.*std::numbers::pi*frequencies[i]/48000.);
    }
    const auto benchmark=[&](bool cached){
        double sum=0;
        const auto begin=std::chrono::steady_clock::now();
        for(int frame=0;frame<200;++frame){
            const double fc=30.+frame*7.13;
            const surgery::PreparedCarveResponse prepared(48000,fc,96,24);
            for(size_t i=0;i<probes.size();++i)
                sum+=cached?prepared.magnitudeDb(probes[i]):carveMagnitudeDb(48000,fc,96,24,frequencies[i]);
        }
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        std::cout<<(cached?"Prepared":"Original")<<" response: "<<ms<<" ms, checksum "<<sum<<"\n";
        return std::pair{ms,sum};
    };
    const auto original=benchmark(false),prepared=benchmark(true);
    if(std::abs(original.second-prepared.second)>1.e-6)return 10;
    std::cout<<"Response evaluation speedup: "<<original.first/prepared.first<<"x\n";
    std::cout<<"Measured/display separation and exact biquad response proofs passed\n";return 0;
}

#include "../Source/ScalpelOmnisModel.h"
#include <cstdlib>
#include <iostream>

int main(){
    using namespace scalpel::omnis;
    const auto check=[](bool b){if(!b){std::cerr<<"OMNIS model regression\n";std::exit(1);}};
    check(litSegments(0)==0&&litSegments(.5)==17&&litSegments(1)==33);
    check(std::abs(std::cos(angle(0))+std::sqrt(.5))<1e-12);
    check(std::abs(std::sin(angle(.5))+1)<1e-12);
    check(std::abs(std::sin(angle(1))-std::sqrt(.5))<1e-12);
    for(int i=1;i<=32;++i)check(litSegments(i/32.)==i+1);
    Trajectory power;power.target(0,100);
    check(power.sample(100)==1&&power.sample(360)==0);
    for(int i=0;i<5000;++i){const double now=100+i*3.1;const double before=power.sample(now);
        power.target(i%2,now);check(std::abs(power.sample(now)-before)<1e-12);check(power.sample(now)>=0&&power.sample(now)<=1);}
    power.target(1,20000,true);check(!power.active(20000)&&power.sample(20000)==1);
    check(crt(0).width==0&&crt(1).width==1&&crt(1).height==1);
    check(crt(smooth(.35)).width==1&&crt(smooth(.35)).height==0);
    Shear s;s.request(1,0,false);
    check(s.extent(0)==1&&s.extent(260)==0&&s.extent(365)==0&&s.extent(625)==1);
    for(int i=0;i<5000;++i){s.request(i%3,i*2.,false);s.tick(i*2.+1,false);check(s.targetMode==i%3);}
    s.tick(11000,false);check(!s.running&&s.extent(11000)==1);
    s.request(3,12000,true);check(!s.running&&s.targetMode==3);
    std::cout<<"PASS 33 segment endpoints, clockwise geometry, 5000 CRT reversals, 5000 latest-mode requests, reduced motion\n";
}

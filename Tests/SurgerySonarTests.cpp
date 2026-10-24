#include "../Source/SurgerySonar.h"
#include <array>
#include <cmath>
#include <iostream>
int main(){
    surgery::SonarField<8> s;
    std::array<float,8> db{};db.fill(-144.0f);
    s.update(db,1.0/30.0,1.5);
    if(s.heat(3)!=0.0f)return 1;
    db[3]=-12.0f;s.update(db,1.0/30.0,1.5);
    if(s.heat(3)<0.8f||s.transient(3)<0.8f)return 2;
    const float first=s.heat(3);
    db.fill(-144.0f);
    s.update(db,1.0/30.0,1.5);
    if(!(s.heat(3)<first && s.heat(3)>0.7f))return 3;
    s.reset();
    if(s.heat(3)!=0.0f||s.transient(3)!=0.0f)return 4;
    std::cout<<"SONAR four proof groups passed\n";return 0;
}
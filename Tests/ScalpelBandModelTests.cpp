#include "../Source/ScalpelBandModel.h"
#include <iostream>
using namespace scalpel::bands;
int main(){
    Model m;int a=m.add(100),b=m.add(300),c=m.add(1200,Domain::mid),d=m.add(1200,Domain::side);
    if(a!=0||b!=1||c!=2||d!=3||!m.valid())return 1;
    m.bands[a].right=150;m.bands[b].left=150;
    if(!m.connect(a,Anchor::right,b,Anchor::left)||!m.valid())return 2;
    const double aw=std::log2(m.bands[a].right/m.bands[a].left),bw=std::log2(m.bands[b].right/m.bands[b].left);
    m.moveGroup(a,1.0);
    if(std::abs(std::log2(m.bands[a].right/m.bands[a].left)-aw)>1e-9||std::abs(std::log2(m.bands[b].right/m.bands[b].left)-bw)>1e-9||std::abs(m.bands[a].right-m.bands[b].left)>.01)return 3;
    if(!m.moveAnchor(a,Anchor::right,400)||std::abs(m.bands[b].left-400)>.01)return 4;
    const double beforeLeft=m.bands[a].left,beforeShared=m.bands[a].right,beforeRight=m.bands[b].right;
    if(m.moveGroup(a,100.0)||m.bands[b].right>20000.001||
       std::abs(std::log2(m.bands[a].right/m.bands[a].left)-aw)>1e-9||
       std::abs(std::log2(m.bands[b].right/m.bands[b].left)-std::log2(beforeRight/beforeShared))>1e-9)return 8;
    if(!(m.bands[a].left>beforeLeft&&m.bands[a].right>beforeShared))return 9;
    m.unlink(a,Anchor::right);if(m.link(a,Anchor::right).band>=0||m.link(b,Anchor::left).band>=0)return 5;
    m.remove(a);if(m.bands[a].present||!m.bands[b].present||!m.valid())return 6;
    if(m.add(1200,Domain::mid)>=0)return 7; // same-domain overlap refused
    std::cout<<"Scalpel magnetic model: add/remove, cross-domain overlap, shared anchor, group move, resize, unlink PASS\n";
}

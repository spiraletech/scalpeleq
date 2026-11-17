#include "../Source/ScalpelBandModel.h"

#include <cmath>
#include <iostream>

using namespace scalpel::bands;

static bool close(double a,double b,double tolerance=0.01){
    return std::abs(a-b)<=tolerance;
}

int main(){
    // Factory/legacy invariant: every approved BAND 1-7 region exists.
    Model factory;
    for(const auto& band:factory.bands)if(!band.present)return 1;
    if(!factory.valid())return 2;

    // Delete is lifecycle-only; the slot can be recreated in the vacated gap.
    factory.remove(2);
    if(factory.bands[2].present||!factory.valid())return 3;
    const int recreated=factory.add(180.0);
    if(recreated!=2||!factory.bands[2].present||!factory.valid())return 4;

    // Independent boundaries cannot invert or overlap a same-domain neighbor.
    if(factory.moveAnchor(2,Anchor::left,250.0))return 5;
    if(factory.moveAnchor(2,Anchor::right,600.0))return 6;

    // Cross-domain overlap is intentional and legal.
    auto cross=factory;
    cross.bands[2].domain=Domain::mid;
    cross.bands[3].domain=Domain::side;
    cross.bands[2].left=260.0;cross.bands[2].right=480.0;
    cross.bands[3].left=300.0;cross.bands[3].right=450.0;
    if(!cross.valid())return 7;

    // Build an isolated three-band chain with two exact shared anchors.
    Model chain;
    for(int i=0;i<count;++i)chain.remove(i);
    chain.bands[0]={true,100.0,200.0,Domain::stereo,{}};
    chain.bands[1]={true,200.0,400.0,Domain::stereo,{}};
    chain.bands[2]={true,400.0,800.0,Domain::stereo,{}};
    if(!chain.connect(0,Anchor::right,1,Anchor::left)
       ||!chain.connect(1,Anchor::right,2,Anchor::left)
       ||!chain.valid())return 8;
    const double widths[]{
        std::log2(chain.bands[0].right/chain.bands[0].left),
        std::log2(chain.bands[1].right/chain.bands[1].left),
        std::log2(chain.bands[2].right/chain.bands[2].left)};
    if(!chain.moveGroup(1,1.0)||!close(chain.bands[0].left,200.0)
       ||!close(chain.bands[0].right,chain.bands[1].left)
       ||!close(chain.bands[1].right,chain.bands[2].left))return 9;
    for(int i=0;i<3;++i)
        if(!close(std::log2(chain.bands[i].right/chain.bands[i].left),widths[i],1e-9))
            return 10;

    // A shared boundary resizes both adjacent regions, then explicit unlink
    // releases both ends without changing either independently assigned band.
    if(!chain.moveAnchor(1,Anchor::right,900.0)
       ||!close(chain.bands[2].left,900.0))return 11;
    chain.unlink(1,Anchor::right);
    if(chain.link(1,Anchor::right).band>=0
       ||chain.link(2,Anchor::left).band>=0||!chain.valid())return 12;

    // A connected group stops rigidly at an unconnected blocker.
    chain.bands[3]={true,3000.0,4000.0,Domain::stereo,{}};
    const double beforeWidth=std::log2(chain.bands[1].right/chain.bands[1].left);
    if(chain.moveGroup(0,10.0)||chain.bands[2].right>3000.01
       ||!close(std::log2(chain.bands[1].right/chain.bands[1].left),beforeWidth,1e-9))
        return 13;

    // Snapshot copy/restore is deterministic, providing a safe undo/state
    // primitive without mutable GUI ownership in the audio thread.
    const Model snapshot=chain;
    chain.remove(1);
    chain=snapshot;
    if(!chain.valid()||!chain.bands[1].present
       ||chain.link(0,Anchor::right).band!=1)return 14;

    std::cout<<"Scalpel magnetic model: seven-band factory, delete/recreate, "
                "dual anchors, cross-domain overlap, shared resize, group move, "
                "explicit unlink and deterministic restore PASS\n";
    return 0;
}

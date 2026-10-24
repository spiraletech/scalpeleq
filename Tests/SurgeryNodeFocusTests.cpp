#include "../Source/SurgeryNodeFocus.h"
#include <cmath>
#include <iostream>
int main(){
    using namespace surgery::ui;
    for(float width:{808.0f,1088.0f,1768.0f}){
        CarveRect last{};
        for(int slot=0;slot<4;++slot){
            const auto r=carveTabBounds(width,slot);
            if(r.x<46.0f||r.y<20.0f||r.y+r.h>46.0f
                ||r.x+r.w>width-46.0f)return 1;
            if(slot>0&&last.x+last.w>=r.x)return 2;
            if(!r.contains(r.x+r.w/2.0f,r.y+r.h/2.0f))return 3;
            last=r;
        }
    }
    for(int root=0;root<7;++root)
        for(int owner=0;owner<7;++owner){
            if(showForegroundCarve(root,owner,true)!=(root==owner))return 4;
            if(showForegroundCarve(root,owner,false))return 5;
        }
    // Every OFF node is invisible even in the selected ROOT; its rail
    // selector is still present as a deliberate activation route.
    for(int root=0;root<7;++root)
        for(int slot=0;slot<4;++slot){
            if(showForegroundCarve(root,root,false))return 7;
            const auto selector=carveTabBounds(1088.0f,slot);
            if(!selector.contains(selector.x+20.0f,selector.y+10.0f))return 8;
        }
    for(float depth:{0.0f,4.0f,24.0f,36.0f}){
        const float y=carveY(140.0f,depth);
        if(std::abs(depthFromY(140.0f,y)-depth)>0.002f)return 6;
    }
    std::cout<<"Selected ROOT CARVE foreground layout and hit geometry passed\n";
    return 0;
}

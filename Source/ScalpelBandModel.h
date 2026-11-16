#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace scalpel::bands {
constexpr int count=7;
enum class Domain:int {stereo=0,mid=1,side=2};
enum class Anchor:int {left=0,right=1};
struct Link { int band=-1; Anchor anchor=Anchor::left; };
struct Band {
    bool present=false;
    double left=20,right=60;
    Domain domain=Domain::stereo;
    std::array<Link,2> links{};
};
struct Model {
    std::array<Band,count> bands{};
    static constexpr double minimumRatio=1.001;
    static double clampHz(double value) noexcept {
        return std::clamp(value,20.0,20000.0);
    }
    double& frequency(int band,Anchor anchor) noexcept {
        return anchor==Anchor::left?bands[(size_t)band].left:bands[(size_t)band].right;
    }
    double frequency(int band,Anchor anchor) const noexcept {
        return anchor==Anchor::left?bands[(size_t)band].left:bands[(size_t)band].right;
    }
    Link& link(int band,Anchor anchor) noexcept {return bands[(size_t)band].links[(size_t)anchor];}
    Link link(int band,Anchor anchor) const noexcept {return bands[(size_t)band].links[(size_t)anchor];}
    bool validLink(int band,Anchor anchor) const noexcept {
        const auto l=link(band,anchor);
        if(l.band<0)return true;
        if(l.band>=count||l.band==band||!bands[(size_t)l.band].present)return false;
        const auto reverse=link(l.band,l.anchor);
        return reverse.band==band&&reverse.anchor==anchor&&
            bands[(size_t)l.band].domain==bands[(size_t)band].domain&&
            std::abs(frequency(band,anchor)-frequency(l.band,l.anchor))<.01;
    }
    bool valid() const noexcept {
        for(int i=0;i<count;++i){
            const auto& b=bands[(size_t)i];if(!b.present)continue;
            if(!(b.left>=20&&b.right<=20000&&b.right>=b.left*minimumRatio))return false;
            if(!validLink(i,Anchor::left)||!validLink(i,Anchor::right))return false;
            for(int j=i+1;j<count;++j){
                const auto& other=bands[(size_t)j];
                if(!other.present||other.domain!=b.domain)continue;
                const bool shared=std::abs(b.right-other.left)<.01||std::abs(other.right-b.left)<.01;
                if(!shared&&std::max(b.left,other.left)<std::min(b.right,other.right))return false;
            }
        }
        return true;
    }
    void unlink(int band,Anchor anchor) noexcept {
        if(band<0||band>=count)return;const auto old=link(band,anchor);link(band,anchor)={};
        if(old.band>=0&&old.band<count){const auto reverse=link(old.band,old.anchor);if(reverse.band==band&&reverse.anchor==anchor)link(old.band,old.anchor)={};}
    }
    bool connect(int a,Anchor aa,int b,Anchor ba) noexcept {
        if(a<0||a>=count||b<0||b>=count||a==b||!bands[(size_t)a].present||!bands[(size_t)b].present||bands[(size_t)a].domain!=bands[(size_t)b].domain)return false;
        const auto backup=*this;
        unlink(a,aa);unlink(b,ba);const double shared=.5*(frequency(a,aa)+frequency(b,ba));
        frequency(a,aa)=frequency(b,ba)=shared;link(a,aa)={b,ba};link(b,ba)={a,aa};
        if(!valid()){*this=backup;return false;}return true;
    }
    void remove(int band) noexcept {
        if(band<0||band>=count)return;unlink(band,Anchor::left);unlink(band,Anchor::right);bands[(size_t)band].present=false;
    }
    int add(double center,Domain domain=Domain::stereo) noexcept {
        int slot=-1;for(int i=0;i<count;++i)if(!bands[(size_t)i].present){slot=i;break;}if(slot<0)return -1;
        center=clampHz(center);double lo=clampHz(center/std::sqrt(2.0)),hi=clampHz(center*std::sqrt(2.0));
        for(const auto& b:bands)if(b.present&&b.domain==domain){
            if(center>=b.left&&center<=b.right)return -1;
            if(hi>b.left&&center<b.left)hi=b.left;
            if(lo<b.right&&center>b.right)lo=b.right;
        }
        if(hi<lo*minimumRatio)return -1;
        bands[(size_t)slot]={true,lo,hi,domain,{}};return slot;
    }
    std::array<bool,count> group(int seed) const noexcept {
        std::array<bool,count> member{};if(seed<0||seed>=count||!bands[(size_t)seed].present)return member;
        member[(size_t)seed]=true;bool changed=true;
        while(changed){changed=false;for(int i=0;i<count;++i)if(member[(size_t)i])for(const auto a:{Anchor::left,Anchor::right}){const auto l=link(i,a);if(l.band>=0&&!member[(size_t)l.band]){member[(size_t)l.band]=true;changed=true;}}}
        return member;
    }
    bool moveGroup(int seed,double octaves) noexcept {
        auto member=group(seed);double minShift=-100,maxShift=100;
        for(int i=0;i<count;++i)if(member[(size_t)i]){minShift=std::max(minShift,std::log2(20.0/bands[(size_t)i].left));maxShift=std::min(maxShift,std::log2(20000.0/bands[(size_t)i].right));}
        for(int i=0;i<count;++i)if(member[(size_t)i])for(int j=0;j<count;++j)if(!member[(size_t)j]&&bands[(size_t)j].present&&bands[(size_t)i].domain==bands[(size_t)j].domain){
            if(bands[(size_t)i].right<=bands[(size_t)j].left)maxShift=std::min(maxShift,std::log2(bands[(size_t)j].left/bands[(size_t)i].right));
            if(bands[(size_t)i].left>=bands[(size_t)j].right)minShift=std::max(minShift,std::log2(bands[(size_t)j].right/bands[(size_t)i].left));
        }
        const double shift=std::clamp(octaves,minShift,maxShift),ratio=std::exp2(shift);
        for(int i=0;i<count;++i)if(member[(size_t)i]){bands[(size_t)i].left*=ratio;bands[(size_t)i].right*=ratio;}return std::abs(shift-octaves)<1.e-9;
    }
    bool moveAnchor(int band,Anchor anchor,double hz) noexcept {
        if(band<0||band>=count||!bands[(size_t)band].present)return false;auto backup=*this;hz=clampHz(hz);
        frequency(band,anchor)=hz;const auto l=link(band,anchor);if(l.band>=0)frequency(l.band,l.anchor)=hz;
        if(!valid()){*this=backup;return false;}return true;
    }
};
}

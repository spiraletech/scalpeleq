#pragma once
// Fixed-header layout that keeps THEME completely above the ROOT tabs,
// away from branding and title even at the supported minimum editor width.
namespace surgery::ui {
struct Rect {
    int x=0,y=0,width=0,height=0;
    constexpr int right() const noexcept {return x+width;}
    constexpr int bottom() const noexcept {return y+height;}
    constexpr bool intersects(const Rect& other) const noexcept {
        return x<other.right()&&right()>other.x&&y<other.bottom()&&bottom()>other.y;
    }
};
struct HeaderLayout {
    Rect title,brand,theme;
    int rootsY=59;
};
constexpr HeaderLayout headerLayout(int w) noexcept {
    return {{35,13,325,37},
            {365,19,w-585,22},
            {w-209,15,188,28},59};
}
} // namespace surgery::ui

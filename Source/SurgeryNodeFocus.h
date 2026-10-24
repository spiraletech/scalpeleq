#pragma once
// Interactive ROOT/CARVE presentation geometry. Independent of JUCE and DSP.
// The selected ROOT gets four top-rail CARVE slots; OFF slots are selectors,
// not fake EQ points. Active markers retain their true host frequency/depth.
#include <algorithm>
namespace surgery::ui {
constexpr int kSlotsPerRoot=4;
struct CarveRect {
    float x=0,y=0,w=0,h=0;
    constexpr bool contains(float px,float py) const noexcept {
        return px>=x&&px<=x+w&&py>=y&&py<=y+h;
    }
};
constexpr CarveRect carveTabBounds(float graphWidth,int slot) noexcept {
    constexpr float width=76.0f,gap=7.0f;
    const float first=graphWidth-46.0f-kSlotsPerRoot*width-(kSlotsPerRoot-1)*gap;
    return {first+slot*(width+gap),24.0f,width,20.0f};
}
constexpr float carveY(float centre,float depthDb) noexcept {
    return centre+23.0f+std::clamp(depthDb,0.0f,36.0f)*1.10f;
}
constexpr float depthFromY(float centre,float y) noexcept {
    return std::clamp((y-centre-23.0f)/1.10f,0.0f,36.0f);
}
constexpr bool showForegroundCarve(int selectedRoot,int ownerRoot,bool enabled) noexcept {
    return selectedRoot>=0&&selectedRoot<7&&ownerRoot==selectedRoot&&enabled;
}
}

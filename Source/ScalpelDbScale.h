#pragma once
#include <array>

// Display coordinates only. The measured FFT values and presentation tilt
// remain in their existing analyzer path; this scale never touches audio.
namespace scalpel::dbaxis {
inline constexpr float rangeDb=60.f;
inline constexpr std::array<int,11> labels{0,-5,-10,-15,-20,-25,-30,-35,-40,-50,-60};
inline constexpr float logicalPerNativePixel=1120.f/1344.f;
inline constexpr float gutterGap=6.f*logicalPerNativePixel;
inline constexpr float gutterWidth=40.f*logicalPerNativePixel;
inline constexpr float labelRightOffset=28.f*logicalPerNativePixel;
inline constexpr float unitElevation=17.f*logicalPerNativePixel;
inline constexpr float labelFontHeight=11.f*logicalPerNativePixel;
inline constexpr float labelInkHeight=8.f*logicalPerNativePixel;
inline constexpr float unitFontHeight=9.f*logicalPerNativePixel;

// Shared by spectrum, peak trace and calibrated grid/axis labels. Inputs are
// already clipped by surgery::displayDb to [-60,0], as before this patch.
inline constexpr float toY(float db,float top,float height) noexcept {
    return top-db*height/rangeDb;
}
}

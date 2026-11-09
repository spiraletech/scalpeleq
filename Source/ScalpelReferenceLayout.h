#pragma once
#include <JuceHeader.h>
#include <array>

namespace scalpel::reference {
constexpr float logicalPerReference=1120.0f/1536.0f;
inline juce::Rectangle<float> rect(float x,float y,float w,float h){
    return juce::Rectangle<float>(x,y,w,h)*logicalPerReference;
}
inline juce::Rectangle<int> bounds(float x,float y,float w,float h){
    return rect(x,y,w,h).toNearestInt();
}
inline const auto header=rect(24,16,1488,78);
inline const auto selectorRail=rect(26,104,1484,56);
inline const auto analyzer=rect(24,170,1488,315);
inline const auto navigation=rect(28,490,1480,65);
inline const auto deck=rect(26,565,1485,260);
inline constexpr std::array<float,5> knobCentres{187,479,768,1055,1341};
}

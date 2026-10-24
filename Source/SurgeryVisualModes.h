#pragma once
#include "SurgerySynesthesia.h"
#include "SurgeryWeatherField.h"
namespace surgery::visual {
// Standard+ is the sole active style. Legacy values remain stored for recall.
inline weather::Pixel style(int mode,const weather::Meaning& f,float,float u,float v,weather::Pixel) noexcept {
    const auto base=synesthesia::RGB{11/255.f,17/255.f,24/255.f};
    const auto colour=synesthesia::read(f,u,.12f);
    const float amount=mode>0?.08f*f.level*synesthesia::localEnergy(f,u)*(1-v):0;
    auto encode=[&](float b,float c){return (std::uint8_t)std::round(255*weather::unit(b+(c-b)*amount));};
    return {encode(base.r,colour.r),encode(base.g,colour.g),encode(base.b,colour.b)};
}
} // namespace surgery::visual

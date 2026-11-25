#pragma once
#include <algorithm>

// ET-MOTION-014: presentation-only math. No JUCE timer, selection or DSP owner.
namespace scalpel::aura {
enum class State { disabled, pressed, selected, hover, idle };
inline constexpr double compressionMs=160.0;
inline constexpr float referenceTravel=1.75f;
inline constexpr State state(bool enabled,bool down,bool selected,bool over) noexcept {
    return !enabled?State::disabled:down?State::pressed:selected?State::selected:over?State::hover:State::idle;
}
inline constexpr float targetTravel(State s) noexcept {
    return s==State::pressed||s==State::selected?referenceTravel:0.f;
}
class Travel final {
public:
    float sample(double nowMs) const noexcept {
        const auto t=std::clamp((nowMs-startMs_)/compressionMs,0.0,1.0);
        if(t>=1.0)return target_;
        return from_+(target_-from_)*static_cast<float>(t*t*(3.0-2.0*t));
    }
    bool retarget(float target,double nowMs) noexcept {
        if(target==target_)return false; // Polls/repeated selection never restart it.
        from_=sample(nowMs);target_=target;startMs_=nowMs;return true;
    }
    bool active(double nowMs) const noexcept {
        return from_!=target_&&nowMs<startMs_+compressionMs;
    }
    float target() const noexcept {return target_;}
private:
    float from_=0.f,target_=0.f;
    double startMs_=0.0;
};
struct Fit {
    float scale,textureX,textureY;
};
inline Fit fit(float width,float height) noexcept {
    const float s=std::min(width/135.f,height/42.f);
    return {s,(width-135.f*s)*.5f-4.f*s,(height-42.f*s)*.5f-4.f*s};
}
inline constexpr float surfaceScale(float d) noexcept {return (40.8f-d)/40.8f;}
inline constexpr float surfaceOffset(float d) noexcept {return 4.6f+d-surfaceScale(d)*4.6f;}
}

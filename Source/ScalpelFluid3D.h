#pragma once
// SCALPEL X-RAY 3.0 -- Windows GPU volumetric fluid simulation.
// This component runs exclusively on the editor/message thread. Its input is
// already-measured analyzer data; neither processBlock nor the analyzer worker
// ever wait for the graphics device.
#include <JuceHeader.h>
#include <array>
#include <memory>

namespace scalpel::xray {

struct FluidColumn {
    float energy = 0.0f;  // Calibrated display energy, [0, 1].
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
};

class FluidVolume3D final {
public:
    static constexpr int gridWidth = 64;
    static constexpr int gridHeight = 40;
    static constexpr int gridDepth = 32;
    static constexpr int imageWidth = 640;
    static constexpr int imageHeight = 224;

    FluidVolume3D();
    ~FluidVolume3D();
    FluidVolume3D(const FluidVolume3D&) = delete;
    FluidVolume3D& operator=(const FluidVolume3D&) = delete;

    // Advances a bounded pressure-projected three-dimensional velocity field
    // at up to 30 Hz and independently ray-marches its participating phases.
    // Returns true only when a GPU result has safely reached the GUI image.
    // Failure leaves the existing Scalpel CPU visualizer available as fallback.
    bool render(const std::array<FluidColumn, gridWidth>& columns,
                float bass, float flux, float level,
                double elapsedSeconds, bool reducedMotion,
                juce::Image& target,int physicalWidth=imageWidth,int physicalHeight=imageHeight);

    // Compiles all six embedded HLSL kernels without requiring a GPU.
    // Used by Windows CI to catch silent shader fallback before release.
    static bool validateShaderSource() noexcept;
    bool hasFrame() const noexcept;
    // Used to diagnose old binaries and GPU/CPU fallbacks in FL Studio.
    const char* statusText() const noexcept;
    void reset() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace scalpel::xray

#pragma once
#include <JuceHeader.h>

namespace scalpel {
// Static embedded artwork must not retain Direct2D/D3D device references until
// CRT destruction under the Windows DLL loader lock. GPU simulation resources
// remain editor-owned and are released explicitly before host unload.
inline juce::Image loadSoftwareAsset(const void* data, std::size_t bytes) {
    const auto decoded = juce::ImageFileFormat::loadFrom(data, bytes);
    return decoded.isValid() ? juce::SoftwareImageType{}.convert(decoded)
                             : juce::Image{};
}
}

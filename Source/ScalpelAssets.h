#pragma once
#include <JuceHeader.h>

namespace scalpel {
// Static artwork must not retain Direct2D/D3D device references until CRT
// destruction under the Windows DLL loader lock. GPU simulation resources
// belong exclusively to the editor and are released before the host unloads.
inline juce::Image loadSoftwareAsset(const void* data,size_t bytes){
    const auto decoded=juce::ImageFileFormat::loadFrom(data,bytes);
    return juce::SoftwareImageType{}.convert(decoded);
}
}

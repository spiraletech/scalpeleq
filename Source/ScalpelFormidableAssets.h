#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
#include <array>

namespace scalpel::formidable {
enum class Art { shell, control_deck, analyzer, band_well, page_well, plot_glass, master_idle, master_selected, master_hover, master_pressed, master_disabled, tab_idle, tab_selected, tab_hover, tab_pressed, band_idle, band_selected, band_pressed, command_idle, command_pressed, knob_face, knob_pointer, dropdown_idle, dropdown_hover, screw, readout_idle, champagne_header, count };

inline const juce::Image& image(Art asset){
    static const auto images=[] {
        std::array<juce::Image,static_cast<size_t>(Art::count)> result {};
        result[static_cast<size_t>(Art::shell)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_shell_png,BinaryData::f1081_shell_pngSize);
        result[static_cast<size_t>(Art::control_deck)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_control_deck_png,BinaryData::f1081_control_deck_pngSize);
        result[static_cast<size_t>(Art::analyzer)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_analyzer_png,BinaryData::f1081_analyzer_pngSize);
        result[static_cast<size_t>(Art::band_well)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_band_well_png,BinaryData::f1081_band_well_pngSize);
        result[static_cast<size_t>(Art::page_well)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_page_well_png,BinaryData::f1081_page_well_pngSize);
        result[static_cast<size_t>(Art::plot_glass)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_plot_glass_png,BinaryData::f1081_plot_glass_pngSize);
        result[static_cast<size_t>(Art::master_idle)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_master_idle_png,BinaryData::f1081_master_idle_pngSize);
        result[static_cast<size_t>(Art::master_selected)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_master_selected_png,BinaryData::f1081_master_selected_pngSize);
        result[static_cast<size_t>(Art::master_hover)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_master_hover_png,BinaryData::f1081_master_hover_pngSize);
        result[static_cast<size_t>(Art::master_pressed)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_master_pressed_png,BinaryData::f1081_master_pressed_pngSize);
        result[static_cast<size_t>(Art::master_disabled)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_master_disabled_png,BinaryData::f1081_master_disabled_pngSize);
        result[static_cast<size_t>(Art::tab_idle)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_tab_idle_png,BinaryData::f1081_tab_idle_pngSize);
        result[static_cast<size_t>(Art::tab_selected)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_tab_selected_png,BinaryData::f1081_tab_selected_pngSize);
        result[static_cast<size_t>(Art::tab_hover)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_tab_hover_png,BinaryData::f1081_tab_hover_pngSize);
        result[static_cast<size_t>(Art::tab_pressed)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_tab_pressed_png,BinaryData::f1081_tab_pressed_pngSize);
        result[static_cast<size_t>(Art::band_idle)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_band_idle_png,BinaryData::f1081_band_idle_pngSize);
        result[static_cast<size_t>(Art::band_selected)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_band_selected_png,BinaryData::f1081_band_selected_pngSize);
        result[static_cast<size_t>(Art::band_pressed)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_band_pressed_png,BinaryData::f1081_band_pressed_pngSize);
        result[static_cast<size_t>(Art::command_idle)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_command_idle_png,BinaryData::f1081_command_idle_pngSize);
        result[static_cast<size_t>(Art::command_pressed)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_command_pressed_png,BinaryData::f1081_command_pressed_pngSize);
        result[static_cast<size_t>(Art::knob_face)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_knob_face_png,BinaryData::f1081_knob_face_pngSize);
        result[static_cast<size_t>(Art::knob_pointer)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_knob_pointer_png,BinaryData::f1081_knob_pointer_pngSize);
        result[static_cast<size_t>(Art::dropdown_idle)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_dropdown_idle_png,BinaryData::f1081_dropdown_idle_pngSize);
        result[static_cast<size_t>(Art::dropdown_hover)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_dropdown_hover_png,BinaryData::f1081_dropdown_hover_pngSize);
        result[static_cast<size_t>(Art::screw)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_screw_png,BinaryData::f1081_screw_pngSize);
        result[static_cast<size_t>(Art::readout_idle)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_readout_idle_png,BinaryData::f1081_readout_idle_pngSize);
        result[static_cast<size_t>(Art::champagne_header)]=juce::ImageFileFormat::loadFrom(
            BinaryData::f1081_champagne_header_png,BinaryData::f1081_champagne_header_pngSize);
        const juce::SoftwareImageType software;
        for(auto& frame:result)frame=software.convert(frame);
        return result;
    }();
    return images[static_cast<size_t>(asset)];
}
inline void draw(juce::Graphics& g,Art asset,juce::Rectangle<float> bounds){
    g.drawImage(image(asset),bounds,juce::RectanglePlacement::stretchToFit);
}
} // namespace scalpel::formidable

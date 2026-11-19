#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
#include <array>

namespace scalpel::formidable {
enum class Art { shell, control_deck, analyzer, band_well, page_well, plot_glass,
    master_idle, master_selected, master_hover, master_pressed, master_disabled,
    tab_idle, tab_selected, tab_hover, tab_pressed, band_idle, band_selected,
    band_pressed, command_idle, command_hover, command_pressed, knob_face,
    knob_pointer, dropdown_idle, dropdown_hover, screw, readout_idle,
    champagne_header, analyzer_faceplate_10842, analyzer_bezel_10842,
    analyzer_smoked_10842, analyzer_gasket_10842, analyzer_pixels_10842,
    analyzer_reflection_10842, spectrum_falloff_10842, meter_well_10842,
    meter_active_10842, meter_clip_10842, meter_off_10842,
    peak_window_10842, xray_face_10842, xray_lamp_off_10842,
    xray_lamp_on_10842, count };

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
            BinaryData::master_idle_png,BinaryData::master_idle_pngSize);
        result[static_cast<size_t>(Art::master_selected)]=juce::ImageFileFormat::loadFrom(
            BinaryData::master_selected_png,BinaryData::master_selected_pngSize);
        result[static_cast<size_t>(Art::master_hover)]=juce::ImageFileFormat::loadFrom(
            BinaryData::master_hover_png,BinaryData::master_hover_pngSize);
        result[static_cast<size_t>(Art::master_pressed)]=juce::ImageFileFormat::loadFrom(
            BinaryData::master_pressed_png,BinaryData::master_pressed_pngSize);
        result[static_cast<size_t>(Art::master_disabled)]=juce::ImageFileFormat::loadFrom(
            BinaryData::master_disabled_png,BinaryData::master_disabled_pngSize);
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
            BinaryData::command_idle_png,BinaryData::command_idle_pngSize);
        result[static_cast<size_t>(Art::command_hover)]=juce::ImageFileFormat::loadFrom(
            BinaryData::command_hover_png,BinaryData::command_hover_pngSize);
        result[static_cast<size_t>(Art::command_pressed)]=juce::ImageFileFormat::loadFrom(
            BinaryData::command_pressed_png,BinaryData::command_pressed_pngSize);
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
        result[static_cast<size_t>(Art::analyzer_faceplate_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::graphite_faceplate_png,BinaryData::graphite_faceplate_pngSize);
        result[static_cast<size_t>(Art::analyzer_bezel_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::champagne_bezel_png,BinaryData::champagne_bezel_pngSize);
        result[static_cast<size_t>(Art::analyzer_smoked_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::smoked_polycarbonate_png,BinaryData::smoked_polycarbonate_pngSize);
        result[static_cast<size_t>(Art::analyzer_gasket_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::lens_gasket_png,BinaryData::lens_gasket_pngSize);
        result[static_cast<size_t>(Art::analyzer_pixels_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::display_pixel_mask_png,BinaryData::display_pixel_mask_pngSize);
        result[static_cast<size_t>(Art::analyzer_reflection_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::polycarbonate_reflection_png,BinaryData::polycarbonate_reflection_pngSize);
        result[static_cast<size_t>(Art::spectrum_falloff_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::spectrum_falloff_png,BinaryData::spectrum_falloff_pngSize);
        result[static_cast<size_t>(Art::meter_well_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::meter_well_png,BinaryData::meter_well_pngSize);
        result[static_cast<size_t>(Art::meter_active_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::meter_segment_active_png,BinaryData::meter_segment_active_pngSize);
        result[static_cast<size_t>(Art::meter_clip_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::meter_segment_clip_png,BinaryData::meter_segment_clip_pngSize);
        result[static_cast<size_t>(Art::meter_off_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::meter_segment_off_png,BinaryData::meter_segment_off_pngSize);
        result[static_cast<size_t>(Art::peak_window_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::peak_display_window_png,BinaryData::peak_display_window_pngSize);
        result[static_cast<size_t>(Art::xray_face_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::xray_face_png,BinaryData::xray_face_pngSize);
        result[static_cast<size_t>(Art::xray_lamp_off_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::xray_lamp_off_png,BinaryData::xray_lamp_off_pngSize);
        result[static_cast<size_t>(Art::xray_lamp_on_10842)]=juce::ImageFileFormat::loadFrom(
            BinaryData::xray_lamp_on_png,BinaryData::xray_lamp_on_pngSize);
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

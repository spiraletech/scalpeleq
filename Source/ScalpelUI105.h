#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include "ScalpelMaterials.h"

namespace scalpel::ui105 {
inline const juce::Colour champagne{0xff0d1720};
inline const juce::Colour ivory{0xffedf5ff};
inline const juce::Colour ink{0xff070d13};
inline const juce::Colour amber{0xff9edcff};
inline const juce::Colour chassisEdge{0xff4e83a5};

inline void roundedPanel(juce::Graphics& g, juce::Rectangle<float> r, bool dark, float radius=8.0f)
{
    g.setColour(juce::Colour(0xff02070b).withAlpha(.96f));
    g.fillRoundedRectangle(r.expanded(2.0f).translated(0, 1.5f), radius + 1.0f);
    juce::ColourGradient face(dark ? juce::Colour(0xff0b151d) : juce::Colour(0xffb7c8d5),
                              r.getX(), r.getY(),
                              dark ? juce::Colour(0xff02060a) : juce::Colour(0xff526b7c),
                              r.getX(), r.getBottom(), false);
    if (!dark) face.addColour(.35, juce::Colour(0xffe6f1fb));
    g.setGradientFill(face);
    g.fillRoundedRectangle(r, radius);
    g.setColour((dark ? juce::Colour(0xff2b4a60) : juce::Colour(0xffb7d0df)).withAlpha(.72f));
    g.drawRoundedRectangle(r.reduced(.8f), radius - .6f, 1.0f);
    g.setColour(juce::Colour(0xff061019).withAlpha(.96f));
    g.drawRoundedRectangle(r, radius, 1.0f);
}

inline void drawRackFace(juce::Graphics& g, int width, int height, int mode)
{
    // The chassis itself fills the complete rectangular editor surface.
    // Rounded lines remain as internal industrial detailing, but there is no
    // separate colour/background exposed outside the rack face.
    const auto full = juce::Rectangle<float>(0, 0, (float)width, (float)height);
    // The approved faceplate is near-black midnight titanium.  Blue is a
    // localized reflection, not the base material, so keep the chassis dark
    // enough for the analyzer and frosted hardware to carry the contrast.
    juce::ColourGradient shell(juce::Colour(0xff0f1b24), 0, 0,
                               juce::Colour(0xff0d171e), 0, (float)height, false);
    shell.addColour(.16, juce::Colour(0xff0b151d));
    shell.addColour(.62, juce::Colour(0xff09131b));
    shell.addColour(.92, juce::Colour(0xff0d171e));
    // A restrained edge floor keeps the rounded outer chassis readable at
    // resize/DPI corners without lifting the faceplate midtones.
    g.setColour(juce::Colour(0xff14222c));
    g.fillRect(full);
    g.setGradientFill(shell);
    g.fillRect(full.reduced(10.0f));
    scalpel::materials::surface(g,full.reduced(6.0f),22.0f);
    g.setColour(juce::Colour(0xff35566d).withAlpha(.72f));
    g.drawRoundedRectangle(full.reduced(2.0f), 23.0f, 1.3f);
    g.setColour(juce::Colour(0xff010305).withAlpha(.98f));
    g.drawRoundedRectangle(full.reduced(5.0f), 20.0f, 1.0f);

    roundedPanel(g, {20, 20, (float)width - 40, 60}, true, 12.0f);
    g.setColour(juce::Colour(0xffedf5ff));
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 34.0f, juce::Font::bold)));
    g.drawFittedText("SCALPEL EQ", 56, 30, 255, 37, juce::Justification::centredLeft, 1);
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 11.5f, juce::Font::plain)));
    g.drawFittedText("P R E C I S I O N   E Q U A L I Z E R", 350, 38, 360, 26,
                     juce::Justification::centred, 1);
    g.drawFittedText("A N A L O G   C O N S O L E   S E R I E S", width - 390, 38, 335, 26,
                     juce::Justification::centredRight, 1);
    g.setColour(juce::Colour(0xffc5c9cc));
    g.drawVerticalLine(318,35.0f,65.0f);
    g.drawVerticalLine(803,35.0f,65.0f);

    // 1.0.4 canon: seven persistent, functional BAND selectors sit between
    // the identity plaque and analyzer. The graph remains the dominant field.
    roundedPanel(g, {18, 88, (float)width - 36, 40}, true, 7.0f);
    const auto analyzerFrame = juce::Rectangle<float>(18, 140, (float)width - 36, 246);
    roundedPanel(g, analyzerFrame, true, 8.0f);
    g.setColour(juce::Colour(0xff4d91b2).withAlpha(.78f));
    g.drawRoundedRectangle(analyzerFrame.reduced(1.2f), 8.0f, 2.0f);
    roundedPanel(g, {32, 389, (float)width - 64, 43}, true, 8.0f);
    roundedPanel(g, {20, 449, (float)width - 40, 170}, true, 13.0f);
    scalpel::materials::surface(g,{23,452,(float)width-46,164},11.0f);

    // Mechanical shelf dividers are page-specific and stay inside the
    // intentional gutters. BAND's first and third lines must clear the
    // ON/OFF switches and COMP THRESHOLD label/control respectively.
    std::array<float, 5> dividers{};
    int count = 0;
    if (mode == 0) dividers = {198, 365, 536, 805, 0}, count = 4;
    else if (mode == 1) dividers = {279, 468, 658, 846, 0}, count = 4;
    else if (mode == 2) dividers = {224, 432, 640, 848, 0}, count = 4;
    else dividers = {379, 738, 0, 0, 0}, count = 2;
    for (int i = 0; i < count; ++i)
    {
        g.setColour(juce::Colour(0xff5b7890).withAlpha(.64f));
        g.drawLine(dividers[(size_t)i], 462, dividers[(size_t)i], 603, 1.0f);
        g.setColour(juce::Colour(0xffd9ecfb).withAlpha(.28f));
        g.drawLine(dividers[(size_t)i] + 1, 462, dividers[(size_t)i] + 1, 603, .8f);
    }
}

inline void drawMeter(juce::Graphics& g, juce::Rectangle<float> r, float db, const juce::String& label)
{
    g.setColour(juce::Colour(0xff050505));
    g.fillRoundedRectangle(r, 3.0f);
    constexpr int segments = 15;
    const float normalized = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    const int active = juce::roundToInt(normalized * segments);
    const float gap = 2.0f;
    const float segmentH = (r.getHeight() - gap * (segments - 1)) / segments;
    for (int i = 0; i < segments; ++i)
    {
        const bool lit = i < active;
        auto cell = juce::Rectangle<float>(r.getX() + 4.0f,
            r.getBottom() - (i + 1) * segmentH - i * gap,
            r.getWidth() - 8.0f, segmentH);
        const auto hot = i > 12 ? juce::Colour(0xfff3fbff)
                                : i > 9 ? juce::Colour(0xffcceeff) : amber;
        g.setColour(lit ? hot : juce::Colour(0xff292522));
        g.fillRect(cell);
    }
    g.setColour(ivory);
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 10.5f, juce::Font::bold)));
    g.drawText(label, (int)r.getX(), (int)r.getBottom() + 2, (int)r.getWidth(), 14,
               juce::Justification::centred);
}

inline void drawAnalyzerPanel(juce::Graphics& g, juce::Rectangle<float> full,
                              float peakHold, float leftPeak, float rightPeak)
{
    const auto panel = juce::Rectangle<float>(full.getRight() - 218.0f, full.getY() + 15.0f,
                                               202.0f, full.getHeight() - 31.0f);
    g.setColour(juce::Colour(0xff060b11).withAlpha(.98f));
    g.fillRoundedRectangle(panel, 6.0f);
    g.setColour(juce::Colour(0xff29465a));
    g.drawRoundedRectangle(panel, 6.0f, 1.0f);

    g.setColour(ivory.withAlpha(.90f));
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 10.0f, juce::Font::bold)));
    g.drawText("PEAK (dBFS)", (int)panel.getX() + 8, (int)panel.getY() + 56, 112, 16,
               juce::Justification::centred);
    auto display = juce::Rectangle<float>(panel.getX() + 11, panel.getY() + 73, 108, 38);
    g.setColour(juce::Colour(0xff050403));
    g.fillRoundedRectangle(display, 4.0f);
    g.setColour(juce::Colour(0xff294457));
    g.drawRoundedRectangle(display, 4.0f, .8f);
    g.setColour(amber);
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 20.0f, juce::Font::bold)));
    const auto text = peakHold <= -143.0f ? juce::String("-inf") : juce::String(peakHold, 2);
    g.drawText(text, display.toNearestInt(), juce::Justification::centred);

    drawMeter(g, {panel.getRight() - 70, panel.getY() + 18, 24, panel.getHeight() - 45}, leftPeak, "L");
    drawMeter(g, {panel.getRight() - 38, panel.getY() + 18, 24, panel.getHeight() - 45}, rightPeak, "R");
}
} // namespace scalpel::ui105

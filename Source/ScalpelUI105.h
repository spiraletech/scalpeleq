#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace scalpel::ui105 {
inline const juce::Colour champagne{0xffd8a273};
inline const juce::Colour ivory{0xffffe8c5};
inline const juce::Colour ink{0xff120e0a};
inline const juce::Colour amber{0xffff8800};
inline const juce::Colour chassisEdge{0xffc58c60};

inline void roundedPanel(juce::Graphics& g, juce::Rectangle<float> r, bool dark, float radius=8.0f)
{
    g.setColour(juce::Colour(0xff52392b).withAlpha(.90f));
    g.fillRoundedRectangle(r.expanded(2.0f).translated(0, 1.5f), radius + 1.0f);
    juce::ColourGradient face(dark ? juce::Colour(0xff171513) : juce::Colour(0xffffe5c8),
                              r.getX(), r.getY(),
                              dark ? juce::Colour(0xff050505) : juce::Colour(0xffc99b7b),
                              r.getX(), r.getBottom(), false);
    if (!dark) face.addColour(.35, juce::Colour(0xffefc8a8));
    g.setGradientFill(face);
    g.fillRoundedRectangle(r, radius);
    g.setColour((dark ? juce::Colour(0xffb48f73) : juce::Colour(0xffffeed8)).withAlpha(.80f));
    g.drawRoundedRectangle(r.reduced(.8f), radius - .6f, 1.0f);
    g.setColour(juce::Colour(0xff4a3328).withAlpha(.72f));
    g.drawRoundedRectangle(r, radius, 1.0f);
}

inline void drawRackFace(juce::Graphics& g, int width, int height, int mode)
{
    // Cover every editor pixel. The rounded face may reveal this warm edge,
    // never a black plugin-owned letterbox when a host presents the window.
    g.fillAll(chassisEdge);
    auto outer = juce::Rectangle<float>(1, 1, (float)width - 2, (float)height - 2);
    juce::ColourGradient shell(juce::Colour(0xffffe0b7), 0, 0,
                               juce::Colour(0xffb77e52), 0, (float)height, false);
    shell.addColour(.16, juce::Colour(0xfff2c292));
    shell.addColour(.62, juce::Colour(0xffdca476));
    shell.addColour(.92, juce::Colour(0xffc78c60));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(outer, 25.0f);
    g.setColour(juce::Colour(0xffffead4).withAlpha(.90f));
    g.drawRoundedRectangle(outer.reduced(2.0f), 23.0f, 1.3f);
    g.setColour(juce::Colour(0xff694735).withAlpha(.85f));
    g.drawRoundedRectangle(outer.reduced(5.0f), 20.0f, 1.0f);

    // Sanyo-style ventilation: evenly machined, never decorative noise.
    for (int i = 0; i < 11; ++i)
    {
        const float y = 11.0f + i * 4.2f;
        auto vent = juce::Rectangle<float>(38.0f, y, (float)width - 76.0f, 1.45f);
        g.setColour(juce::Colour(0xff7d5742).withAlpha(.46f));
        g.fillRoundedRectangle(vent, .72f);
        g.setColour(juce::Colour(0xffffe3c8).withAlpha(.38f));
        g.fillRoundedRectangle(vent.translated(0, -1.0f), .72f);
    }

    roundedPanel(g, {24, 64, (float)width - 48, 48}, true, 11.0f);
    g.setColour(juce::Colour(0xffffe3c8));
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 28.0f, juce::Font::bold | juce::Font::italic)));
    g.drawFittedText("SCALPEL EQ", 58, 69, 255, 37, juce::Justification::centredLeft, 1);
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 11.5f, juce::Font::plain)));
    g.drawFittedText("P R E C I S I O N   E Q U A L I Z E R", 350, 75, 360, 26,
                     juce::Justification::centred, 1);
    g.drawFittedText("A N A L O G   C O N S O L E   S E R I E S", width - 390, 75, 335, 26,
                     juce::Justification::centredRight, 1);
    g.setColour(amber);
    for (int i = 0; i < 3; ++i)
    {
        juce::Path slash;
        const float x = width * .63f + i * 22.0f;
        slash.startNewSubPath(x + 10, 76);
        slash.lineTo(x + 28, 76);
        slash.lineTo(x + 16, 99);
        slash.lineTo(x - 2, 99);
        slash.closeSubPath();
        g.fillPath(slash);
    }

    // 1.0.4 canon: seven persistent, functional BAND selectors sit between
    // the identity plaque and analyzer. The graph remains the dominant field.
    roundedPanel(g, {18, 118, (float)width - 36, 38}, false, 7.0f);
    roundedPanel(g, {18, 160, (float)width - 36, 246}, true, 8.0f);
    roundedPanel(g, {32, 412, (float)width - 64, 43}, false, 8.0f);
    roundedPanel(g, {20, 461, (float)width - 40, 158}, false, 13.0f);

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
        g.setColour(juce::Colour(0xff6d4c3a).withAlpha(.64f));
        g.drawLine(dividers[(size_t)i], 474, dividers[(size_t)i], 603, 1.0f);
        g.setColour(juce::Colour(0xffffead2).withAlpha(.38f));
        g.drawLine(dividers[(size_t)i] + 1, 474, dividers[(size_t)i] + 1, 603, .8f);
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
        const auto hot = i > 12 ? juce::Colour(0xffff3b16)
                                : i > 9 ? juce::Colour(0xffffa000) : amber;
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
    g.setColour(juce::Colour(0xff0b0908).withAlpha(.96f));
    g.fillRoundedRectangle(panel, 6.0f);
    g.setColour(juce::Colour(0xff6b4a39));
    g.drawRoundedRectangle(panel, 6.0f, 1.0f);

    g.setColour(ivory.withAlpha(.90f));
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 10.0f, juce::Font::bold)));
    g.drawText("PEAK (dBFS)", (int)panel.getX() + 8, (int)panel.getY() + 56, 112, 16,
               juce::Justification::centred);
    auto display = juce::Rectangle<float>(panel.getX() + 11, panel.getY() + 73, 108, 38);
    g.setColour(juce::Colour(0xff050403));
    g.fillRoundedRectangle(display, 4.0f);
    g.setColour(juce::Colour(0xff684a38));
    g.drawRoundedRectangle(display, 4.0f, .8f);
    g.setColour(amber);
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 20.0f, juce::Font::bold)));
    const auto text = peakHold <= -143.0f ? juce::String("-inf") : juce::String(peakHold, 2);
    g.drawText(text, display.toNearestInt(), juce::Justification::centred);

    drawMeter(g, {panel.getRight() - 70, panel.getY() + 18, 24, panel.getHeight() - 45}, leftPeak, "L");
    drawMeter(g, {panel.getRight() - 38, panel.getY() + 18, 24, panel.getHeight() - 45}, rightPeak, "R");
}
} // namespace scalpel::ui105

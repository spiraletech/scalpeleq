#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace scalpel::ui105 {
inline const juce::Colour champagne{0xff121212};
inline const juce::Colour ivory{0xfff2f0ec};
inline const juce::Colour ink{0xff080808};
inline const juce::Colour amber{0xffff8300};
inline const juce::Colour chassisEdge{0xff343434};

inline void roundedPanel(juce::Graphics& g, juce::Rectangle<float> r, bool dark, float radius=8.0f)
{
    g.setColour(juce::Colour(0xff202020).withAlpha(.90f));
    g.fillRoundedRectangle(r.expanded(2.0f).translated(0, 1.5f), radius + 1.0f);
    juce::ColourGradient face(dark ? juce::Colour(0xff121212) : juce::Colour(0xff343434),
                              r.getX(), r.getY(),
                              dark ? juce::Colour(0xff080808) : juce::Colour(0xff080808),
                              r.getX(), r.getBottom(), false);
    if (!dark) face.addColour(.35, juce::Colour(0xff202020));
    g.setGradientFill(face);
    g.fillRoundedRectangle(r, radius);
    g.setColour((dark ? juce::Colour(0xff343434) : juce::Colour(0xfff2f0ec)).withAlpha(.80f));
    g.drawRoundedRectangle(r.reduced(.8f), radius - .6f, 1.0f);
    g.setColour(juce::Colour(0xff0d0d0d).withAlpha(.72f));
    g.drawRoundedRectangle(r, radius, 1.0f);
}

inline void drawRackFace(juce::Graphics& g, int width, int height, int mode)
{
    // The chassis itself fills the complete rectangular editor surface.
    // Rounded lines remain as internal industrial detailing, but there is no
    // separate colour/background exposed outside the rack face.
    const auto full = juce::Rectangle<float>(0, 0, (float)width, (float)height);
    juce::ColourGradient shell(juce::Colour(0xff202020), 0, 0,
                               juce::Colour(0xff080808), 0, (float)height, false);
    shell.addColour(.16, juce::Colour(0xff202020));
    shell.addColour(.62, juce::Colour(0xff121212));
    shell.addColour(.92, juce::Colour(0xff080808));
    // The graphite shell owns the complete editor rectangle; no black halo
    // is painted behind it.
    g.setGradientFill(shell);
    // Keep the exposed black corner radius tight so host letterboxing never
    // reads as a second border around the chassis.
    // The chassis owns the full editor rectangle; do not leave a black
    // rounded halo around the product face.
    g.fillRect(full);
    g.setColour(juce::Colour(0xfff2f0ec).withAlpha(.90f));
    g.drawRoundedRectangle(full.reduced(2.0f), 23.0f, 1.3f);
    g.setColour(juce::Colour(0xff0d0d0d).withAlpha(.85f));
    g.drawRoundedRectangle(full.reduced(5.0f), 20.0f, 1.0f);

    // The approved Carbon Polymer face has a clean uninterrupted top shell.\n    roundedPanel(g, {24, 20, (float)width - 48, 60}, true, 11.0f);
    g.setColour(juce::Colour(0xfff2f0ec));
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 28.0f, juce::Font::bold | juce::Font::italic)));
    g.drawFittedText("SCALPEL EQ", 58, 25, 255, 45, juce::Justification::centredLeft, 1);
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 11.5f, juce::Font::plain)));
    g.drawFittedText("P R E C I S I O N   E Q U A L I Z E R", 350, 31, 360, 26,
                     juce::Justification::centred, 1);
    g.drawFittedText("A N A L O G   C O N S O L E   S E R I E S", width - 390, 31, 335, 26,
                     juce::Justification::centredRight, 1);
    g.setColour(amber);
    for (int i = 0; i < 3; ++i)
    {
        juce::Path slash;
        const float x = width * .63f + i * 22.0f;
        slash.startNewSubPath(x + 10, 32);
        slash.lineTo(x + 28, 32);
        slash.lineTo(x + 16, 55);
        slash.lineTo(x - 2, 55);
        slash.closeSubPath();
        g.fillPath(slash);
    }

    // 1.0.4 canon: seven persistent, functional BAND selectors sit between
    // the identity plaque and analyzer. The graph remains the dominant field.
    roundedPanel(g, {18, 88, (float)width - 36, 40}, false, 7.0f);
    const auto analyzerFrame = juce::Rectangle<float>(18, 140, (float)width - 36, 246);
    roundedPanel(g, analyzerFrame, true, 8.0f);
    // The analyzer is the single orange-framed hero feature; the header and
    // lower deck remain restrained carbon polymer with no orange perimeter.
    g.setColour(juce::Colour(0xffff8300));
    g.drawRoundedRectangle(analyzerFrame.reduced(1.2f), 8.0f, 2.4f);
    roundedPanel(g, {32, 389, (float)width - 64, 43}, false, 8.0f);
    // Recessed carbon-fiber control bay: a substantial champagne chassis lip
    // surrounds one continuous material surface, matching the approved reference.
    const auto bay = juce::Rectangle<float>(20, 449, (float)width - 40, 158};
    const auto lip = bay.expanded(5.0f);
    juce::ColourGradient lipFace(juce::Colour(0xff343434), lip.getX(), lip.getY(),
                                 juce::Colour(0xff080808), lip.getX(), lip.getBottom(), false);
    lipFace.addColour(.42, juce::Colour(0xff202020));
    g.setGradientFill(lipFace);
    g.fillRoundedRectangle(lip, 17.0f);
    g.setColour(juce::Colour(0xff0d0d0d).withAlpha(.9f));
    g.drawRoundedRectangle(lip.reduced(1.0f), 16.0f, 2.0f);

    const auto surface = bay.reduced(1.0f);
    juce::ColourGradient carbonFace(juce::Colour(0xff202020), surface.getX(), surface.getY(),
                                    juce::Colour(0xff080808), surface.getX(), surface.getBottom(), false);
    carbonFace.addColour(.48, juce::Colour(0xff121212));
    g.setGradientFill(carbonFace);
    g.fillRoundedRectangle(surface, 12.0f);
    g.saveState();
    g.reduceClipRegion(surface.toNearestInt());
    // Fine diagonal weave, kept subtle like the reference carbon menu.
    for (int y = -170; y < 330; y += 8) {
        g.setColour(juce::Colour(0xff343434).withAlpha(.075f));
        g.drawLine(surface.getX(), surface.getY() + y,
                   surface.getRight(), surface.getY() + y + surface.getWidth(), .7f);
        g.setColour(juce::Colour(0xff080808).withAlpha(.28f));
        g.drawLine(surface.getX(), surface.getY() + y + 2,
                   surface.getRight(), surface.getY() + y + surface.getWidth() + 2, .7f);
    }
    g.restoreState();
    // The edge is integrated into the material/chassis transition, not a
    // separate blank black border.
    g.setColour(juce::Colour(0xff343434).withAlpha(.95f));
    g.drawRoundedRectangle(surface, 12.0f, 2.0f);
    g.setColour(juce::Colour(0xfff2f0ec).withAlpha(.32f));
    g.drawRoundedRectangle(surface.reduced(2.2f), 10.0f, .8f);

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
        g.setColour(juce::Colour(0xff343434).withAlpha(.64f));
        g.drawLine(dividers[(size_t)i], 474, dividers[(size_t)i], 603, 1.0f);
        g.setColour(juce::Colour(0xfff2f0ec).withAlpha(.38f));
        g.drawLine(dividers[(size_t)i] + 1, 474, dividers[(size_t)i] + 1, 603, .8f);
    }
}

inline void drawMeter(juce::Graphics& g, juce::Rectangle<float> r, float db, const juce::String& label)
{
    g.setColour(juce::Colour(0xff080808));
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
                                : i > 9 ? juce::Colour(0xffffa347) : amber;
        g.setColour(lit ? hot : juce::Colour(0xff202020));
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
    g.setColour(juce::Colour(0xff111111).withAlpha(.96f));
    g.fillRoundedRectangle(panel, 6.0f);
    g.setColour(juce::Colour(0xff343434));
    g.drawRoundedRectangle(panel, 6.0f, 1.0f);

    g.setColour(ivory.withAlpha(.90f));
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 10.0f, juce::Font::bold)));
    g.drawText("PEAK (dBFS)", (int)panel.getX() + 8, (int)panel.getY() + 56, 112, 16,
               juce::Justification::centred);
    auto display = juce::Rectangle<float>(panel.getX() + 11, panel.getY() + 73, 108, 38);
    g.setColour(juce::Colour(0xff080808));
    g.fillRoundedRectangle(display, 4.0f);
    g.setColour(juce::Colour(0xff0d0d0d));
    g.drawRoundedRectangle(display, 4.0f, .8f);
    g.setColour(amber);
    g.setFont(juce::Font(juce::FontOptions("Segoe UI", 20.0f, juce::Font::bold)));
    const auto text = peakHold <= -143.0f ? juce::String("-inf") : juce::String(peakHold, 2);
    g.drawText(text, display.toNearestInt(), juce::Justification::centred);

    drawMeter(g, {panel.getRight() - 70, panel.getY() + 18, 24, panel.getHeight() - 45}, leftPeak, "L");
    drawMeter(g, {panel.getRight() - 38, panel.getY() + 18, 24, panel.getHeight() - 45}, rightPeak, "R");
}
} // namespace scalpel::ui105

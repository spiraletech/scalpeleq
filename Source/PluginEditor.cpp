#include "PluginEditor.h"
#include <cmath>
#include <limits>

namespace
{
constexpr float kFloorDb = -144.0f;
constexpr std::array<float, PRISMVSTAudioProcessor::numCrossovers> kDefaultCrossovers {
    60.0f, 120.0f, 250.0f, 500.0f, 2000.0f, 6000.0f
};

float readParameter(const juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    if (auto* value = state.getRawParameterValue(id))
        return value->load();
    return 0.0f;
}

juce::Colour backgroundColour() { return juce::Colour::fromRGB(7, 9, 11); }
juce::Colour panelColour()      { return juce::Colour::fromRGB(13, 16, 19); }
juce::Colour raisedColour()     { return juce::Colour::fromRGB(20, 23, 26); }
juce::Colour lineColour()       { return juce::Colour::fromRGB(49, 56, 61); }
juce::Colour textMuted()        { return juce::Colour::fromRGB(143, 153, 161); }
juce::Colour accentColour()     { return juce::Colour::fromRGB(78, 201, 236); }

void drawPlasticPanel(juce::Graphics& g, juce::Rectangle<float> bounds,
                      float cornerRadius, bool raised)
{
    const auto top = raised ? juce::Colour::fromRGB(28, 31, 34)
                            : juce::Colour::fromRGB(20, 23, 26);
    const auto bottom = raised ? juce::Colour::fromRGB(13, 15, 17)
                               : juce::Colour::fromRGB(9, 11, 13);

    g.setColour(juce::Colours::black.withAlpha(0.52f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), cornerRadius);

    juce::ColourGradient plastic(top, bounds.getX(), bounds.getY(),
                                 bottom, bounds.getX(), bounds.getBottom(), false);
    plastic.addColour(0.20, top.brighter(0.035f));
    plastic.addColour(0.56, juce::Colour::fromRGB(18, 20, 22));
    g.setGradientFill(plastic);
    g.fillRoundedRectangle(bounds, cornerRadius);

    g.setColour(juce::Colours::white.withAlpha(0.045f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), cornerRadius - 1.0f, 1.0f);

    g.setColour(juce::Colour::fromRGB(2, 3, 4).withAlpha(0.95f));
    g.drawRoundedRectangle(bounds, cornerRadius, 1.2f);
}

void drawRecessedScrew(juce::Graphics& g, juce::Point<float> centre)
{
    const float r = 7.0f;
    g.setColour(juce::Colours::black.withAlpha(0.72f));
    g.fillEllipse(centre.x - r - 1.5f, centre.y - r + 1.5f, (r + 1.5f) * 2.0f, (r + 1.5f) * 2.0f);

    juce::ColourGradient screw(juce::Colour::fromRGB(48, 52, 55), centre.x - r, centre.y - r,
                               juce::Colour::fromRGB(9, 11, 12), centre.x + r, centre.y + r, false);
    g.setGradientFill(screw);
    g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);

    g.setColour(juce::Colour::fromRGB(86, 91, 94).withAlpha(0.72f));
    g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 0.8f);

    g.setColour(juce::Colour::fromRGB(3, 4, 5));
    g.drawLine(centre.x - 3.5f, centre.y, centre.x + 3.5f, centre.y, 1.4f);
    g.drawLine(centre.x, centre.y - 3.5f, centre.x, centre.y + 3.5f, 1.0f);
}

juce::Colour sectionColour(int section)
{
    const float hue = juce::jmap((float)juce::jlimit(0, PRISMVSTAudioProcessor::numSections - 1, section),
                                 0.0f, (float)(PRISMVSTAudioProcessor::numSections - 1),
                                 0.58f, 0.86f);
    return juce::Colour::fromHSV(hue, 0.48f, 0.92f, 1.0f);
}

juce::String formatFrequency(float frequency)
{
    if (frequency >= 10000.0f)
        return juce::String(frequency / 1000.0f, 1) + "k";
    if (frequency >= 1000.0f)
        return juce::String(frequency / 1000.0f, 2) + "k";
    return juce::String(frequency, frequency < 100.0f ? 1 : 0);
}
}

//==============================================================================
void PrecisionSlider::mouseDown(const juce::MouseEvent& e)
{
    dragStartProportion = valueToProportionOfLength(getValue());
    dragStartY = e.getScreenY();
    juce::Slider::mouseDown(e);
}

void PrecisionSlider::mouseDrag(const juce::MouseEvent& e)
{
    double travel = normalPixelsForFullRange;
    if (e.mods.isShiftDown())
        travel = finePixelsForFullRange;
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        travel = microPixelsForFullRange;

    const double delta = (double)(dragStartY - e.getScreenY());
    const double proportion = juce::jlimit(0.0, 1.0, dragStartProportion + delta / travel);
    setValue(proportionOfLengthToValue(proportion), juce::sendNotificationSync);
}

void PrecisionSlider::mouseUp(const juce::MouseEvent& e)
{
    juce::Slider::mouseUp(e);
}

//==============================================================================
void EtherTechLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                             int x, int y, int width, int height,
                                             float sliderPosProportional,
                                             float rotaryStartAngle,
                                             float rotaryEndAngle,
                                             juce::Slider&)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height)
                      .reduced(10.0f, 8.0f);
    const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto socket = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre()).reduced(3.0f);
    auto knob = socket.reduced(socket.getWidth() * 0.13f);

    const float angle = juce::jmap(sliderPosProportional, 0.0f, 1.0f,
                                   rotaryStartAngle, rotaryEndAngle);

    // Recessed molded-plastic socket.
    g.setColour(juce::Colours::black.withAlpha(0.78f));
    g.fillEllipse(socket.translated(0.0f, 2.0f));
    g.setColour(juce::Colour::fromRGB(24, 28, 31));
    g.fillEllipse(socket);
    g.setColour(juce::Colour::fromRGB(63, 69, 73).withAlpha(0.68f));
    g.drawEllipse(socket, 1.0f);

    // Plastic cap: satin sheen, no chrome / brushed-metal treatment.
    juce::ColourGradient cap(juce::Colour::fromRGB(43, 47, 50), knob.getX(), knob.getY(),
                             juce::Colour::fromRGB(8, 10, 12), knob.getRight(), knob.getBottom(), false);
    cap.addColour(0.34, juce::Colour::fromRGB(29, 33, 36));
    cap.addColour(0.70, juce::Colour::fromRGB(15, 18, 20));
    g.setGradientFill(cap);
    g.fillEllipse(knob);

    g.setColour(juce::Colours::white.withAlpha(0.075f));
    g.drawEllipse(knob.reduced(2.0f), 1.0f);

    auto arcBounds = socket.expanded(2.0f);
    juce::Path baseArc;
    baseArc.addCentredArc(arcBounds.getCentreX(), arcBounds.getCentreY(),
                          arcBounds.getWidth() * 0.5f, arcBounds.getHeight() * 0.5f,
                          0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour::fromRGB(43, 49, 53));
    g.strokePath(baseArc, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(arcBounds.getCentreX(), arcBounds.getCentreY(),
                           arcBounds.getWidth() * 0.5f, arcBounds.getHeight() * 0.5f,
                           0.0f, rotaryStartAngle, angle, true);
    g.setColour(accentColour());
    g.strokePath(valueArc, juce::PathStrokeType(2.7f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    juce::Path pointer;
    pointer.addRoundedRectangle(-1.2f, -knob.getHeight() * 0.35f,
                                2.4f, knob.getHeight() * 0.31f, 1.1f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle)
                               .translated(knob.getCentreX(), knob.getCentreY()));
    g.setColour(juce::Colour::fromRGB(239, 243, 245));
    g.fillPath(pointer);
}

void EtherTechLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                juce::Button& button,
                                                const juce::Colour&,
                                                bool highlighted,
                                                bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool selected = button.getToggleState();

    g.setColour(juce::Colours::black.withAlpha(0.58f));
    g.fillRoundedRectangle(b.translated(0.0f, 1.5f), 5.0f);

    const auto top = selected ? juce::Colour::fromRGB(29, 47, 55)
                              : juce::Colour::fromRGB(29, 32, 35);
    const auto bottom = selected ? juce::Colour::fromRGB(10, 26, 31)
                                 : juce::Colour::fromRGB(11, 13, 15);

    juce::ColourGradient plastic(top, b.getX(), b.getY(),
                                 bottom, b.getX(), b.getBottom(), false);
    if (highlighted)
        plastic.addColour(0.18, top.brighter(0.08f));

    g.setGradientFill(plastic);
    g.fillRoundedRectangle(b, 5.0f);

    g.setColour(juce::Colours::white.withAlpha(down ? 0.025f : 0.055f));
    g.drawRoundedRectangle(b.reduced(1.0f), 4.0f, 1.0f);

    g.setColour(selected ? accentColour()
                         : juce::Colour::fromRGB(58, 64, 68));
    g.drawRoundedRectangle(b, 5.0f, selected ? 1.4f : 1.0f);
}

void EtherTechLookAndFeel::drawButtonText(juce::Graphics& g,
                                          juce::TextButton& button,
                                          bool,
                                          bool)
{
    g.setColour(button.getToggleState() ? juce::Colours::white.withAlpha(0.96f)
                                        : juce::Colour::fromRGB(166, 177, 188));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(5, 2),
                     juce::Justification::centred, 1);
}

void EtherTechLookAndFeel::drawToggleButton(juce::Graphics& g,
                                            juce::ToggleButton& button,
                                            bool highlighted,
                                            bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = button.getToggleState();

    g.setColour(juce::Colours::black.withAlpha(0.60f));
    g.fillRoundedRectangle(b.translated(0.0f, 1.5f), 5.0f);

    const auto top = on ? juce::Colour::fromRGB(28, 52, 61)
                        : juce::Colour::fromRGB(27, 30, 33);
    const auto bottom = on ? juce::Colour::fromRGB(9, 27, 33)
                           : juce::Colour::fromRGB(10, 12, 14);

    juce::ColourGradient plastic(top, b.getX(), b.getY(),
                                 bottom, b.getX(), b.getBottom(), false);
    if (highlighted)
        plastic.addColour(0.18, top.brighter(0.08f));

    g.setGradientFill(plastic);
    g.fillRoundedRectangle(b, 5.0f);

    g.setColour(on ? accentColour()
                   : juce::Colour::fromRGB(58, 65, 69));
    g.drawRoundedRectangle(b, 5.0f, on ? 1.4f : 1.0f);

    g.setColour(on ? juce::Colours::white.withAlpha(0.96f) : textMuted());
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(6, 2),
                     juce::Justification::centred, 1);

    if (down)
    {
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillRoundedRectangle(b.reduced(2.0f), 4.0f);
    }
}

//==============================================================================
// Full-width analyzer. The engineering grid remains log-Hz / dB; lotus geometry
// is a visual layer only and never changes coordinates or DSP.
SpectrumAuraDisplay::SpectrumAuraDisplay(PRISMVSTAudioProcessor& p)
    : processor(p)
{
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    latestSpectrum.fill(kFloorDb);
    latestLabDb.fill(prism::analysis::LabBands::floorDb);
    latestLabPeakDb.fill(prism::analysis::LabBands::floorDb);
}

juce::Rectangle<float> SpectrumAuraDisplay::graphBounds() const
{
    return getLocalBounds().toFloat().reduced(10.0f, 8.0f);
}

float SpectrumAuraDisplay::parameter(const juce::String& id) const
{
    return readParameter(processor.apvts, id);
}

void SpectrumAuraDisplay::setParameter(const juce::String& id, float value)
{
    if (auto* p = processor.apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

float SpectrumAuraDisplay::frequencyToX(float frequency) const
{
    const auto b = graphBounds();
    const float f = juce::jlimit(20.0f, 20000.0f, frequency);
    const float norm = std::log10(f / 20.0f) / std::log10(1000.0f);
    return b.getX() + norm * b.getWidth();
}

float SpectrumAuraDisplay::xToFrequency(float x) const
{
    const auto b = graphBounds();
    const float norm = juce::jlimit(0.0f, 1.0f, (x - b.getX()) / b.getWidth());
    return 20.0f * std::pow(1000.0f, norm);
}

float SpectrumAuraDisplay::displayFloorDb() const
{
    const int mode = juce::roundToInt(parameter("analyzer_depth"));
    if (mode == 1) return -120.0f;
    if (mode >= 2) return -144.0f;
    return -96.0f;
}

float SpectrumAuraDisplay::levelToY(float db) const
{
    const auto b = graphBounds();
    const float floor = displayFloorDb();
    return juce::jmap(juce::jlimit(floor, 0.0f, db),
                      floor, 0.0f, b.getBottom(), b.getY());
}

float SpectrumAuraDisplay::yToLevel(float y) const
{
    const auto b = graphBounds();
    const float floor = displayFloorDb();
    return juce::jmap(juce::jlimit(b.getY(), b.getBottom(), y),
                      b.getBottom(), b.getY(), floor, 0.0f);
}

float SpectrumAuraDisplay::crossoverFrequency(int index) const
{
    return parameter(PRISMVSTAudioProcessor::crossoverId(index));
}

float SpectrumAuraDisplay::clampedCrossoverFrequency(int index, float frequency) const
{
    constexpr float spacing = 1.08f;
    float low = 20.0f * spacing;
    float high = 20000.0f / spacing;

    if (index > 0)
        low = crossoverFrequency(index - 1) * spacing;
    if (index < PRISMVSTAudioProcessor::numCrossovers - 1)
        high = crossoverFrequency(index + 1) / spacing;

    return juce::jlimit(low, high, frequency);
}

int SpectrumAuraDisplay::findCrossoverAt(juce::Point<float> point) const
{
    if (!graphBounds().contains(point))
        return -1;

    int hit = -1;
    float best = 11.0f;
    for (int i = 0; i < PRISMVSTAudioProcessor::numCrossovers; ++i)
    {
        const float distance = std::abs(point.x - frequencyToX(crossoverFrequency(i)));
        if (distance < best)
        {
            best = distance;
            hit = i;
        }
    }
    return hit;
}

int SpectrumAuraDisplay::sectionForX(float x) const
{
    const float f = xToFrequency(x);
    for (int i = 0; i < PRISMVSTAudioProcessor::numCrossovers; ++i)
        if (f < crossoverFrequency(i))
            return i;
    return PRISMVSTAudioProcessor::numSections - 1;
}

float SpectrumAuraDisplay::spectrumDbAt(float frequency) const
{
    const float norm = juce::jlimit(
        0.0f, 1.0f,
        std::log10(juce::jlimit(20.0f, 20000.0f, frequency) / 20.0f)
            / std::log10(1000.0f));

    const int bin = juce::jlimit(
        0, PRISMVSTAudioProcessor::spectrumBins - 1,
        juce::roundToInt(norm * (PRISMVSTAudioProcessor::spectrumBins - 1)));

    return latestSpectrum[(size_t)bin];
}

float SpectrumAuraDisplay::displayedSpectrumDbAt(float frequency) const
{
    // True spectrum display: no octave compensation / tilt.
    // This keeps the analyzer from looking bent or artificially zoomed.
    return spectrumDbAt(frequency);
}

void SpectrumAuraDisplay::drawLotusGrid(juce::Graphics& g, juce::Rectangle<float> b) const
{
    const auto centre = b.getCentre();
    const float petalW = b.getWidth() * 0.24f;
    const float petalH = b.getHeight() * 0.74f;

    g.setColour(juce::Colour::fromRGB(67, 79, 91).withAlpha(0.13f));

    for (int i = 0; i < 8; ++i)
    {
        juce::Path petal;
        petal.addEllipse(centre.x - petalW * 0.5f,
                         centre.y - petalH * 0.5f,
                         petalW, petalH);
        const float angle = juce::MathConstants<float>::twoPi * (float)i / 8.0f;
        g.strokePath(petal, juce::PathStrokeType(0.8f),
                     juce::AffineTransform::rotation(angle, centre.x, centre.y));
    }

    g.setColour(juce::Colour::fromRGB(77, 91, 104).withAlpha(0.10f));
    g.drawEllipse(b.withSizeKeepingCentre(b.getWidth() * 0.42f, b.getHeight() * 0.72f), 0.8f);
    g.drawEllipse(b.withSizeKeepingCentre(b.getWidth() * 0.22f, b.getHeight() * 0.42f), 0.8f);
}

void SpectrumAuraDisplay::pushSpectrum(
    const std::array<float, PRISMVSTAudioProcessor::spectrumBins>& values)
{
    // Fast attack keeps the analyzer feeling immediate. A gentler release
    // removes frame-to-frame chatter without delaying new spectral events.
    for (int i = 0; i < PRISMVSTAudioProcessor::spectrumBins; ++i)
    {
        const float target = values[(size_t)i];
        const float current = latestSpectrum[(size_t)i];
        const float alpha = target > current ? 0.82f : 0.28f;
        latestSpectrum[(size_t)i] = current + alpha * (target - current);
    }

    // Tiny spatial smoothing makes the crest read as one continuous PRISM
    // surface without changing its frequency coordinate system.
    auto smoothed = latestSpectrum;
    for (int i = 1; i < PRISMVSTAudioProcessor::spectrumBins - 1; ++i)
        smoothed[(size_t)i] =
            0.20f * latestSpectrum[(size_t)(i - 1)]
          + 0.60f * latestSpectrum[(size_t)i]
          + 0.20f * latestSpectrum[(size_t)(i + 1)];

    latestSpectrum = smoothed;
    repaint();
}

void SpectrumAuraDisplay::pushPrismLabBars(
    const std::array<float, PRISMVSTAudioProcessor::labBars>& db,
    const std::array<float, PRISMVSTAudioProcessor::labBars>& peakDb)
{
    // These values were smoothed by the analyzer worker, never guessed
    // from painted pixels or re-smoothed with a different time base.
    latestLabDb = db;
    latestLabPeakDb = peakDb;
    repaint();
}

void SpectrumAuraDisplay::setSelectedSection(int section)
{
    selectedSection = juce::jlimit(0, PRISMVSTAudioProcessor::numSections - 1, section);
    repaint();
}

void SpectrumAuraDisplay::paint(juce::Graphics& g)
{
    const auto b = graphBounds();
    const float floor = displayFloorDb();

    juce::ColourGradient glass(
        juce::Colour::fromRGB(16, 21, 27), b.getX(), b.getY(),
        juce::Colour::fromRGB(5, 7, 10), b.getX(), b.getBottom(), false);
    g.setGradientFill(glass);
    g.fillRoundedRectangle(b, 8.0f);

    g.setColour(accentColour().withAlpha(0.045f));
    g.fillRoundedRectangle(b.reduced(1.0f), 7.0f);

    const float selectedLeft = selectedSection == 0
        ? b.getX() : frequencyToX(crossoverFrequency(selectedSection - 1));
    const float selectedRight = selectedSection == PRISMVSTAudioProcessor::numSections - 1
        ? b.getRight() : frequencyToX(crossoverFrequency(selectedSection));

    g.setColour(sectionColour(selectedSection).withAlpha(0.055f));
    g.fillRect(juce::Rectangle<float>(selectedLeft, b.getY(),
                                      juce::jmax(1.0f, selectedRight - selectedLeft), b.getHeight()));

    // Genuine stereo side-energy activity in the selected territory.
    // This is measured from the processed L/R signal, not inferred from
    // the WIDTH knob. The analyzer's -96 dB scale and spectrum are untouched.
    const float stereoActivity = juce::jlimit(0.0f, 1.0f,
                                              processor.getStereoActivity(selectedSection));
    if (stereoActivity > 0.012f)
    {
        const juce::Graphics::ScopedSaveState savedState(g);
        g.reduceClipRegion(juce::Rectangle<int>(
            juce::roundToInt(selectedLeft), juce::roundToInt(b.getY()),
            juce::jmax(1, juce::roundToInt(selectedRight - selectedLeft)),
            juce::roundToInt(b.getHeight())));

        const float cx = 0.5f * (selectedLeft + selectedRight);
        const float cy = b.getCentreY();
        const float spread = juce::jmin(selectedRight - selectedLeft, b.getHeight());
        const float radiusX = 12.0f + spread * (0.16f + 0.36f * stereoActivity);
        const float radiusY = 14.0f + b.getHeight() * (0.12f + 0.36f * stereoActivity);
        juce::ColourGradient field(
            accentColour().withAlpha(0.035f + 0.13f * stereoActivity), cx, cy,
            accentColour().withAlpha(0.0f), cx + radiusX, cy, true);
        g.setGradientFill(field);
        g.fillEllipse(cx - radiusX, cy - radiusY, radiusX * 2.0f, radiusY * 2.0f);
    }

    drawLotusGrid(g, b);

    // Original PRISM Analysis 256-bar visual language, now driven by
    // coherent-gain calibrated FFT magnitudes. Audio processor is unaffected.
    for (int i = 0; i < PRISMVSTAudioProcessor::labBars; ++i)
    {
        const float x = b.getX() + (i + 0.5f) * b.getWidth()
                        / (float)PRISMVSTAudioProcessor::labBars;
        const float topY = levelToY(latestLabDb[(size_t)i]);
        const float peakY = levelToY(latestLabPeakDb[(size_t)i]);
        if (topY < b.getBottom() - 1.0f)
        {
            g.setColour(accentColour().withAlpha(0.19f));
            g.drawVerticalLine(juce::roundToInt(x), topY, b.getBottom());
        }
        if (peakY < b.getBottom() - 1.0f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.37f));
            g.drawHorizontalLine(juce::roundToInt(peakY), x - 1.0f, x + 1.0f);
        }
    }

    const std::array<float, 10> gridFreq {
        20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
        1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f
    };

    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    for (float f : gridFreq)
    {
        const float x = frequencyToX(f);
        g.setColour(lineColour().withAlpha(0.42f));
        g.drawVerticalLine(juce::roundToInt(x), b.getY(), b.getBottom());

        g.setColour(textMuted());
        g.drawText(formatFrequency(f), juce::roundToInt(x - 23.0f),
                   juce::roundToInt(b.getBottom() - 16.0f),
                   46, 13, juce::Justification::centred);
    }

    for (int i = 0; i <= 8; ++i)
    {
        const float db = -12.0f * (float)i;
        const float y = levelToY(db);
        g.setColour(lineColour().withAlpha(i == 0 ? 0.72f : 0.26f));
        g.drawHorizontalLine(juce::roundToInt(y), b.getX(), b.getRight());
        g.setColour(textMuted());
        g.drawText(juce::String(db, 0),
                   juce::roundToInt(b.getX() + 4.0f), juce::roundToInt(y - 7.0f),
                   38, 13, juce::Justification::centredLeft);
    }

    juce::Path spectrumPath;
    bool started = false;
    for (int px = 0; px <= juce::roundToInt(b.getWidth()); px += 2)
    {
        const float x = b.getX() + (float)px;
        const float frequency = xToFrequency(x);
        const float y = levelToY(juce::jlimit(floor, 0.0f, displayedSpectrumDbAt(frequency)));

        if (!started)
        {
            spectrumPath.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            spectrumPath.lineTo(x, y);
        }
    }

    // PRISM crest: restrained aura around a precise line. This is not a heat field.
    g.setColour(accentColour().withAlpha(0.10f));
    g.strokePath(spectrumPath, juce::PathStrokeType(5.0f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    g.setColour(accentColour().withAlpha(0.34f));
    g.strokePath(spectrumPath, juce::PathStrokeType(2.6f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    g.setColour(juce::Colour::fromRGB(232, 240, 244).withAlpha(0.96f));
    g.strokePath(spectrumPath, juce::PathStrokeType(1.15f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

    // Six fixed-slope crossover boundaries.
    for (int i = 0; i < PRISMVSTAudioProcessor::numCrossovers; ++i)
    {
        const float f = crossoverFrequency(i);
        const float x = frequencyToX(f);
        const bool hot = i == draggingCrossover;

        g.setColour(hot ? juce::Colours::white.withAlpha(0.95f)
                        : accentColour().withAlpha(0.78f));
        g.drawVerticalLine(juce::roundToInt(x), b.getY(), b.getBottom());

        g.fillRoundedRectangle(x - 4.0f, b.getY() + 4.0f, 8.0f, 18.0f, 3.0f);

        g.setColour(juce::Colour::fromRGB(8, 11, 14).withAlpha(0.96f));
        g.fillRoundedRectangle(x - 28.0f, b.getY() + 25.0f, 56.0f, 18.0f, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
        g.drawText(formatFrequency(f) + " Hz",
                   juce::roundToInt(x - 28.0f), juce::roundToInt(b.getY() + 27.0f),
                   56, 14, juce::Justification::centred);
    }

    // Dim sections that are explicitly OFF.
    for (int s = 0; s < PRISMVSTAudioProcessor::numSections; ++s)
    {
        if (parameter(PRISMVSTAudioProcessor::sectionId(s, "on")) >= 0.5f)
            continue;

        const float x1 = s == 0 ? b.getX() : frequencyToX(crossoverFrequency(s - 1));
        const float x2 = s == PRISMVSTAudioProcessor::numSections - 1
            ? b.getRight() : frequencyToX(crossoverFrequency(s));

        g.setColour(juce::Colour::fromRGB(3, 4, 6).withAlpha(0.58f));
        g.fillRect(x1, b.getY(), juce::jmax(1.0f, x2 - x1), b.getHeight());
        g.setColour(juce::Colour::fromRGB(184, 94, 100).withAlpha(0.82f));
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText("OFF", juce::roundToInt(x1), juce::roundToInt(b.getCentreY() - 8.0f),
                   juce::roundToInt(x2 - x1), 16, juce::Justification::centred);
    }

    if (hasHover && b.contains(hoverPoint))
    {
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.drawVerticalLine(juce::roundToInt(hoverPoint.x), b.getY(), b.getBottom());
        g.drawHorizontalLine(juce::roundToInt(hoverPoint.y), b.getX(), b.getRight());

        const float frequency = xToFrequency(hoverPoint.x);
        const int bar = prism::analysis::LabBands::indexForFrequency(frequency);
        const float db = latestLabDb[(size_t)bar];
        const float held = latestLabPeakDb[(size_t)bar];
        // Labels are measured peak FFT-bin dBFS, not vertical cursor pixels.
        // Cursor Hz denotes the nominal spectral position.
        const juce::String info =
            formatFrequency(frequency) + " Hz  BAR " + juce::String(db, 1)
            + " dBFS  PEAK " + juce::String(held, 1) + " dBFS";

        const float boxW = 286.0f;
        const float bx = juce::jlimit(b.getX(), b.getRight() - boxW, hoverPoint.x + 12.0f);
        const float by = juce::jlimit(b.getY(), b.getBottom() - 28.0f, hoverPoint.y - 34.0f);

        g.setColour(juce::Colour::fromRGB(5, 7, 10).withAlpha(0.96f));
        g.fillRoundedRectangle(bx, by, boxW, 24.0f, 5.0f);
        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawFittedText(info, juce::roundToInt(bx + 7.0f), juce::roundToInt(by + 4.0f),
                         (int)boxW - 14, 16, juce::Justification::centredLeft, 1);
    }

    g.setColour(juce::Colour::fromRGB(77, 89, 101));
    g.drawRoundedRectangle(b, 8.0f, 1.0f);
}

void SpectrumAuraDisplay::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showContextMenu(e.getPosition());
        return;
    }

    draggingCrossover = findCrossoverAt(e.position);
    if (draggingCrossover >= 0)
    {
        dragStartFrequency = crossoverFrequency(draggingCrossover);
        dragStartX = e.position.x;
        if (auto* p = processor.apvts.getParameter(PRISMVSTAudioProcessor::crossoverId(draggingCrossover)))
            p->beginChangeGesture();
        return;
    }

    if (graphBounds().contains(e.position))
    {
        selectedSection = sectionForX(e.position.x);
        if (onSectionSelected)
            onSectionSelected(selectedSection);
        repaint();
    }
}

void SpectrumAuraDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingCrossover < 0)
        return;

    float travel = 3000.0f;
    if (e.mods.isShiftDown())
        travel = 9000.0f;
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        travel = 24000.0f;

    const float startNorm = std::log10(dragStartFrequency / 20.0f) / std::log10(1000.0f);
    const float norm = juce::jlimit(0.0f, 1.0f,
                                    startNorm + (e.position.x - dragStartX) / travel);
    const float proposed = 20.0f * std::pow(1000.0f, norm);
    setParameter(PRISMVSTAudioProcessor::crossoverId(draggingCrossover),
                 clampedCrossoverFrequency(draggingCrossover, proposed));
    repaint();
}

void SpectrumAuraDisplay::mouseUp(const juce::MouseEvent&)
{
    if (draggingCrossover >= 0)
        if (auto* p = processor.apvts.getParameter(PRISMVSTAudioProcessor::crossoverId(draggingCrossover)))
            p->endChangeGesture();

    draggingCrossover = -1;
}

void SpectrumAuraDisplay::mouseMove(const juce::MouseEvent& e)
{
    hoverPoint = e.position;
    hasHover = graphBounds().contains(e.position);
    repaint();
}

void SpectrumAuraDisplay::mouseExit(const juce::MouseEvent&)
{
    hasHover = false;
    repaint();
}

void SpectrumAuraDisplay::resetCrossovers()
{
    for (int i = 0; i < PRISMVSTAudioProcessor::numCrossovers; ++i)
        setParameter(PRISMVSTAudioProcessor::crossoverId(i), kDefaultCrossovers[(size_t)i]);
}

void SpectrumAuraDisplay::showContextMenu(juce::Point<int>)
{
    juce::PopupMenu menu;
    const bool on = parameter(PRISMVSTAudioProcessor::sectionId(selectedSection, "on")) >= 0.5f;
    const bool solo = parameter(PRISMVSTAudioProcessor::sectionId(selectedSection, "solo")) >= 0.5f;
    const int depth = juce::roundToInt(parameter("analyzer_depth"));

    menu.addSectionHeader(PRISMVSTAudioProcessor::sectionName(selectedSection));
    menu.addItem(10, "Section ON", true, on);
    menu.addItem(11, "Section SOLO", true, solo);
    menu.addSeparator();
    menu.addItem(1, "Reset crossovers");
    menu.addSeparator();
    menu.addSectionHeader("Analyzer depth");
    menu.addItem(20, "FULL 0 to -96 dB", true, depth == 0);
    menu.addItem(21, "DEEP 0 to -120 dB", true, depth == 1);
    menu.addItem(22, "FORENSIC 0 to -144 dB", true, depth == 2);

    juce::Component::SafePointer<SpectrumAuraDisplay> safe(this);
    const int section = selectedSection;

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                       [safe, section](int result)
                       {
                           if (safe == nullptr || result == 0)
                               return;

                           if (result == 1)
                               safe->resetCrossovers();
                           else if (result == 10)
                               safe->setParameter(PRISMVSTAudioProcessor::sectionId(section, "on"),
                                                  safe->parameter(PRISMVSTAudioProcessor::sectionId(section, "on")) >= 0.5f ? 0.0f : 1.0f);
                           else if (result == 11)
                               safe->setParameter(PRISMVSTAudioProcessor::sectionId(section, "solo"),
                                                  safe->parameter(PRISMVSTAudioProcessor::sectionId(section, "solo")) >= 0.5f ? 0.0f : 1.0f);
                           else if (result >= 20 && result <= 22)
                               safe->setParameter("analyzer_depth", (float)(result - 20));

                           safe->repaint();
                       });
}

//==============================================================================
// Maximus-style dB-in -> dB-out transfer map. Threshold / ratio remain explicit,
// but the magenta transfer line itself is now a direct manipulation surface:
// grab the line and drag it vertically to bend the curve under the mouse.
DynamicsTransferDisplay::DynamicsTransferDisplay(PRISMVSTAudioProcessor& p)
    : processor(p)
{
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

juce::Rectangle<float> DynamicsTransferDisplay::graphBounds() const
{
    return getLocalBounds().toFloat().reduced(14.0f, 12.0f);
}

juce::String DynamicsTransferDisplay::id(const juce::String& suffix) const
{
    return PRISMVSTAudioProcessor::sectionId(selectedSection, suffix);
}

float DynamicsTransferDisplay::parameter(const juce::String& suffix) const
{
    return readParameter(processor.apvts, id(suffix));
}

juce::RangedAudioParameter* DynamicsTransferDisplay::rangedParameter(const juce::String& suffix) const
{
    return processor.apvts.getParameter(id(suffix));
}

void DynamicsTransferDisplay::setParameter(const juce::String& suffix, float value)
{
    if (auto* p = rangedParameter(suffix))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

float DynamicsTransferDisplay::dbToX(float db) const
{
    const auto b = graphBounds();
    return juce::jmap(juce::jlimit(-60.0f, 0.0f, db), -60.0f, 0.0f, b.getX(), b.getRight());
}

float DynamicsTransferDisplay::dbToY(float db) const
{
    const auto b = graphBounds();
    return juce::jmap(juce::jlimit(-60.0f, 0.0f, db), -60.0f, 0.0f, b.getBottom(), b.getY());
}

float DynamicsTransferDisplay::xToDb(float x) const
{
    const auto b = graphBounds();
    return juce::jmap(juce::jlimit(b.getX(), b.getRight(), x),
                      b.getX(), b.getRight(), -60.0f, 0.0f);
}

float DynamicsTransferDisplay::yToDb(float y) const
{
    const auto b = graphBounds();
    return juce::jmap(juce::jlimit(b.getY(), b.getBottom(), y),
                      b.getBottom(), b.getY(), -60.0f, 0.0f);
}

float DynamicsTransferDisplay::outputDbForInput(float inputDb) const
{
    return outputDbForInput(
        inputDb,
        juce::jlimit(1.0f, 20.0f, parameter("ratio")),
        juce::jlimit(-1.0f, 1.0f, parameter("curve")));
}

float DynamicsTransferDisplay::outputDbForInput(float inputDb,
                                                 float ratio,
                                                 float curve) const
{
    const float threshold = parameter("threshold_db");
    if (inputDb <= threshold)
        return inputDb;

    ratio = juce::jlimit(1.0f, 20.0f, ratio);
    curve = juce::jlimit(-1.0f, 1.0f, curve);

    const float over = inputDb - threshold;
    const float exponent = juce::jmap(curve, -1.0f, 1.0f, 0.65f, 1.75f);
    const float normalisedOver = juce::jlimit(0.0f, 1.0f, over / 24.0f);
    const float shaped = 24.0f * std::pow(normalisedOver, exponent);
    return inputDb - shaped * (1.0f - 1.0f / ratio);
}

juce::Point<float> DynamicsTransferDisplay::thresholdPoint() const
{
    const float threshold = parameter("threshold_db");
    return { dbToX(threshold), dbToY(threshold) };
}

juce::Point<float> DynamicsTransferDisplay::ratioPoint() const
{
    const float threshold = parameter("threshold_db");
    const float inputDb = juce::jmin(0.0f, threshold + 18.0f);
    return { dbToX(inputDb), dbToY(outputDbForInput(inputDb)) };
}

juce::Point<float> DynamicsTransferDisplay::curvePoint() const
{
    const float threshold = parameter("threshold_db");
    const float inputDb = juce::jmap(0.58f, threshold, 0.0f);
    return { dbToX(inputDb), dbToY(outputDbForInput(inputDb)) };
}

bool DynamicsTransferDisplay::isCurveNear(juce::Point<float> point) const
{
    if (!graphBounds().contains(point))
        return false;

    const float inputDb = xToDb(point.x);
    const float threshold = parameter("threshold_db");
    if (inputDb <= threshold + 0.75f)
        return false;

    const float expectedY = dbToY(outputDbForInput(inputDb));
    return std::abs(point.y - expectedY) <= 15.0f;
}

float DynamicsTransferDisplay::solveCurveForPoint(float inputDb,
                                                   float targetOutputDb,
                                                   float ratio) const
{
    float bestCurve = parameter("curve");
    float bestError = std::numeric_limits<float>::max();

    for (int i = 0; i <= 256; ++i)
    {
        const float candidate = -1.0f + 2.0f * (float)i / 256.0f;
        const float output = outputDbForInput(inputDb, ratio, candidate);
        const float error = std::abs(output - targetOutputDb);

        if (error < bestError)
        {
            bestError = error;
            bestCurve = candidate;
        }
    }

    return bestCurve;
}

float DynamicsTransferDisplay::solveRatioForPoint(float inputDb,
                                                   float targetOutputDb,
                                                   float curve) const
{
    const float threshold = parameter("threshold_db");
    if (inputDb <= threshold)
        return 1.0f;

    const float targetReduction = juce::jmax(0.0f, inputDb - targetOutputDb);
    if (targetReduction <= 0.001f)
        return 1.0f;

    const float over = inputDb - threshold;
    const float exponent = juce::jmap(
        juce::jlimit(-1.0f, 1.0f, curve),
        -1.0f, 1.0f, 0.65f, 1.75f);

    const float normalisedOver = juce::jlimit(0.0f, 1.0f, over / 24.0f);
    const float shaped = 24.0f * std::pow(normalisedOver, exponent);

    if (shaped <= 0.001f)
        return 1.0f;

    const float factor = juce::jlimit(0.0f, 0.95f, targetReduction / shaped);
    return juce::jlimit(1.0f, 20.0f, 1.0f / juce::jmax(0.05f, 1.0f - factor));
}

float DynamicsTransferDisplay::dragScale(const juce::ModifierKeys& mods) const
{
    if (mods.isCtrlDown() || mods.isCommandDown())
        return 24000.0f;
    if (mods.isShiftDown())
        return 9000.0f;
    return 3000.0f;
}

void DynamicsTransferDisplay::setSelectedSection(int section)
{
    selectedSection = juce::jlimit(0, PRISMVSTAudioProcessor::numSections - 1, section);
    repaint();
}

void DynamicsTransferDisplay::paint(juce::Graphics& g)
{
    const auto b = graphBounds();
    g.setColour(panelColour());
    g.fillRoundedRectangle(b, 7.0f);

    for (int db : { -60, -48, -36, -24, -12, 0 })
    {
        const float x = dbToX((float)db);
        const float y = dbToY((float)db);
        g.setColour(lineColour().withAlpha(0.30f));
        g.drawVerticalLine(juce::roundToInt(x), b.getY(), b.getBottom());
        g.drawHorizontalLine(juce::roundToInt(y), b.getX(), b.getRight());
    }

    g.setColour(juce::Colour::fromRGB(102, 113, 125).withAlpha(0.52f));
    g.drawLine(b.getX(), b.getBottom(), b.getRight(), b.getY(), 1.0f);

    juce::Path curvePath;
    bool started = false;
    for (int px = 0; px <= juce::roundToInt(b.getWidth()); px += 2)
    {
        const float inputDb = xToDb(b.getX() + (float)px);
        const float outputDb = outputDbForInput(inputDb);
        const float x = dbToX(inputDb);
        const float y = dbToY(outputDb);

        if (!started)
        {
            curvePath.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            curvePath.lineTo(x, y);
        }
    }

    const auto colour = sectionColour(selectedSection);
    g.setColour(colour.withAlpha(0.94f));
    g.strokePath(curvePath, juce::PathStrokeType(2.2f));

    const auto threshold = thresholdPoint();
    const auto ratio = ratioPoint();
    const auto bend = curvePoint();

    g.setColour(colour);
    g.fillEllipse(threshold.x - 6.0f, threshold.y - 6.0f, 12.0f, 12.0f);

    g.setColour(juce::Colours::white.withAlpha(0.94f));
    g.fillEllipse(ratio.x - 5.0f, ratio.y - 5.0f, 10.0f, 10.0f);

    g.setColour(curveHover || dragTarget == DragTarget::curve
                    ? juce::Colours::white.withAlpha(0.96f)
                    : colour.withAlpha(0.90f));
    g.drawEllipse(bend.x - 6.5f, bend.y - 6.5f, 13.0f, 13.0f, 1.8f);

    const juce::String info =
        juce::String(PRISMVSTAudioProcessor::sectionName(selectedSection))
        + " TRANSFER   THR " + juce::String(parameter("threshold_db"), 1) + " dB"
        + "   RATIO " + juce::String(parameter("ratio"), 2) + ":1"
        + "   CURVE " + juce::String(parameter("curve"), 2)
        + "   • drag magenta line to bend   • wheel = fine";

    g.setColour(textMuted());
    g.setFont(juce::Font(juce::FontOptions(9.2f, juce::Font::bold)));
    g.drawFittedText(info,
                     juce::roundToInt(b.getX() + 8.0f), juce::roundToInt(b.getY() + 4.0f),
                     juce::roundToInt(b.getWidth() - 16.0f), 14,
                     juce::Justification::centred, 1);

    g.setColour(juce::Colour::fromRGB(78, 90, 102));
    g.drawRoundedRectangle(b, 7.0f, 1.0f);
}

void DynamicsTransferDisplay::mouseDown(const juce::MouseEvent& e)
{
    const float thresholdDistance = e.position.getDistanceFrom(thresholdPoint());
    const float ratioDistance = e.position.getDistanceFrom(ratioPoint());

    if (thresholdDistance <= 16.0f)
    {
        dragTarget = DragTarget::threshold;
        dragStartValue = parameter("threshold_db");
        dragStartPixel = e.position.x;
        if (auto* p = rangedParameter("threshold_db"))
            p->beginChangeGesture();
    }
    else if (ratioDistance <= 16.0f)
    {
        dragTarget = DragTarget::ratio;
        dragStartValue = parameter("ratio");
        dragStartPixel = e.position.y;
        if (auto* p = rangedParameter("ratio"))
            p->beginChangeGesture();
    }
    else if (isCurveNear(e.position))
    {
        dragTarget = DragTarget::curve;
        dragStartValue = parameter("curve");
        dragStartPixel = e.position.y;
        dragAnchorInputDb = xToDb(e.position.x);

        if (auto* p = rangedParameter("curve"))
            p->beginChangeGesture();
        if (auto* p = rangedParameter("ratio"))
            p->beginChangeGesture();
    }
    else
    {
        dragTarget = DragTarget::none;
    }

    repaint();
}

void DynamicsTransferDisplay::mouseDrag(const juce::MouseEvent& e)
{
    const float scale = dragScale(e.mods);

    if (dragTarget == DragTarget::threshold)
    {
        const float delta = (e.position.x - dragStartPixel) * 60.0f / scale;
        setParameter("threshold_db", juce::jlimit(-60.0f, 0.0f, dragStartValue + delta));
    }
    else if (dragTarget == DragTarget::ratio)
    {
        const float delta = (dragStartPixel - e.position.y) * 19.0f / scale;
        setParameter("ratio", juce::jlimit(1.0f, 20.0f, dragStartValue + delta));
    }
    else if (dragTarget == DragTarget::curve)
    {
        float targetOutputDb = yToDb(e.position.y);
        targetOutputDb = juce::jmin(dragAnchorInputDb, targetOutputDb);

        float ratio = parameter("ratio");
        const float curve = parameter("curve");

        // Neutral 1:1 used to make CURVE look broken. The first downward drag
        // now creates the minimum ratio required to make the grabbed point move.
        if (ratio <= 1.001f && targetOutputDb < dragAnchorInputDb - 0.05f)
        {
            ratio = solveRatioForPoint(dragAnchorInputDb, targetOutputDb, curve);
            setParameter("ratio", ratio);
        }

        if (ratio > 1.001f)
        {
            float solvedCurve = solveCurveForPoint(
                dragAnchorInputDb, targetOutputDb, ratio);

            // Modifier keys retain PRISM's deliberate precision law.
            if (e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown())
            {
                const float fineScale =
                    (e.mods.isCtrlDown() || e.mods.isCommandDown()) ? 0.12f : 0.35f;
                solvedCurve = juce::jlimit(
                    -1.0f, 1.0f,
                    dragStartValue + (solvedCurve - dragStartValue) * fineScale);
            }

            setParameter("curve", solvedCurve);
        }
    }

    repaint();
}

void DynamicsTransferDisplay::mouseUp(const juce::MouseEvent&)
{
    if (dragTarget == DragTarget::threshold)
        if (auto* p = rangedParameter("threshold_db"))
            p->endChangeGesture();

    if (dragTarget == DragTarget::ratio)
        if (auto* p = rangedParameter("ratio"))
            p->endChangeGesture();

    if (dragTarget == DragTarget::curve)
    {
        if (auto* p = rangedParameter("curve"))
            p->endChangeGesture();
        if (auto* p = rangedParameter("ratio"))
            p->endChangeGesture();
    }

    dragTarget = DragTarget::none;
    repaint();
}

void DynamicsTransferDisplay::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.position.getDistanceFrom(thresholdPoint()) <= 16.0f)
        setParameter("threshold_db", -18.0f);
    else if (e.position.getDistanceFrom(ratioPoint()) <= 16.0f)
        setParameter("ratio", 1.0f);
    else if (isCurveNear(e.position))
        setParameter("curve", 0.0f);

    repaint();
}

void DynamicsTransferDisplay::mouseMove(const juce::MouseEvent& e)
{
    curveHover = isCurveNear(e.position);
    setMouseCursor(curveHover
        ? juce::MouseCursor::UpDownResizeCursor
        : juce::MouseCursor::CrosshairCursor);
    repaint();
}

void DynamicsTransferDisplay::mouseExit(const juce::MouseEvent&)
{
    curveHover = false;
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    repaint();
}

void DynamicsTransferDisplay::mouseWheelMove(const juce::MouseEvent&,
                                              const juce::MouseWheelDetails& wheel)
{
    const float current = parameter("curve");
    setParameter("curve", juce::jlimit(-1.0f, 1.0f, current + wheel.deltaY * 0.12f));
    repaint();
}

//==============================================================================
PRISMVSTAudioProcessorEditor::PRISMVSTAudioProcessorEditor(PRISMVSTAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      spectrumDisplay(p)
{
    setSize(1360, 900);
    setResizable(true, true);
    setResizeLimits(1120, 760, 1900, 1250);
    setLookAndFeel(&lookAndFeel);

    lookAndFeel.setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(219, 226, 232));
    lookAndFeel.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB(7, 10, 13));
    lookAndFeel.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(42, 50, 59));

    addAndMakeVisible(spectrumDisplay);

    spectrumDisplay.onSectionSelected = [this](int section)
    {
        bindSelectedSection(section);
    };

    for (int i = 0; i < PRISMVSTAudioProcessor::numSections; ++i)
    {
        auto& button = sectionButtons[(size_t)i];
        button.setButtonText(PRISMVSTAudioProcessor::sectionName(i));
        button.setClickingTogglesState(true);
        button.setRadioGroupId(101);
        button.onClick = [this, i] { bindSelectedSection(i); };
        addAndMakeVisible(button);
    }

    selectedSectionLabel.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    selectedSectionLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(174, 185, 196));
    selectedSectionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(selectedSectionLabel);

    addAndMakeVisible(sectionOn);
    addAndMakeVisible(sectionSolo);

    const std::array<const char*, controlCount> names {
        "INPUT", "OUTPUT", "ATTACK", "RELEASE", "WIDTH", "ONYX"
    };
    const std::array<PrecisionSlider*, controlCount> sliders {
        &input, &output, &attack, &release, &width, &onyx
    };
    const std::array<juce::String, controlCount> suffixes {
        " dB", " dB", " ms", " ms", " %", " %"
    };
    const std::array<int, controlCount> decimals { 2, 2, 2, 1, 1, 1 };

    for (int i = 0; i < controlCount; ++i)
    {
        configureRotary(*sliders[(size_t)i], suffixes[(size_t)i], decimals[(size_t)i]);
        addAndMakeVisible(*sliders[(size_t)i]);

        controlLabels[(size_t)i].setText(names[(size_t)i], juce::dontSendNotification);
        controlLabels[(size_t)i].setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        controlLabels[(size_t)i].setColour(juce::Label::textColourId, textMuted());
        controlLabels[(size_t)i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(controlLabels[(size_t)i]);
    }

    // The existing WIDTH knob shows only additional widening (0..50%).
    // Underlying width_pct remains 100..150 for compatibility with v1.1.1.
    width.setRange(0.0, 50.0, 0.1);
    width.setDoubleClickReturnValue(true, 0.0);
    width.onDragStart = [this]
    {
        widthDragging = true;
        if (widthParameter != nullptr)
            widthParameter->beginChangeGesture();
    };
    width.onDragEnd = [this]
    {
        if (widthParameter != nullptr)
            widthParameter->endChangeGesture();
        widthDragging = false;
    };
    width.onValueChange = [this]
    {
        if (widthParameter != nullptr)
        {
            const float stored = (float)(100.0 + width.getValue());
            widthParameter->setValueNotifyingHost(widthParameter->convertTo0to1(stored));
        }
    };

    for (auto* label : { &inputPeakLabel, &grLabel, &outputPeakLabel })
    {
        label->setFont(juce::Font(juce::FontOptions(9.7f, juce::Font::bold)));
        label->setColour(juce::Label::textColourId, juce::Colour::fromRGB(176, 187, 198));
        label->setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(*label);
    }

    bindSelectedSection(0);
    startTimerHz(60);
}

PRISMVSTAudioProcessorEditor::~PRISMVSTAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void PRISMVSTAudioProcessorEditor::configureRotary(juce::Slider& slider,
                                                    const juce::String& suffix,
                                                    int decimals)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 92, 19);
    slider.setTextValueSuffix(suffix);
    slider.setNumDecimalPlacesToDisplay(decimals);
    slider.setMouseDragSensitivity(3000);
    slider.setScrollWheelEnabled(true);
}

void PRISMVSTAudioProcessorEditor::enableDefaultReset(juce::Slider& slider,
                                                       const juce::String& parameterId)
{
    if (auto* p = processor.apvts.getParameter(parameterId))
        slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
}

void PRISMVSTAudioProcessorEditor::bindSelectedSection(int section)
{
    selectedSection = juce::jlimit(0, PRISMVSTAudioProcessor::numSections - 1, section);

    inputA.reset();
    outputA.reset();
    attackA.reset();
    releaseA.reset();
    widthParameter = nullptr;
    onyxA.reset();
    sectionOnA.reset();
    sectionSoloA.reset();

    const auto id = [this](const juce::String& suffix)
    {
        return PRISMVSTAudioProcessor::sectionId(selectedSection, suffix);
    };

    inputA = std::make_unique<SliderAttachment>(processor.apvts, id("input_db"), input);
    outputA = std::make_unique<SliderAttachment>(processor.apvts, id("output_db"), output);
    attackA = std::make_unique<SliderAttachment>(processor.apvts, id("attack_ms"), attack);
    releaseA = std::make_unique<SliderAttachment>(processor.apvts, id("release_ms"), release);
    widthParameter = processor.apvts.getParameter(id("width_pct"));
    width.setValue(juce::jlimit(0.0, 50.0,
        (double)readParameter(processor.apvts, id("width_pct")) - 100.0),
        juce::dontSendNotification);
    onyxA = std::make_unique<SliderAttachment>(processor.apvts, id("onyx_pct"), onyx);
    sectionOnA = std::make_unique<ButtonAttachment>(processor.apvts, id("on"), sectionOn);
    sectionSoloA = std::make_unique<ButtonAttachment>(processor.apvts, id("solo"), sectionSolo);

    enableDefaultReset(input, id("input_db"));
    enableDefaultReset(output, id("output_db"));
    enableDefaultReset(attack, id("attack_ms"));
    enableDefaultReset(release, id("release_ms"));
    // WIDTH reset is 0% additional widening (host value 100%).
    enableDefaultReset(onyx, id("onyx_pct"));

    for (int i = 0; i < PRISMVSTAudioProcessor::numSections; ++i)
        sectionButtons[(size_t)i].setToggleState(i == selectedSection, juce::dontSendNotification);

    spectrumDisplay.setSelectedSection(selectedSection);

    selectedSectionLabel.setText(
        juce::String(PRISMVSTAudioProcessor::sectionName(selectedSection))
        + "   •   fixed 36 dB/oct crossover territory   •   SHIFT = fine   CTRL/CMD = micro",
        juce::dontSendNotification);

    updateSectionButtonText();
}

void PRISMVSTAudioProcessorEditor::updateSectionButtonText()
{
    for (int i = 0; i < PRISMVSTAudioProcessor::numSections; ++i)
    {
        const bool on = readParameter(processor.apvts, PRISMVSTAudioProcessor::sectionId(i, "on")) >= 0.5f;
        const bool solo = processor.getActiveSoloSection() == i;

        juce::String text(PRISMVSTAudioProcessor::sectionName(i));
        if (!on)
            text += "  OFF";
        else if (solo)
            text += "  S";

        sectionButtons[(size_t)i].setButtonText(text);
        sectionButtons[(size_t)i].setAlpha(on ? 1.0f : 0.48f);
    }

    const bool selectedOn = readParameter(
        processor.apvts, PRISMVSTAudioProcessor::sectionId(selectedSection, "on")) >= 0.5f;
    sectionOn.setButtonText(selectedOn ? "ON" : "OFF");
}

void PRISMVSTAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();

    // Entire enclosure is molded graphite plastic: soft satin highlights,
    // no brushed grain, no metallic faceplate.
    juce::ColourGradient chassis(
        juce::Colour::fromRGB(25, 28, 30), full.getX(), full.getY(),
        juce::Colour::fromRGB(6, 8, 10), full.getX(), full.getBottom(), false);
    chassis.addColour(0.13, juce::Colour::fromRGB(30, 33, 35));
    chassis.addColour(0.52, juce::Colour::fromRGB(12, 15, 17));
    g.setGradientFill(chassis);
    g.fillRect(full);

    // Outer molded lip.
    g.setColour(juce::Colour::fromRGB(2, 3, 4));
    g.drawRoundedRectangle(full.reduced(2.0f), 14.0f, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.045f));
    g.drawRoundedRectangle(full.reduced(4.0f), 12.0f, 1.0f);

    // Header and tab rails are separate plastic pieces.
    drawPlasticPanel(g, { 10.0f, 7.0f, full.getWidth() - 20.0f, 42.0f }, 8.0f, true);
    drawPlasticPanel(g, { 10.0f, 54.0f, full.getWidth() - 20.0f, 42.0f }, 7.0f, false);

    g.setColour(juce::Colour::fromRGB(242, 246, 249));
    g.setFont(juce::Font(juce::FontOptions(25.0f, juce::Font::bold)));
    g.drawText("PRISM", 43, 10, 112, 32, juce::Justification::centredLeft);

    g.setColour(accentColour());
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawText("ROOTS PROTO 001", 158, 18, 148, 17, juce::Justification::centredLeft);

    g.setColour(textMuted());
    g.setFont(juce::Font(juce::FontOptions(9.4f)));
    g.drawText("ETHERTECH  /  7-SECTION SPECTRAL DYNAMICS  /  FIXED 36 dB/OCT",
               309, 17, 490, 18, juce::Justification::centredLeft);

    const int engineY = getHeight() - 132;

    // Analyzer housing: thick molded bezel around the existing analyzer component.
    const int analyzerTop = 94;
    const int statusY = engineY - 32;
    auto analyzerHousing = juce::Rectangle<float>(
        10.0f, (float)analyzerTop,
        full.getWidth() - 20.0f,
        (float)statusY - analyzerTop + 2.0f);
    drawPlasticPanel(g, analyzerHousing, 9.0f, false);

    // Bottom control shelf is one continuous molded-plastic tray.
    auto enginePanel = juce::Rectangle<float>(
        10.0f, (float)engineY - 2.0f,
        full.getWidth() - 20.0f, 120.0f);
    drawPlasticPanel(g, enginePanel, 9.0f, true);

    g.setColour(juce::Colour::fromRGB(46, 52, 56).withAlpha(0.55f));
    g.drawLine(24.0f, (float)engineY + 19.0f,
               full.getWidth() - 24.0f, (float)engineY + 19.0f, 1.0f);

    g.setColour(textMuted());
    g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
    g.drawText("SELECTED SECTION",
               28, engineY + 5, 160, 14, juce::Justification::centredLeft);

    // Realistic recessed black screws: decorative only, never controls.
    drawRecessedScrew(g, { 20.0f, 22.0f });
    drawRecessedScrew(g, { full.getRight() - 20.0f, 22.0f });
    drawRecessedScrew(g, { 20.0f, (float)engineY + 102.0f });
    drawRecessedScrew(g, { full.getRight() - 20.0f, (float)engineY + 102.0f });
}

void PRISMVSTAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    const int left = 18;
    const int contentW = w - 36;

    inputPeakLabel.setBounds(w - 392, 10, 120, 20);
    grLabel.setBounds(w - 266, 10, 112, 20);
    outputPeakLabel.setBounds(w - 148, 10, 130, 20);

    const int tabsY = 58;
    const int gap = 5;
    const int tabW = (contentW - gap * (PRISMVSTAudioProcessor::numSections - 1))
                     / PRISMVSTAudioProcessor::numSections;

    for (int i = 0; i < PRISMVSTAudioProcessor::numSections; ++i)
        sectionButtons[(size_t)i].setBounds(left + i * (tabW + gap), tabsY, tabW, 32);

    const int engineY = h - 132;
    const int statusH = 24;
    const int analyzerY = 98;
    const int statusY = engineY - statusH - 8;
    const int analyzerH = juce::jmax(300, statusY - analyzerY - 8);

    spectrumDisplay.setBounds(left, analyzerY, contentW, analyzerH);
    selectedSectionLabel.setBounds(left + 8, statusY, contentW - 170, statusH);
    sectionOn.setBounds(left + contentW - 156, statusY + 1, 72, statusH - 2);
    sectionSolo.setBounds(left + contentW - 78, statusY + 1, 72, statusH - 2);

    const int controlsTop = engineY + 21;
    const int cellW = contentW / controlCount;
    const std::array<PrecisionSlider*, controlCount> sliders {
        &input, &output, &attack, &release, &width, &onyx
    };

    for (int i = 0; i < controlCount; ++i)
    {
        const int x = left + i * cellW;
        controlLabels[(size_t)i].setBounds(x, controlsTop, cellW, 14);
        sliders[(size_t)i]->setBounds(x + 6, controlsTop + 12, cellW - 12, 79);
    }
}

void PRISMVSTAudioProcessorEditor::timerCallback()
{
    std::array<float, PRISMVSTAudioProcessor::spectrumBins> spectrum {};
    processor.copySpectrum(spectrum);
    spectrumDisplay.pushSpectrum(spectrum);
    std::array<float, PRISMVSTAudioProcessor::labBars> labDb {};
    std::array<float, PRISMVSTAudioProcessor::labBars> labPeak {};
    processor.copyPrismLabBars(labDb, labPeak);
    spectrumDisplay.pushPrismLabBars(labDb, labPeak);

    inputPeakLabel.setText("IN  " + juce::String(processor.getInputPeakDb(), 1) + " dBFS",
                           juce::dontSendNotification);
    grLabel.setText("GR  " + juce::String(processor.getGainReductionDb(), 1) + " dB",
                    juce::dontSendNotification);
    outputPeakLabel.setText("OUT  " + juce::String(processor.getPeakDb(), 1) + " dBFS",
                            juce::dontSendNotification);

    // Mirror host automation without writing values back during UI refresh.
    if (!widthDragging && widthParameter != nullptr)
    {
        const float stored = readParameter(processor.apvts,
            PRISMVSTAudioProcessor::sectionId(selectedSection, "width_pct"));
        width.setValue(juce::jlimit(0.0, 50.0, (double)stored - 100.0),
                       juce::dontSendNotification);
    }

    updateSectionButtonText();
    repaint();
}

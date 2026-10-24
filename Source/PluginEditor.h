#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <functional>
#include <memory>

class PrecisionSlider final : public juce::Slider
{
public:
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    double dragStartProportion = 0.0;
    int dragStartY = 0;
    static constexpr double normalPixelsForFullRange = 3000.0;
    static constexpr double finePixelsForFullRange = 9000.0;
    static constexpr double microPixelsForFullRange = 24000.0;
};

class EtherTechLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics&, int, int, int, int,
                          float, float, float, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

class SpectrumAuraDisplay final : public juce::Component
{
public:
    explicit SpectrumAuraDisplay(PRISMVSTAudioProcessor&);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    void pushSpectrum(const std::array<float, PRISMVSTAudioProcessor::spectrumBins>&);
    void pushPrismLabBars(const std::array<float, PRISMVSTAudioProcessor::labBars>&,
                          const std::array<float, PRISMVSTAudioProcessor::labBars>&);
    void setSelectedSection(int section);
    std::function<void(int)> onSectionSelected;

private:
    PRISMVSTAudioProcessor& processor;
    int selectedSection = 0;
    int draggingCrossover = -1;
    float dragStartFrequency = 0.0f;
    float dragStartX = 0.0f;

    std::array<float, PRISMVSTAudioProcessor::spectrumBins> latestSpectrum {};
    std::array<float, PRISMVSTAudioProcessor::labBars> latestLabDb {};
    std::array<float, PRISMVSTAudioProcessor::labBars> latestLabPeakDb {};

    bool hasHover = false;
    juce::Point<float> hoverPoint;

    juce::Rectangle<float> graphBounds() const;
    float frequencyToX(float frequency) const;
    float xToFrequency(float x) const;
    float levelToY(float db) const;
    float yToLevel(float y) const;
    float displayFloorDb() const;

    float parameter(const juce::String&) const;
    void setParameter(const juce::String&, float);
    float crossoverFrequency(int index) const;
    float clampedCrossoverFrequency(int index, float frequency) const;
    int findCrossoverAt(juce::Point<float>) const;
    int sectionForX(float x) const;

    float spectrumDbAt(float frequency) const;
    float displayedSpectrumDbAt(float frequency) const;
    void drawLotusGrid(juce::Graphics&, juce::Rectangle<float>) const;
    void showContextMenu(juce::Point<int> position);
    void resetCrossovers();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAuraDisplay)
};

class DynamicsTransferDisplay final : public juce::Component
{
public:
    explicit DynamicsTransferDisplay(PRISMVSTAudioProcessor&);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void setSelectedSection(int section);

private:
    enum class DragTarget { none, threshold, ratio, curve };

    PRISMVSTAudioProcessor& processor;
    int selectedSection = 0;
    DragTarget dragTarget = DragTarget::none;
    float dragStartValue = 0.0f;
    float dragStartPixel = 0.0f;
    float dragAnchorInputDb = -6.0f;
    bool curveHover = false;

    juce::Rectangle<float> graphBounds() const;
    juce::String id(const juce::String&) const;
    float parameter(const juce::String&) const;
    void setParameter(const juce::String&, float);
    juce::RangedAudioParameter* rangedParameter(const juce::String&) const;

    float dbToX(float db) const;
    float dbToY(float db) const;
    float xToDb(float x) const;
    float yToDb(float y) const;
    float outputDbForInput(float inputDb) const;
    float outputDbForInput(float inputDb, float ratio, float curve) const;
    juce::Point<float> thresholdPoint() const;
    juce::Point<float> ratioPoint() const;
    juce::Point<float> curvePoint() const;
    bool isCurveNear(juce::Point<float> point) const;
    float solveCurveForPoint(float inputDb, float targetOutputDb, float ratio) const;
    float solveRatioForPoint(float inputDb, float targetOutputDb, float curve) const;
    float dragScale(const juce::ModifierKeys&) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicsTransferDisplay)
};

class PRISMVSTAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit PRISMVSTAudioProcessorEditor(PRISMVSTAudioProcessor&);
    ~PRISMVSTAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    static constexpr int controlCount = 6;

    PRISMVSTAudioProcessor& processor;
    EtherTechLookAndFeel lookAndFeel;
    SpectrumAuraDisplay spectrumDisplay;

    int selectedSection = 0;

    std::array<juce::TextButton, PRISMVSTAudioProcessor::numSections> sectionButtons;
    juce::Label selectedSectionLabel;
    juce::ToggleButton sectionOn { "ON" };
    juce::ToggleButton sectionSolo { "SOLO" };

    std::array<juce::Label, controlCount> controlLabels;
    PrecisionSlider input, output, attack, release, width, onyx;

    juce::Label inputPeakLabel, grLabel, outputPeakLabel;

    std::unique_ptr<SliderAttachment> inputA, outputA, attackA, releaseA, onyxA;
    juce::RangedAudioParameter* widthParameter = nullptr;
    bool widthDragging = false;
    std::unique_ptr<ButtonAttachment> sectionOnA, sectionSoloA;

    void configureRotary(juce::Slider&, const juce::String& suffix, int decimals);
    void enableDefaultReset(juce::Slider&, const juce::String& parameterId);
    void bindSelectedSection(int section);
    void updateSectionButtonText();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PRISMVSTAudioProcessorEditor)
};

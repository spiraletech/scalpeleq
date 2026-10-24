#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SurgerySpectrumFill.h"
#include "SurgeryWeatherField.h"
#include "SurgeryStormChassis.h"
#include "SurgeryVisualModes.h"
#include "SurgeryLookAndFeel.h"
#include "SurgeryHeaderLayout.h"
#include "SurgeryNodeFocus.h"
#include "SurgeryPrecision.h"
#include "SurgeryResponse.h"
#include "SurgeryAnalysis.h"
#include <array>
#include <functional>
#include <memory>

class RootSelectorButton final : public juce::TextButton {
public:
    std::function<void()> onContext;
    void mouseDown(const juce::MouseEvent& event) override {
        if(event.mods.isPopupMenu()) {
            if(onContext)onContext();
            return;
        }
        juce::TextButton::mouseDown(event);
    }
};

// Retained parameter values preserve host/project compatibility. The only
// visible choice and active presentation are Standard+.
class SurgeryVisualChoiceAttachment final {
public:
    SurgeryVisualChoiceAttachment(juce::RangedAudioParameter& parameter,juce::RangedAudioParameter& legacy,juce::ComboBox& box)
        : box_(box),legacy_(legacy),attachment_(parameter,[this](float){box_.setSelectedId(8,juce::dontSendNotification);},nullptr) {
        attachment_.sendInitialUpdate();
        box_.onChange=[this]{legacy_.beginChangeGesture();legacy_.setValueNotifyingHost(0);legacy_.endChangeGesture();attachment_.setValueAsCompleteGesture((float)(box_.getSelectedId()-1));};
    }
    ~SurgeryVisualChoiceAttachment(){box_.onChange=nullptr;}
private:
    juce::ComboBox& box_;juce::RangedAudioParameter& legacy_;juce::ParameterAttachment attachment_;
};

class RootsGraph final : public juce::Component {
public:
    explicit RootsGraph(PRISMVSTAudioProcessor& owner);
    ~RootsGraph() override;
    void paint(juce::Graphics&) override;
    std::uint64_t paintedFrames() const noexcept {return paintedFrames_;}
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    void updateAnalyzer(const std::array<float,PRISMVSTAudioProcessor::labBars>&,
                        const std::array<float,PRISMVSTAudioProcessor::labBars>&);
    void updateWeather(const surgery::weather::Snapshot*,double elapsed,bool active,bool reduced,double memory,int style=1);
    const surgery::weather::Meaning& weatherMeaning() const noexcept {return weatherInterpretation_.state();}
    const surgery::weather::Snapshot& weatherMeasured() const noexcept {return weatherInterpretation_.measured();}
    // The parameter is authoritative even before the next UI timer tick.
    // This matters if HEAT MAP is disabled while the graph is frozen or the
    // host is blocking repaints: OFF must never report itself as still ON.
    bool weatherActive() const noexcept {
        const auto* enabled=processor_.apvts.getRawParameterValue("aura_color");
        return weatherActive_&&enabled&&enabled->load(std::memory_order_relaxed)>=0.5f;
    }
    const surgery::weather::Field& weatherField() const noexcept {return weatherField_;}
    std::function<void(int)> onSelectNode;
    std::function<void(int)> onSelectCarve;
    int selectedNode() const noexcept{return selected_;}
    void selectNode(int node);
    void showRootMenu(int root);
    surgery::SpectralSnapshot spectralMetrics() const noexcept {
        return surgery::summarizeSpectrum(bands_);
    }
    void setFrozen(bool frozen) noexcept {frozen_=frozen;repaint();}
    bool isFrozen() const noexcept {return frozen_;}
    void setXray(bool active) {if(xrayActive_!=active){xrayActive_=active;repaint();}}
    void setPeakWarnings(const std::array<float,7>& levels){
        if(peakWarnings_!=levels){peakWarnings_=levels;repaint();}
    }
    void setPresentation(int mode,int slot) {
        if(mode_!=mode||slot_!=slot){mode_=mode;slot_=slot;repaint();}
    }
private:
    std::uint64_t paintedFrames_=0;
    PRISMVSTAudioProcessor& processor_;
    std::array<float,PRISMVSTAudioProcessor::labBars> bands_{},peaks_{};
    std::array<float,7> peakWarnings_{};
    int selected_=0,dragging_=-1,draggingCarve_=-1;
    float dragStartX_=0.0f,dragStartHz_=0.0f;
    bool fineDragging_=false;
    int mode_=0,slot_=0;
    surgery::LiveSpectrumEnvelope<PRISMVSTAudioProcessor::labBars> spectrumEnvelope_;
    bool hover_=false,frozen_=false,xrayActive_=false;
    juce::Point<float> mouse_;
    struct ResponseCache {
        std::array<double,9> key{};
        juce::Path contour,footprint;
        bool valid=false;
    };
    std::array<ResponseCache,4> responseCache_;
    juce::Image spectralHeat_{juce::Image::ARGB,PRISMVSTAudioProcessor::labBars,64,true};
    std::array<float,PRISMVSTAudioProcessor::labBars> heatLevels_{};
    bool heatValid_=false;
    std::array<float,3> paintOptions_{};
    surgery::weather::Interpretation weatherInterpretation_;
    surgery::weather::Field weatherField_;
    juce::Image weatherImage_{juce::Image::RGB,128,64,true};
    bool weatherActive_=false;int weatherStyle_=0;float visualPhase_=0;
    // Low-resolution material field is shaded at ~20 fps and composited
    // beneath the measured FFT silhouette at full graph refresh rate.
    juce::Image mercuryMaterial_{juce::Image::RGB,360,128,false};
    unsigned mercuryPaintCount_=0;
    float mercuryPhase_=0;
    juce::Image responseOverlay_;
    std::array<double,32> responseOverlayKey_{};
    bool responseOverlayValid_=false;

    juce::Rectangle<float> area() const;
    float freqToX(float f) const;
    float xToFreq(float x) const;
    float param(int i,const char* suffix) const;
    void write(int i,const char* suffix,float value);
    int hitNode(juce::Point<float>) const;
    int hitCarveNode(juce::Point<float>) const;
    int hitCarveTab(juce::Point<float>) const;
    void popup();
    void clampCarvesToRoot(int root);
    juce::Colour colour(int territory) const;
};

class RootsEditor final:public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit RootsEditor(PRISMVSTAudioProcessor&);
    ~RootsEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::Point<int> backgroundRasterSize() const noexcept {return {rackBackground_.getWidth(),rackBackground_.getHeight()};}
private:
    using SliderA=juce::AudioProcessorValueTreeState::SliderAttachment;
    void paintRack(juce::Graphics&);
    void layoutRack();
    using ButtonA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    PRISMVSTAudioProcessor& processor_;
    RootsGraph graph_;
    SurgeryLookAndFeel lookAndFeel_;
    juce::Image rackBackground_;
    int backgroundMode_=-1;
    float backgroundScale_=0;
    std::array<RootSelectorButton,7> rootButtons_;
    std::array<double,7> peakWarningUntilMs_{};
    std::uint64_t lastPeakRevision_=0;
    juce::Slider gain_,threshold_,limitAmount_,analyzerSlope_,analyzerMemory_;
    juce::ComboBox analyzerDepth_,weatherMode_,jukeMode_;
    juce::Label jukeModeLabel_;
    juce::TextButton reducedMotion_{"REDUCED MOTION"};
    double weatherTick_=0;
    std::unique_ptr<SurgeryVisualChoiceAttachment> weatherModeA_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> jukeModeA_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reducedMotionA_;
    juce::TextButton heatToggle_{"SONAR ON"},freezeButton_{"FREEZE"};
    juce::TextButton mono_{"MONO"},on_{"ON"},off_{"OFF"},solo_{"SOLO"},phase_{"PHASE"},limit_{"LIMIT"};
    int mode_=0,carveSlot_=0;
    std::array<juce::TextButton,4> modeButtons_;
    std::array<juce::TextButton,4> carveSlotButtons_;
    juce::Slider carveFrequency_,carveDepth_,carveQ_;
    std::array<juce::Slider,5> onyxSliders_;
    std::array<juce::Label,5> onyxLabels_;
    std::array<std::unique_ptr<SliderA>,5> onyxAttachments_;
    juce::TextButton xrayButton_{"X-RAY"};
    std::unique_ptr<ButtonA> xrayAttachment_;
    juce::TextButton carveOn_{"ON"},carveOff_{"OFF"},carveDynamic_{"DYNAMIC"};
    juce::Label carveFrequencyLabel_,carveDepthLabel_,carveQLabel_;
    std::unique_ptr<SliderA> carveFrequencyA_,carveDepthA_,carveQA_;
    std::unique_ptr<ButtonA> carveOnA_,carveDynamicA_;
    juce::Label title_,selection_,status_,gainLabel_,thresholdLabel_,
        analyzerDepthLabel_,analyzerSlopeLabel_,analyzerMemoryLabel_,analyzerLegend_;
    using ComboA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderA> gainA_,thresholdA_,limitAmountA_,analyzerSlopeA_,analyzerMemoryA_;
    std::unique_ptr<ComboA> analyzerDepthA_;
    std::unique_ptr<ButtonA> heatA_;
    std::unique_ptr<ButtonA> monoA_,onA_,soloA_,phaseA_,limitA_;
    juce::TextButton bypass_{"BYPASS"},compare_{"A / B: A"},copy_{"COPY A > B"};
    juce::TextButton save_{"SAVE"},load_{"LOAD"};
    juce::TextButton peakHold_{"PEAK HOLD"};
    juce::Slider output_;
    juce::Label outputLabel_,compLabel_,domainLabel_,detectorLabel_;
    juce::ComboBox compressor_,domain_,detector_;
    std::unique_ptr<ComboA> compressorA_,domainA_,detectorA_;
    std::unique_ptr<SliderA> outputA_;
    std::unique_ptr<ButtonA> bypassA_;
    std::unique_ptr<juce::FileChooser> presetChooser_;
    juce::TooltipWindow tooltips_{this, 650};
    void choosePreset(bool save);
    int node_=0;
    void bindNode(int n);
    void bindCarve();
    void setMode(int mode);
    void updateRootButtons();
    void timerCallback() override;
};

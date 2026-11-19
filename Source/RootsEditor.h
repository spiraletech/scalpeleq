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
#include "SurgeryLogSliderAttachment.h"
#include "SurgeryCalibratedSliders.h"
#include "SurgeryResponse.h"
#include "SurgeryAnalysis.h"
#include "ScalpelMercuryField.h"
#include "ScalpelFluid3D.h"
#include "ScalpelBandModel.h"
#include <array>
#include <functional>
#include <memory>

// The legacy cut remains readable/automatable until this point is edited.
class ScalpelCarveGainAttachment final : private juce::Slider::Listener {
public:
    ScalpelCarveGainAttachment(juce::RangedAudioParameter& gain,
                               juce::RangedAudioParameter& depth,juce::Slider& slider)
        : slider_(slider),
          signedGain_(gain,[this](float value){gain_=value;refresh();},nullptr),
          legacyCut_(depth,[this](float value){cut_=value;refresh();},nullptr) {
        slider_.setRange(-40.0,40.0,0.01);
        legacyCut_.sendInitialUpdate();
        signedGain_.sendInitialUpdate();
        slider_.addListener(this);
    }
    ~ScalpelCarveGainAttachment() override {
        slider_.removeListener(this);
        if(dragging_)signedGain_.endGesture();
    }
private:
    void refresh() {
        if(!writing_)slider_.setValue(gain_!=0.0f?gain_:-cut_,juce::dontSendNotification);
    }
    void sliderValueChanged(juce::Slider*) override {
        const juce::ScopedValueSetter<bool> writing(writing_,true);
        const float value=(float)slider_.getValue();
        if(dragging_)signedGain_.setValueAsPartOfGesture(value);
        else signedGain_.setValueAsCompleteGesture(value);
        if(cut_!=0.0f)legacyCut_.setValueAsCompleteGesture(0.0f);
    }
    void sliderDragStarted(juce::Slider*) override {dragging_=true;signedGain_.beginGesture();}
    void sliderDragEnded(juce::Slider*) override {signedGain_.endGesture();dragging_=false;}
    juce::Slider& slider_;
    bool dragging_=false,writing_=false;
    float gain_=0.0f,cut_=0.0f;
    juce::ParameterAttachment signedGain_,legacyCut_;
};

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
    void showRootMenu(int root,juce::Component& button);
    surgery::SpectralSnapshot spectralMetrics() const noexcept {
        return surgery::summarizeSpectrum(bands_);
    }
    void clearPeakReadouts() noexcept { peakReadouts_.fill(-144.f); repaint(); }
    void setFrozen(bool frozen) noexcept {
        if(frozen_!=frozen){frozen_=frozen;mercuryPaintCount_=0;}
        repaint();
    }
    bool isFrozen() const noexcept {return frozen_;}
    void releaseVisualResources() noexcept {mercury3D_.reset();mercury3DImage_={};}
    void setXray(bool active) {
        if(xrayActive_!=active){
            xrayActive_=active;
            mercuryPaintCount_=0;
            if(!active)mercury3D_.reset(); // release GPU resources on editor/host mode change
            repaint();
        }
    }
    void setPeakWarnings(const std::array<float,7>& levels){
        if(peakWarnings_!=levels){peakWarnings_=levels;repaint();}
    }
    void setPresentation(int mode,int slot) {
        if(mode_!=mode||slot_!=slot){mode_=mode;slot_=slot;repaint();}
    }
private:
    std::uint64_t paintedFrames_=0;
    void paintStaticFrame(juce::Graphics&) const;
    juce::Image staticFrame_;
    float staticFrameScale_=0;
    PRISMVSTAudioProcessor& processor_;
    std::array<float,PRISMVSTAudioProcessor::labBars> bands_{},peaks_{};
    std::array<float,7> peakWarnings_{};
    int selected_=0,dragging_=-1,draggingCarve_=-1,draggingInterior_=-1;
    float dragStartX_=0.0f,dragStartHz_=0.0f;
    double snapTargetHz_=0.0;
    std::array<bool,prism::roots::v2::kNodes> bandGestures_ {};
    scalpel::bands::Model dragStartModel_ {};
    bool fineDragging_=false;
    int mode_=0,slot_=0;
    surgery::LiveSpectrumEnvelope<PRISMVSTAudioProcessor::labBars> spectrumEnvelope_;
    int displayDomain_=0;
    std::array<float,surgery::kRoots> peakReadouts_ {};
    bool hover_=false,frozen_=false,xrayActive_=false;
    juce::Point<float> mouse_;
    struct ResponseCache {
        std::array<double,9> key{};
        juce::Path contour,footprint;
        bool valid=false;
    };
    std::array<ResponseCache,4> responseCache_;
    struct ResponseProbe {float x=0;std::complex<double> z1;};
    std::array<ResponseProbe,2049> responseProbes_;
    std::array<double,3> responseProbeKey_{};
    juce::Image spectralHeat_{juce::Image::ARGB,PRISMVSTAudioProcessor::labBars,224,true};
    std::array<float,PRISMVSTAudioProcessor::labBars> heatLevels_{};
    bool heatValid_=false;
    std::array<float,3> paintOptions_{};
    surgery::weather::Interpretation weatherInterpretation_;
    surgery::weather::Field weatherField_;
    juce::Image weatherImage_{juce::Image::RGB,128,64,true};
    bool weatherActive_=false;int weatherStyle_=0;float visualPhase_=0;
    // GPU fluid runs on the editor thread and is always downstream of the FFT.
    // The original liquid image remains a reliable CPU renderer fallback.
    juce::Image mercuryMaterial_{juce::Image::RGB,360,128,false};
    juce::Image mercury3DImage_;
    scalpel::xray::FluidVolume3D mercury3D_;
    double mercuryElapsed_=1.0/60.0;
    unsigned mercuryPaintCount_=0;
    float mercuryPhase_=0;
    scalpel::xray::LiquidSurface<> mercurySurface_;
    juce::Image responseOverlay_;
    std::array<double,32> responseOverlayKey_{};
    bool responseOverlayValid_=false;
    // The graph observes the same host parameters as the controls. Carve
    // edits must invalidate it even when FFT frames are silent or frozen.
    static constexpr std::array<const char*,5> carveRepaintFields_{"on","hz","depth","gain_db","q"};
    std::array<std::unique_ptr<juce::ParameterAttachment>,
               surgery::kSlots*carveRepaintFields_.size()+2> carveRepaintAttachments_;
    void bindCarveRepaints();

    juce::Rectangle<float> area() const;
    float freqToX(float f) const;
    float xToFreq(float x) const;
    float param(int i,const char* suffix) const;
    juce::Point<float> boundaryPoint(int node,const scalpel::bands::Model&) const;
    int hitNode(juce::Point<float>,bool cycleCoincidentBand=false) const;
    int hitBandInterior(juce::Point<float>) const;
    scalpel::bands::Model loadBandModel() const;
    void storeBandModel(const scalpel::bands::Model&);
    void endBandGesture();
    double snappedFrequency(double,bool disableSnap,int movingBand,
                            scalpel::bands::Anchor movingAnchor);
    int hitCarveNode(juce::Point<float>) const;
    int hitCarveTab(juce::Point<float>) const;
    void popup(juce::Component* bandButton=nullptr,juce::Point<int> screenPoint={});
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
    bool closing_=false;
    void paintRack(juce::Graphics&);
    void layoutRack();
    void setSelectedEnabled(bool carve,bool enabled);
    void updateCarveTightness();
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
    juce::Slider gain_,limitAmount_,analyzerSlope_,analyzerMemory_;
    SurgeryFineWheelSlider threshold_;
    juce::ComboBox analyzerDepth_,weatherMode_,jukeMode_;
    juce::Label jukeModeLabel_;
    juce::TextButton reducedMotion_{"REDUCED MOTION"};
    double weatherTick_=0;
    std::unique_ptr<SurgeryVisualChoiceAttachment> weatherModeA_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> jukeModeA_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reducedMotionA_;
    juce::TextButton heatToggle_{"SONAR ON"},freezeButton_{"FREEZE"};
    juce::TextButton mono_{"MONO"},on_{"ON"},off_{"OFF"},solo_{"SOLO"},
        phase_{"PHASE CORRECTION"},limit_{"LIMIT"};
    int mode_=0,carveSlot_=0;
    std::array<juce::TextButton,3> modeButtons_,masterButtons_;
    juce::Label masterLabel_;
    std::array<juce::TextButton,4> carveSlotButtons_;
    juce::Slider carveFrequency_,carveDepth_;
    SurgeryFineWheelSlider carveQ_;
    juce::TextButton xrayButton_{"X-RAY"};
    std::unique_ptr<ButtonA> xrayAttachment_;
    juce::TextButton carveOn_{"ON"},carveOff_{"OFF"},carveDynamic_{"DYNAMIC"};
    juce::Label carveFrequencyLabel_,carveDepthLabel_,carveQLabel_;
    std::unique_ptr<SurgeryLogSliderAttachment> carveFrequencyA_,carveQA_;
    std::unique_ptr<ScalpelCarveGainAttachment> carveDepthA_;
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
    juce::Label outputLabel_,compLabel_,ratioLabel_,detectorLabel_;
    juce::ComboBox compressor_,ratio_,detector_;
    std::unique_ptr<juce::ParameterAttachment> compressorA_;
    std::unique_ptr<ComboA> ratioA_,detectorA_;
    std::unique_ptr<SliderA> outputA_;
    std::unique_ptr<ButtonA> bypassA_;
    std::unique_ptr<juce::FileChooser> presetChooser_;
    juce::TooltipWindow tooltips_{this, 650};
    void choosePreset(bool save);
    int node_=0;
    void bindNode(int n);
    void bindCarve();
    void setMode(int mode);
    void updateCarveButtons();
    void updateRootButtons();
    void timerCallback() override;
};


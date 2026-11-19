#pragma once
#include <bit>
#include "SurgeryWeatherMeasurement.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include "PrismLabAnalyzer.h"
#include "ScalpelAnalyzerHealth.h"
#include "RootsAudioEngine.h"
#include "ScalpelBandModel.h"
#include "SurgeryCarveEngine.h"

class PRISMVSTAudioProcessor final : public juce::AudioProcessor,
                                    private juce::AudioProcessorValueTreeState::Listener,
                                    private juce::AsyncUpdater
{
public:
    static constexpr int numSections = 7;
    static constexpr int numCrossovers = numSections - 1;
    static constexpr int spectrumBins = 512;
    static constexpr int labBars = prism::analysis::LabBands::count;

    PRISMVSTAudioProcessor();
    ~PRISMVSTAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter("surgery_bypass"); }

    const juce::String getName() const override { return "Scalpel EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    bool savePreset(const juce::File&);
    bool loadPreset(const juce::File&);
    void switchComparison();
    void copyComparison();
    int comparisonSlot() const noexcept { return comparisonSlot_; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    float getInputPeakDb() const noexcept { return inputPeakDb.load(); }
    float getPeakDb() const noexcept { return peakDb.load(); }
    float getPeakHoldDb() const noexcept { return heldPeak(numSections); }
    float getChannelPeakDb(int channel) const noexcept {
        return channelPeakDb[(size_t)juce::jlimit(0, 1, channel)].load();
    }
    void resetPeakHold() noexcept {
        bandPeakEpoch_.fetch_add(1, std::memory_order_acq_rel);
    }
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }
    float getLufsShort() const noexcept { return lufsShort.load(); }
    float getLufsIntegrated() const noexcept { return lufsIntegrated.load(); }
    void copySpectrum(std::array<float, spectrumBins>& destination) const noexcept;
    void copyPrismLabBars(std::array<float, labBars>& db,
                          std::array<float, labBars>& peakDb) const noexcept;
    // Read-only diagnostics for a Windows baseline harness; never used for DSP.
    scalpel::analysis::AnalyzerHealthSnapshot getAnalyzerHealth() const noexcept {
        return analyzerHealth_.snapshot();
    }
    // UI-thread consumer; producer is the existing analysis worker.
    bool readWeather(surgery::weather::Snapshot& frame) noexcept {return weatherSnapshots_.latest(frame);}
    void beginWeatherConsumer() noexcept {if(weatherConsumers_.load()==0){surgery::weather::Snapshot discard;weatherSnapshots_.latest(discard);}weatherConsumers_.fetch_add(1);}
    void endWeatherConsumer() noexcept {weatherConsumers_.fetch_sub(1);}
    float getStereoActivity(int section) const noexcept
    {
        return stereoActivity[(size_t)juce::jlimit(0, numSections - 1, section)].load();
    }
    int getActiveSoloSection() const noexcept;
    int getActiveRootsSolo() const noexcept;
    scalpel::bands::Model magneticBandModel() const noexcept; // message thread only
    void setMagneticBandModel(const scalpel::bands::Model&); // message thread only
    // Message-thread selection transfer while SOLO is engaged.
    void transferRootsSoloTo(int node);

    static juce::String sectionId(int section, const juce::String& suffix);
    static juce::String crossoverId(int crossover);
    static juce::String rootsNodeId(int node, const juce::String& suffix);
    static juce::String carveId(int root, int slot, const juce::String& suffix);
    // Message-thread, host-automatable node resets used by the graph double-click.
    bool resetRootBoundary(int node);
    bool resetCarveNode(int root, int slot);
    static juce::String rootsRootSoloId(int root);
    static juce::String rootsRootGainId(int root);
    static juce::String rootsRootControlId(int root, const juce::String& suffix);
    prism::roots::v2::Meter getRootsMeter() const noexcept {
        return { rootMeters[0].load(), rootMeters[1].load(), rootMeters[2].load(),
                 rootMeters[3].load(), rootMeters[4].load() };
    }
    void setAnalyzerDomain(int domain) noexcept { analyzerDomain_.store(juce::jlimit(0,2,domain)); }
    int getMasterDomain() const noexcept { return juce::roundToInt(masterDomainValue_->load()); }
    int getAnalyzerDomain() const noexcept { return measuredAnalyzerDomain_.load(); }
    float getCarveGainDb(int root,int slot) const noexcept {
        const auto index=(size_t)(root*4+slot);
        const auto signedGain=carveGainParameters[index]->load();
        return signedGain!=0.f?signedGain:-carveParameters[index][2]->load();
    }
    float getRootBandPeakDb(int root) const noexcept {
        return root>=0&&root<numSections?rootBandPeakDb[(size_t)root].load():-144.f;
    }
    float getHeldRootBandPeakDb(int root) const noexcept {
        return root >= 0 && root < numSections ? heldPeak(root) : -144.f;
    }
    std::uint64_t getRootBandPeakRevision() const noexcept {
        return rootBandPeakRevision_.load(std::memory_order_acquire);
    }
    static const char* sectionName(int section) noexcept;

private:
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        float process(float x) noexcept;
        void reset() noexcept { z1 = z2 = 0.0f; }
        void setFirstOrderLowPass(double sampleRate, float frequency) noexcept;
        void setFirstOrderHighPass(double sampleRate, float frequency) noexcept;
        void setSecondOrderLowPass(double sampleRate, float frequency, float q) noexcept;
        void setSecondOrderHighPass(double sampleRate, float frequency, float q) noexcept;
        void setFirstOrderAllPass(double sampleRate, float frequency) noexcept;
        void setSecondOrderAllPass(double sampleRate, float frequency, float q) noexcept;
    };

    // A fixed 36 dB/oct Linkwitz-Riley-style electrical crossover:
    // two cascaded 3rd-order Butterworth sections on each branch.
    // The high branch is polarity-corrected so low+high is all-pass / flat magnitude.
    struct LR6Splitter
    {
        std::array<Biquad, 4> low;
        std::array<Biquad, 4> high;

        void reset() noexcept;
        void setCutoff(double sampleRate, float frequency) noexcept;
        void process(float x, float& lowOut, float& highOut) noexcept;
    };

    // The low+high sum of an LR6 split is a 3rd-order all-pass. Applying this
    // to earlier bands phase-aligns a serial multiband bank without changing magnitude.
    struct ThirdOrderAllPass
    {
        Biquad first;
        Biquad second;

        void reset() noexcept;
        void setCutoff(double sampleRate, float frequency) noexcept;
        float process(float x) noexcept;
    };

    struct SectionState
    {
        float envelope = 0.0f;
        float gainReductionDb = 0.0f;
    };

    std::array<std::array<LR6Splitter, 2>, numCrossovers> splitters;
    std::array<std::array<std::array<ThirdOrderAllPass, 2>, numSections>, numCrossovers> compensators;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, numCrossovers> crossoverSmoothers;
    std::array<SectionState, numSections> sectionStates;
    std::array<juce::MemoryBlock, 2> comparisonStates;
    int comparisonSlot_ = 0;
    struct RootParameters {
        // Legacy indices 0..11 are frozen for recall; the new COLOR DYNAMIC,
        // PHASE, and optical LIMIT controls append without reordering them.
        // Presence is appended at index 16. BAND OFF remains index 2 and
        // continues to mean spectral removal; deletion never reuses it.
        std::array<std::atomic<float>*, 17> values {};
    };
    std::array<RootParameters, numSections> rootParameters;
    std::array<std::array<std::atomic<float>*, 2>, 14> nodeParameters {};
    std::array<std::atomic<float>*,28> carveGainParameters {};
    std::array<std::atomic<float>*,numSections> dynamicsModeParameters {}, dynamicsOffsetParameters {}, dynamicsRatioParameters {};
    std::array<std::array<std::atomic<float>*, 7>, 28> carveParameters {};
    std::atomic<float>* engineMode = nullptr;
    std::atomic<float>* bypassValue = nullptr;
    std::atomic<float>* trimValue = nullptr;
    std::atomic<float>* masterDomainValue_ = nullptr;
    std::array<std::atomic<double>, 5> rootMeters { 1.0, 0.0, 0.0, 1.0, 0.0 };
    std::array<std::atomic<float>,numSections> rootBandPeakDb {};
    // Each published hold carries its reset epoch, so an in-flight audio block
    // cannot repopulate a cleared reading from before RESET.
    std::atomic<std::uint32_t> bandPeakEpoch_ {1};
    std::array<std::atomic<std::uint64_t>,numSections+1> heldPeaks_ {};
    float heldPeak(int index) const noexcept;
    void recordPeak(int index, float peak, std::uint32_t epoch) noexcept;
    std::atomic<std::uint64_t> rootBandPeakRevision_ {0};
    juce::SmoothedValue<float> outputTrimSmoother, bypassMix, masterMidGain_, masterSideGain_;
    juce::AudioBuffer<float> dryScratch;
    prism::roots::v2::Engine rootsEngine;
    surgery::CarveEngine carveEngine, signedCarveEngine;
    std::array<std::atomic<float>, numSections> stereoActivity {};
    std::array<juce::String, numSections> soloParameterIds {};
    std::array<juce::String, numSections> rootsSoloParameterIds {};
    std::array<juce::String, 14> magneticFrequencyParameterIds {};
    std::array<juce::String, 28> magneticStateParameterIds {};
    scalpel::bands::Model realtimeBandModel_; // audio thread only
    std::atomic<unsigned> magneticEditSequence { 0 };
    scalpel::bands::Model lastValidBandModel_;
    bool sanitizingMagneticState_ = false; // message thread only
    std::atomic<int> latestRootsSolo { -1 };
    std::atomic<int> latestSoloSection { -1 };

    std::array<juce::dsp::IIR::Filter<float>, 2> loudHp;
    std::array<juce::dsp::IIR::Filter<float>, 2> loudShelf;

    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> fftWindow { fftSize, juce::dsp::WindowingFunction<float>::hann };
    // Display update cadence improves without adding audio path latency.
    static constexpr int fftHopSize = 384;
    static constexpr int analyzerQueueSize = 65536;

    class AnalyzerWorker final : public juce::Thread
    {
    public:
        explicit AnalyzerWorker(PRISMVSTAudioProcessor& ownerIn)
            : juce::Thread("PRISM Analyzer"), owner(ownerIn) {}
        void run() override { owner.runAnalyzerThread(); }

    private:
        PRISMVSTAudioProcessor& owner;
    };

    juce::AbstractFifo analyzerFifo { analyzerQueueSize };
    std::array<float, analyzerQueueSize> analyzerQueue {}, analyzerRightQueue {};
    juce::WaitableEvent analyzerEvent;
    AnalyzerWorker analyzerWorker { *this };
    scalpel::analysis::AnalyzerHealth analyzerHealth_;

    std::array<float, fftSize> fftFifo {}, fftRightFifo {};
    std::array<float, fftSize / 2 + 1> leftSpectralPower {};
    std::array<float, fftSize * 2> fftData {};
    int fftWritePos = 0;
    int fftHopCounter = 0;
    std::array<std::atomic<float>, spectrumBins> spectrum {};
    // Written only by the existing analyzer worker; read by GUI via atomics.
    prism::analysis::LabBands labAnalyzer;
    std::atomic<int> analyzerDomain_ { 0 }, measuredAnalyzerDomain_ { 0 };
    int lastAnalyzerDomain_ = 0; // worker only once prepared
    std::array<std::atomic<float>, labBars> labDb {};
    std::array<std::atomic<float>, labBars> labPeakDb {};

    surgery::weather::Measurement weatherMeasurement_;
    surgery::weather::SnapshotQueue<surgery::weather::Snapshot> weatherSnapshots_;
    std::atomic<unsigned> weatherConsumers_{0};
    std::atomic<float>* weatherModeValue_=nullptr;
    std::atomic<float>* weatherFillValue_=nullptr;
    std::atomic<float>* visualStyleValue_=nullptr;
    std::atomic<float>* jukeModeValue_=nullptr;
    double weatherWindowSum_=4096,weatherWindowEnergy_=6144,weatherSeconds_=0;
    bool weatherWasEnabled_=false;
    void prepareWeatherAnalysis();
    void publishWeatherAnalysis();
    double currentSampleRate = 44100.0;
    double integratedEnergy = 0.0;
    uint64_t integratedSamples = 0;

    std::atomic<float> inputPeakDb { -144.0f };
    std::atomic<float> peakDb { -144.0f };
    std::array<std::atomic<float>, 2> channelPeakDb {{ -144.0f, -144.0f }};
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> lufsShort { -144.0f };
    std::atomic<float> lufsIntegrated { -144.0f };

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void reconcileExclusiveSolo(); // only invoked on message thread
    void reconcileExclusiveRootsSolo(); // message thread only
    void sanitizeMagneticBandState(); // message thread only
    scalpel::bands::Model readMagneticBandModel() const noexcept;
    void sanitizeCrossovers();
    void updateCrossoverCoefficients(int blockSamples);
    void processSevenSectionEngine(juce::AudioBuffer<float>& buffer);
    void processRootsEngine(juce::AudioBuffer<float>& buffer, std::uint32_t bandPeakEpoch);
    void updateMeters(const juce::AudioBuffer<float>& buffer, std::uint32_t bandPeakEpoch);
    void pushAnalyzerSamples(const juce::AudioBuffer<float>& buffer);
    void runAnalyzerThread();
    void consumeAnalyzerSample(float left, float right);
    void renderSpectrumFrame();

    static float read(const juce::AudioProcessorValueTreeState& state, const juce::String& id);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PRISMVSTAudioProcessor)
};

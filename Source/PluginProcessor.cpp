#include "PluginProcessor.h"
#include "RootsEditor.h"
#include "PluginEditor.h"
#include <cmath>
#include <charconv>

namespace
{
constexpr float kFloorDb = -144.0f;
constexpr std::array<float, PRISMVSTAudioProcessor::numCrossovers> kDefaultCrossovers {
    60.0f, 120.0f, 250.0f, 500.0f, 2000.0f, 6000.0f
};

inline float toDb(float g) { return juce::Decibels::gainToDecibels(g, kFloorDb); }
inline float toGain(float d) { return juce::Decibels::decibelsToGain(d); }

inline float safeFrequency(double sampleRate, float frequency)
{
    return juce::jlimit(20.0f, (float)(sampleRate * 0.45), frequency);
}

bool finiteParameterState(const juce::XmlElement& xml,
                          const juce::AudioProcessorValueTreeState& parameters)
{
    for(auto* child=xml.getFirstChildElement();child!=nullptr;child=child->getNextElement()) {
        if(parameters.getParameter(child->getStringAttribute("id"))==nullptr)continue;
        const auto text=child->getStringAttribute("value").trim();
        const auto* begin=text.toRawUTF8();
        const auto* end=begin+text.getNumBytesAsUTF8();
        double value=0;
        const auto parsed=std::from_chars(begin,end,value);
        if(parsed.ec!=std::errc{}||parsed.ptr!=end||!std::isfinite(value)||!std::isfinite((float)value))return false;
    }
    return true;
}
}

const char* PRISMVSTAudioProcessor::sectionName(int section) noexcept
{
    static constexpr std::array<const char*, numSections> names {
        "ROOT 1", "ROOT 2", "ROOT 3", "ROOT 4",
        "ROOT 5", "ROOT 6", "ROOT 7"
    };
    return names[(size_t)juce::jlimit(0, numSections - 1, section)];
}

juce::String PRISMVSTAudioProcessor::sectionId(int section, const juce::String& suffix)
{
    return "section" + juce::String(section + 1) + "_" + suffix;
}

juce::String PRISMVSTAudioProcessor::rootsRootControlId(int root,const juce::String& suffix)
{
    return "roots_root" + juce::String(root + 1) + "_" + suffix;
}

juce::String PRISMVSTAudioProcessor::rootsRootGainId(int root)
{
    return "roots_root" + juce::String(root + 1) + "_gain_db";
}

juce::String PRISMVSTAudioProcessor::rootsRootSoloId(int root)
{
    return "roots_root" + juce::String(root + 1) + "_solo";
}

juce::String PRISMVSTAudioProcessor::carveId(int root,int slot,const juce::String& suffix)
{
    return "surgery_carve_r"+juce::String(root+1)+"_c"+juce::String(slot+1)+"_"+suffix;
}

bool PRISMVSTAudioProcessor::resetRootBoundary(int node)
{
    if(node<0||node>=prism::roots::v2::kNodes)return false;
    if(auto* parameter=apvts.getParameter(rootsNodeId(node,"hz"))) {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(
            (float)prism::roots::v2::Engine::defaults[(size_t)node]));
        parameter->endChangeGesture();
        return true;
    }
    return false;
}

bool PRISMVSTAudioProcessor::resetCarveNode(int root,int slot)
{
    if(root<0||root>=surgery::kRoots||slot<0||slot>=surgery::kSlots)return false;
    // Center the neutral node inside its currently edited territory.
    // The global 1 kHz factory value would land outside most ROOTS.
    const float a=read(apvts,rootsNodeId(root*2,"hz"));
    const float b=read(apvts,rootsNodeId(root*2+1,"hz"));
    const double fraction=(slot+1.0)/5.0;
    const float low=juce::jmax(20.0f,std::min(a,b));
    const float high=juce::jmax(low,std::max(a,b));
    const float frequency=juce::jlimit(20.0f,20000.0f,
        (float)std::exp(std::log((double)low)+fraction*std::log(high/low)));
    const auto assign=[this,root,slot](const char* suffix,float value) {
        if(auto* parameter=apvts.getParameter(carveId(root,slot,suffix))) {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    };
    assign("hz",frequency);
    assign("depth",0.0f);
    assign("q",2.0f);
    assign("threshold",-24.0f);
    assign("dynamic",0.0f);
    assign("domain",0.0f);
    // ON stays unchanged so the selected node does not disappear on reset.
    return true;
}

juce::String PRISMVSTAudioProcessor::rootsNodeId(int node,const juce::String& suffix)
{
    return "roots_node" + juce::String(node + 1) + "_" + suffix;
}

juce::String PRISMVSTAudioProcessor::crossoverId(int crossover)
{
    return "crossover" + juce::String(crossover + 1) + "_hz";
}

float PRISMVSTAudioProcessor::read(const juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    if (auto* v = state.getRawParameterValue(id))
        return v->load();
    return 0.0f;
}

//==============================================================================
// Allocation-free filter primitives.
float PRISMVSTAudioProcessor::Biquad::process(float x) noexcept
{
    const float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
}

void PRISMVSTAudioProcessor::Biquad::setFirstOrderLowPass(double sr, float f) noexcept
{
    const float k = std::tan(juce::MathConstants<float>::pi * safeFrequency(sr, f) / (float)sr);
    const float n = 1.0f / (1.0f + k);
    b0 = k * n; b1 = b0; b2 = 0.0f;
    a1 = (k - 1.0f) * n; a2 = 0.0f;
}

void PRISMVSTAudioProcessor::Biquad::setFirstOrderHighPass(double sr, float f) noexcept
{
    const float k = std::tan(juce::MathConstants<float>::pi * safeFrequency(sr, f) / (float)sr);
    const float n = 1.0f / (1.0f + k);
    b0 = n; b1 = -n; b2 = 0.0f;
    a1 = (k - 1.0f) * n; a2 = 0.0f;
}

void PRISMVSTAudioProcessor::Biquad::setSecondOrderLowPass(double sr, float f, float q) noexcept
{
    const float k = std::tan(juce::MathConstants<float>::pi * safeFrequency(sr, f) / (float)sr);
    const float k2 = k * k;
    const float n = 1.0f / (1.0f + k / q + k2);
    b0 = k2 * n; b1 = 2.0f * b0; b2 = b0;
    a1 = 2.0f * (k2 - 1.0f) * n;
    a2 = (1.0f - k / q + k2) * n;
}

void PRISMVSTAudioProcessor::Biquad::setSecondOrderHighPass(double sr, float f, float q) noexcept
{
    const float k = std::tan(juce::MathConstants<float>::pi * safeFrequency(sr, f) / (float)sr);
    const float k2 = k * k;
    const float n = 1.0f / (1.0f + k / q + k2);
    b0 = n; b1 = -2.0f * n; b2 = n;
    a1 = 2.0f * (k2 - 1.0f) * n;
    a2 = (1.0f - k / q + k2) * n;
}

void PRISMVSTAudioProcessor::Biquad::setFirstOrderAllPass(double sr, float f) noexcept
{
    const float k = std::tan(juce::MathConstants<float>::pi * safeFrequency(sr, f) / (float)sr);
    const float a = (k - 1.0f) / (k + 1.0f);
    b0 = a; b1 = 1.0f; b2 = 0.0f;
    a1 = a; a2 = 0.0f;
}

void PRISMVSTAudioProcessor::Biquad::setSecondOrderAllPass(double sr, float f, float q) noexcept
{
    const float k = std::tan(juce::MathConstants<float>::pi * safeFrequency(sr, f) / (float)sr);
    const float k2 = k * k;
    const float n = 1.0f / (1.0f + k / q + k2);
    a1 = 2.0f * (k2 - 1.0f) * n;
    a2 = (1.0f - k / q + k2) * n;
    b0 = a2; b1 = a1; b2 = 1.0f;
}

void PRISMVSTAudioProcessor::LR6Splitter::reset() noexcept
{
    for (auto& f : low) f.reset();
    for (auto& f : high) f.reset();
}

void PRISMVSTAudioProcessor::LR6Splitter::setCutoff(double sr, float f) noexcept
{
    // One 3rd-order Butterworth = first-order + second-order Q=1.
    // Squaring it produces Linkwitz-Riley 6th order: 36 dB/oct and -6 dB at Fc.
    low[0].setFirstOrderLowPass(sr, f);
    low[1].setSecondOrderLowPass(sr, f, 1.0f);
    low[2].setFirstOrderLowPass(sr, f);
    low[3].setSecondOrderLowPass(sr, f, 1.0f);

    high[0].setFirstOrderHighPass(sr, f);
    high[1].setSecondOrderHighPass(sr, f, 1.0f);
    high[2].setFirstOrderHighPass(sr, f);
    high[3].setSecondOrderHighPass(sr, f, 1.0f);
}

void PRISMVSTAudioProcessor::LR6Splitter::process(float x, float& lowOut, float& highOut) noexcept
{
    lowOut = x;
    highOut = x;
    for (auto& f : low) lowOut = f.process(lowOut);
    for (auto& f : high) highOut = f.process(highOut);

    // LR6 branches are 180 degrees apart. Polarity-correct the high branch so
    // their electrical sum is the matching all-pass response.
    highOut = -highOut;
}

void PRISMVSTAudioProcessor::ThirdOrderAllPass::reset() noexcept
{
    first.reset();
    second.reset();
}

void PRISMVSTAudioProcessor::ThirdOrderAllPass::setCutoff(double sr, float f) noexcept
{
    first.setFirstOrderAllPass(sr, f);
    second.setSecondOrderAllPass(sr, f, 1.0f);
}

float PRISMVSTAudioProcessor::ThirdOrderAllPass::process(float x) noexcept
{
    return second.process(first.process(x));
}

//==============================================================================
PRISMVSTAudioProcessor::PRISMVSTAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    const char* rootNames[] = { "gain_db", "mono", "on", "domain", "comp", "detector", "threshold",
        "onyx_sparkle", "onyx_shine", "onyx_smooth", "onyx_tape", "solo",
        "color_dynamic", "phase", "limit_amount", "limit_on" };
    for (int root = 0; root < numSections; ++root)
        for (int field = 0; field < 16; ++field)
            rootParameters[(size_t)root].values[(size_t)field] = apvts.getRawParameterValue(rootsRootControlId(root, rootNames[field]));
    for (int node = 0; node < 14; ++node)
        for (int field = 0; field < 2; ++field)
            nodeParameters[(size_t)node][(size_t)field] = apvts.getRawParameterValue(rootsNodeId(node, field == 0 ? "hz" : "q"));
    const char* carveNames[] = { "on", "hz", "depth", "q", "threshold", "dynamic", "domain" };
    for (int root = 0; root < 7; ++root)
        for (int slot = 0; slot < 4; ++slot)
            for (int field = 0; field < 7; ++field)
                carveParameters[(size_t)(root * 4 + slot)][(size_t)field] = apvts.getRawParameterValue(carveId(root, slot, carveNames[field]));
    engineMode = apvts.getRawParameterValue("roots_engine_mode");
    bypassValue = apvts.getRawParameterValue("surgery_bypass");
    trimValue = apvts.getRawParameterValue("surgery_output_db");
    weatherModeValue_=apvts.getRawParameterValue("sonar_weather_mode");
    visualStyleValue_=apvts.getRawParameterValue("sonar_visual_style");jukeModeValue_=apvts.getRawParameterValue("juke_mode");
    weatherFillValue_=apvts.getRawParameterValue("aura_color");

    for (auto& v : spectrum)
        v.store(kFloorDb);

    for (int s = 0; s < numSections; ++s)
    {
        stereoActivity[(size_t)s].store(0.0f);
        rootBandPeakDb[(size_t)s].store(-144.0f);
        soloParameterIds[(size_t)s] = sectionId(s, "solo");
        apvts.addParameterListener(soloParameterIds[(size_t)s], this);
    }
    for (int i = 0; i < numSections; ++i)
    {
        rootsSoloParameterIds[(size_t)i] = rootsRootSoloId(i);
        apvts.addParameterListener(rootsSoloParameterIds[(size_t)i], this);
    }
}

PRISMVSTAudioProcessor::~PRISMVSTAudioProcessor()
{
    cancelPendingUpdate();
    for (const auto& id : soloParameterIds)
        apvts.removeParameterListener(id, this);
    for (const auto& id : rootsSoloParameterIds)
        apvts.removeParameterListener(id, this);

    analyzerWorker.signalThreadShouldExit();
    analyzerEvent.signal();
    analyzerWorker.stopThread(-1); // Indefinite clean join: never force-kill the FFT worker.
}

int PRISMVSTAudioProcessor::getActiveSoloSection() const noexcept
{
    const int recent = latestSoloSection.load();
    if (recent >= 0 && recent < numSections
        && read(apvts, soloParameterIds[(size_t)recent]) >= 0.5f)
        return recent;

    for (int s = 0; s < numSections; ++s)
        if (read(apvts, soloParameterIds[(size_t)s]) >= 0.5f)
            return s;

    return -1;
}

int PRISMVSTAudioProcessor::getActiveRootsSolo() const noexcept
{
    const int recent=latestRootsSolo.load();
    if(recent>=0&&recent<numSections
       &&rootParameters[(size_t)recent].values[11]->load()>=0.5f)
        return recent;
    for(int root=0;root<numSections;++root)
        if(rootParameters[(size_t)root].values[11]->load()>=0.5f)
            return root;
    return -1;
}

// Selecting a node only moves the existing SOLO if it is already engaged
// AND the newly selected handle belongs to a different ROOT.
void PRISMVSTAudioProcessor::transferRootsSoloTo(int node)
{
    const int previous=getActiveRootsSolo();
    if(previous<0||node<0||node>=prism::roots::v2::kNodes)return;
    const int root=node/2;
    if(root==previous)return;
    if(auto* p=apvts.getParameter(rootsSoloParameterIds[(size_t)root]))
        p->setValueNotifyingHost(p->convertTo0to1(1.0f));
    latestRootsSolo.store(root);
    reconcileExclusiveRootsSolo();
}

void PRISMVSTAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    for (int s = 0; s < numSections; ++s)
    {
        if (parameterID != soloParameterIds[(size_t)s])
            continue;

        if (newValue >= 0.5f)
            latestSoloSection.store(s);
        else if (latestSoloSection.load() == s)
            latestSoloSection.store(-1);

        // The parameter callback can arrive on the audio thread. Never
        // notify the host or mutate other parameters from here.
        triggerAsyncUpdate();
        return;
    }
    for (int i = 0; i < numSections; ++i)
    {
        if (parameterID != rootsSoloParameterIds[(size_t)i])
            continue;
        if (newValue >= 0.5f)
            latestRootsSolo.store(i);
        else if (latestRootsSolo.load() == i)
            latestRootsSolo.store(-1);
        triggerAsyncUpdate();
        return;
    }
}

void PRISMVSTAudioProcessor::handleAsyncUpdate()
{
    reconcileExclusiveSolo();
    reconcileExclusiveRootsSolo();
}

void PRISMVSTAudioProcessor::reconcileExclusiveSolo()
{
    const int active = getActiveSoloSection();
    if (active < 0)
        return;

    for (int s = 0; s < numSections; ++s)
        if (s != active && read(apvts, soloParameterIds[(size_t)s]) >= 0.5f)
            if (auto* p = apvts.getParameter(soloParameterIds[(size_t)s]))
                p->setValueNotifyingHost(p->convertTo0to1(0.0f));
}

void PRISMVSTAudioProcessor::reconcileExclusiveRootsSolo()
{
    const int active = getActiveRootsSolo();
    if (active < 0)
        return;
    for (int i = 0; i < numSections; ++i)
    {
        if (i == active) continue;
        if (read(apvts, rootsSoloParameterIds[(size_t)i]) >= 0.5f)
            if (auto* p = apvts.getParameter(rootsSoloParameterIds[(size_t)i]))
                p->setValueNotifyingHost(p->convertTo0to1(0.0f));
    }
}

void PRISMVSTAudioProcessor::releaseResources()
{
    analyzerWorker.signalThreadShouldExit();
    analyzerEvent.signal();
    analyzerWorker.stopThread(-1); // Indefinite clean join: never force-kill the FFT worker.
}

bool PRISMVSTAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PRISMVSTAudioProcessor::sanitizeCrossovers()
{
    constexpr float minRatio = 1.08f;
    float previous = 20.0f;

    for (int i = 0; i < numCrossovers; ++i)
    {
        const float remaining = (float)(numCrossovers - 1 - i);
        const float maxForIndex = 20000.0f / std::pow(minRatio, remaining + 1.0f);
        const float low = previous * minRatio;
        const float current = read(apvts, crossoverId(i));
        const float corrected = juce::jlimit(low, maxForIndex, current);

        if (auto* p = apvts.getParameter(crossoverId(i)))
            if (std::abs(corrected - current) > 0.001f)
                p->setValueNotifyingHost(p->convertTo0to1(corrected));

        previous = corrected;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout PRISMVSTAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    juce::NormalisableRange<float> crossoverRange(20.0f, 20000.0f, 0.01f);
    crossoverRange.setSkewForCentre(1000.0f);

    for (int i = 0; i < numCrossovers; ++i)
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { crossoverId(i), 1 },
            "Crossover " + juce::String(i + 1),
            crossoverRange, kDefaultCrossovers[(size_t)i]));

    juce::NormalisableRange<float> attackRange(0.10f, 250.0f, 0.01f);
    attackRange.setSkewForCentre(10.0f);
    juce::NormalisableRange<float> releaseRange(5.0f, 2500.0f, 0.1f);
    releaseRange.setSkewForCentre(180.0f);

    for (int i = 0; i < numSections; ++i)
    {
        const auto prefix = juce::String(sectionName(i)) + " ";

        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { sectionId(i, "on"), 1 }, prefix + "On", true));
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { sectionId(i, "solo"), 1 }, prefix + "Solo", false));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { sectionId(i, "input_db"), 1 }, prefix + "Input",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { sectionId(i, "output_db"), 1 }, prefix + "Output",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { sectionId(i, "attack_ms"), 1 }, prefix + "Attack",
            attackRange, 10.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { sectionId(i, "release_ms"), 1 }, prefix + "Release",
            releaseRange, 180.0f));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { sectionId(i, "width_pct"), 1 }, prefix + "Stereo Width",
            juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f), 100.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { sectionId(i, "onyx_pct"), 1 }, prefix + "ONYX",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

        // The transfer map owns these. They are intentionally not duplicated as knobs.
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { sectionId(i, "threshold_db"), 1 }, prefix + "Curve Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.01f), -18.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { sectionId(i, "ratio"), 1 }, prefix + "Curve Ratio",
            juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f), 1.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { sectionId(i, "curve"), 1 }, prefix + "Curve Shape",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));
    }

    // ROOTS replaces the visible six-knob interface. Keep legacy parameter
    // IDs for saved Alpha compatibility; the dedicated ROOTS path ignores them.
    p.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "roots_engine_mode", 1 }, "ROOTS Engine", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "roots_mono", 1 }, "ROOTS Mono", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "scalpel_xray", 1 }, "X-RAY Visualizer", true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "roots_space", 1 }, "ROOTS Space",
        juce::NormalisableRange<float>(0.0f,100.0f,0.1f),50.0f));

    for(int root=0;root<numSections;++root)
    {
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { rootsRootSoloId(root), 1 },
            juce::String(sectionName(root)) + " ROOT Solo",false));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { rootsRootGainId(root), 1 },
            juce::String(sectionName(root)) + " ROOT Gain",
            juce::NormalisableRange<float>(-60.0f,60.0f,0.01f),0.0f));
        const auto id=[root](const char* suffix){return rootsRootControlId(root,suffix);};
        const auto label=juce::String(sectionName(root))+" ";
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{id("on"),1},label+"Enabled",true));
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{id("mono"),1},label+"Mono Within Territory",false));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id("space"),1},label+"Band Space",
            juce::NormalisableRange<float>(0.0f,100.0f,0.1f),50.0f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id("threshold"),1},label+"Compression Threshold",
            juce::NormalisableRange<float>(-60.0f,0.0f,0.1f),0.0f));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{id("comp"),1},label+"Compression Profile",
            juce::StringArray{"OFF","OPTICAL 3:1","OPTICAL 4:1","CLB1 TUBE 3:1","FAST 4:1"},0));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{id("domain"),1},label+"EQ Domain",
            juce::StringArray{"STEREO","MID","SIDE"},0));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{id("detector"),1},label+"Compression Domain",
            juce::StringArray{"STEREO LINK","DUAL MID/SIDE","MID ONLY","SIDE ONLY"},0));
    }

    // The v1.0 global ONYX IDs remain registered for session compatibility;
    // they no longer alter the master bus.
    for (int root=0;root<numSections;++root)
        for(const auto& fx:std::array<std::pair<const char*,const char*>,4> {{
            {"sparkle","Sparkle"},{"shine","Shine"},{"smooth","Smoothening"},{"tape","Vintage Tape"} }})
            p.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { rootsRootControlId(root,juce::String("onyx_")+fx.first), 1 },
                juce::String(sectionName(root))+" ONYX "+fx.second,
                juce::NormalisableRange<float>(0.0f,100.0f,0.1f),0.0f));
    // Newly allocated IDs cannot collide with existing ONYX/CARVE automation.
    for(int root=0;root<numSections;++root){
        const auto label=juce::String(sectionName(root))+" ";
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{rootsRootControlId(root,"color_dynamic"),1},label+"COLOR Dynamic",
            juce::NormalisableRange<float>(0.0f,100.0f,0.1f),0.0f));
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{rootsRootControlId(root,"phase"),1},label+"PHASE Auto Coherence",false));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{rootsRootControlId(root,"limit_amount"),1},label+"LIMIT Optical Amount",
            juce::NormalisableRange<float>(0.0f,100.0f,0.1f),0.0f));
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{rootsRootControlId(root,"limit_on"),1},label+"LIMIT Optical Enabled",false));
    }
    for (const auto& effect : std::array<std::pair<const char*, const char*>,4> {{
        {"surgery_onyx_sparkle","ONYX Sparkle"}, {"surgery_onyx_shine","ONYX Shine"},
        {"surgery_onyx_smooth","ONYX Smoothening"}, {"surgery_onyx_tape","ONYX Vintage Tape"} }})
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { effect.first, 1 }, effect.second,
            juce::NormalisableRange<float>(0.0f,100.0f,0.1f),0.0f));
    // Deprecated per-handle effect IDs are retained for old presets, but ignored
    // by the ROOTS engine. Only the two Hz handles remain active.
    for (int i=0;i<prism::roots::v2::kNodes;++i)
    {
        using namespace prism::roots::v2;
        const auto id=[i](const char* suffix){return rootsNodeId(i,suffix);};
        const juce::String name="ROOTS " + juce::String(i+1) + " ";
        juce::NormalisableRange<float> hzRange(20.0f,20000.0f,0.01f);
        hzRange.setSkewForCentre(1000.0f);
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id("hz"),1}, name + "Hz",hzRange,(float)Engine::defaults[(size_t)i]));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id("gain_db"),1}, name + "Gain",
            juce::NormalisableRange<float>(-60.0f,60.0f,0.01f),0.0f));
        juce::NormalisableRange<float> qRange(0.15f,24.0f,0.01f);
        qRange.setSkewForCentre(1.0f);
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id("q"),1}, name + "Q",qRange,1.5f));
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{id("on"),1},name + "On",true));
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{id("solo"),1},name + "Solo",false));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{id("domain"),1},name + "EQ Domain",
            juce::StringArray{"STEREO","MID","SIDE"},0));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id("space"),1},name + "Band Space",
            juce::NormalisableRange<float>(0.0f,100.0f,0.1f),50.0f));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{id("comp"),1},name + "Compressor",
            juce::StringArray{"OFF","OPTICAL 3:1","OPTICAL 4:1","CLB1 TUBE 3:1","FAST 4:1"},0));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{id("detector"),1},name + "Dynamics Domain",
            juce::StringArray{"STEREO LINK","DUAL MID/SIDE","MID ONLY","SIDE ONLY"},0));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id("threshold"),1},name + "Threshold",
            juce::NormalisableRange<float>(-60.0f,0.0f,0.1f),-18.0f));
    }

    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "analyzer_depth", 2 }, "Analyzer Depth",
        juce::StringArray { "FULL -96", "DEEP -120", "FORENSIC -144" }, 0));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "analyzer_slope", 2 }, "Analyzer Display Slope",
        juce::NormalisableRange<float>(0.0f, 6.0f, 0.1f), 0.0f));

    juce::NormalisableRange<float> auraMemoryRange(0.05f, 10.0f, 0.01f);
    auraMemoryRange.setSkewForCentre(0.4f);
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "aura_memory", 2 }, "Spectrum Release Time",
        auraMemoryRange, 0.18f));

    p.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "aura_color", 1 }, "Measured Spectral Energy Fill", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "surgery_heat_view", 1 }, "Legacy Spectrum View (Retired)", false));

    // Four original surgically controllable EQ nodes in each of seven ROOTS.
    for(int root=0;root<surgery::kRoots;++root)
        for(int slot=0;slot<surgery::kSlots;++slot) {
            const auto id=[root,slot](const char* s){return carveId(root,slot,s);};
            const auto name=juce::String("CARVE ")+juce::String(root+1)+"/"+juce::String(slot+1)+" ";
            p.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{id("on"),1},name+"Enabled",false));
            juce::NormalisableRange<float> range(20.0f,20000.0f,0.01f);
            range.setSkewForCentre(1000.0f);
            p.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{id("hz"),1},name+"Frequency",range,
                surgery::defaultCarveHz(root,slot)));
            p.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{id("depth"),1},name+"Cut Depth",
                juce::NormalisableRange<float>(0.0f,36.0f,0.01f),0.0f));
            juce::NormalisableRange<float> q(0.3f,96.0f,0.001f);q.setSkewForCentre(4.0f);
            p.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{id("q"),1},name+"Q",q,2.0f));
            p.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{id("threshold"),1},name+"Threshold",
                juce::NormalisableRange<float>(-60.0f,0.0f,0.1f),-24.0f));
            p.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{id("dynamic"),1},name+"Dynamic",false));
            p.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{id("domain"),1},name+"Channel",
                juce::StringArray{"STEREO","MID","SIDE"},0));
        }
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"surgery_theme",1},"Surgery UI Theme",
        juce::StringArray{"ONYX","ETHER LAB","ALIEN GRID","CINEMA VOID"},0));
    static constexpr unsigned colours[]{0x6BBDD4,0xDFA871,0xE6BD6A,0x8DCEA8,0x9DADEA,0xD2A2E5,0xE89EAD};
    for(int root=0;root<7;++root)
        p.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"surgery_root"+juce::String(root+1)+"_rgb",1},
            "ROOT "+juce::String(root+1)+" Colour",0,16777215,(int)colours[root]));
    // Append new IDs so every historical parameter and its ordering stays intact.
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "surgery_bypass", 1 }, "Surgery Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "surgery_output_db", 1 }, "Surgery Output Trim",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.01f), 0.0f));
    // Optional visual-only append: historical IDs, ordering and defaults stay intact.
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"sonar_weather_mode",1},"SONAR Visual Mode",juce::StringArray{"STANDARD","STORM SYMBIOTE"},0));
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sonar_reduced_motion",1},"SONAR Reduced Motion",true));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"sonar_visual_style",1},"SONAR Visual Style",juce::StringArray{"Standard","Storm Symbiote","Synesthesia","Brooding","Pulse","Aura","Focus","Standard+"},0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"juke_mode",1},"JUKE Mode",juce::StringArray{"Real Jukebox","Storm Tube","Symbiote Flow","Pulse Rail","Ambient Idle","Off"},0));
    return { p.begin(), p.end() };
}

void PRISMVSTAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // No lookahead or intentional buffering latency in PRISM.
    setLatencySamples(0);

    analyzerWorker.signalThreadShouldExit();
    analyzerEvent.signal();
    analyzerWorker.stopThread(-1); // Indefinite clean join: never force-kill the FFT worker.

    currentSampleRate = sampleRate;
    dryScratch.setSize(2, juce::jmax(1, samplesPerBlock));
    outputTrimSmoother.reset(sampleRate, 0.02);
    outputTrimSmoother.setCurrentAndTargetValue(toGain(trimValue->load()));
    bypassMix.reset(sampleRate, 0.005);
    bypassMix.setCurrentAndTargetValue(bypassValue->load() >= 0.5f ? 1.0f : 0.0f);
    inputPeakDb.store(kFloorDb); peakDb.store(kFloorDb); peakHoldDb.store(kFloorDb); gainReductionDb.store(0);
    lufsShort.store(kFloorDb); lufsIntegrated.store(kFloorDb);
    rootMeters[0].store(1); rootMeters[1].store(0); rootMeters[2].store(0); rootMeters[3].store(1); rootMeters[4].store(0);
    for(auto& peak:rootBandPeakDb)peak.store(-144.f);
    rootsEngine.prepare(sampleRate);
    for(auto& state:onyxFinish)state={};
    carveEngine.prepare(sampleRate);
    sanitizeCrossovers();

    for (auto& section : sectionStates)
    {
        section.envelope = 0.0f;
        section.gainReductionDb = 0.0f;
        section.onyxLow.fill(0.0f);
    }
    for (auto& v : stereoActivity)
        v.store(0.0f);

    for (int i = 0; i < numCrossovers; ++i)
    {
        const float f = read(apvts, crossoverId(i));
        crossoverSmoothers[(size_t)i].reset(sampleRate, 0.05);
        crossoverSmoothers[(size_t)i].setCurrentAndTargetValue(f);

        for (int c = 0; c < 2; ++c)
        {
            splitters[(size_t)i][(size_t)c].reset();
            splitters[(size_t)i][(size_t)c].setCutoff(sampleRate, f);
        }

        for (int j = 0; j < numSections; ++j)
            for (int c = 0; c < 2; ++c)
            {
                compensators[(size_t)i][(size_t)j][(size_t)c].reset();
                compensators[(size_t)i][(size_t)j][(size_t)c].setCutoff(sampleRate, f);
            }
    }

    juce::dsp::ProcessSpec monoSpec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        1
    };

    auto hpC = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 38.0);
    auto shC = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 1681.0, 0.7071f, toGain(4.0f));

    for (int c = 0; c < 2; ++c)
    {
        loudHp[(size_t)c].reset();
        loudShelf[(size_t)c].reset();
        *loudHp[(size_t)c].coefficients = *hpC;
        *loudShelf[(size_t)c].coefficients = *shC;
        loudHp[(size_t)c].prepare(monoSpec);
        loudShelf[(size_t)c].prepare(monoSpec);
    }

    analyzerFifo.reset();
    analyzerQueue.fill(0.0f);analyzerRightQueue.fill(0.0f);
    fftWritePos = 0;
    fftHopCounter = 0;
    fftFifo.fill(0.0f);fftRightFifo.fill(0.0f);leftSpectralPower.fill(0.0f);
    fftData.fill(0.0f);
    for (auto& v : spectrum)
        v.store(kFloorDb);
    labAnalyzer.reset();
    prepareWeatherAnalysis();
    for (auto& v : labDb) v.store(prism::analysis::LabBands::floorDb);
    for (auto& v : labPeakDb) v.store(prism::analysis::LabBands::floorDb);

    integratedEnergy = 0.0;
    integratedSamples = 0;
    analyzerWorker.startThread();
}

void PRISMVSTAudioProcessor::updateCrossoverCoefficients(int blockSamples)
{
    for (int i = 0; i < numCrossovers; ++i)
    {
        auto& smoother = crossoverSmoothers[(size_t)i];
        smoother.setTargetValue(read(apvts, crossoverId(i)));
        const float f = smoother.skip(blockSamples);

        for (int c = 0; c < 2; ++c)
            splitters[(size_t)i][(size_t)c].setCutoff(currentSampleRate, f);

        for (int j = 0; j < i; ++j)
            for (int c = 0; c < 2; ++c)
                compensators[(size_t)i][(size_t)j][(size_t)c].setCutoff(currentSampleRate, f);
    }
}

void PRISMVSTAudioProcessor::processSevenSectionEngine(juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    updateCrossoverCoefficients(n);

    std::array<bool, numSections> on {};
    // SOLO is exclusive: the most recently activated section wins.
    // Any stale overlapping host automation is audibly resolved immediately,
    // then reconciled to one true SOLO parameter on the message thread.
    const int activeSolo = getActiveSoloSection();

    std::array<float, numSections> inputGain {}, outputGain {}, attackCoeff {}, releaseCoeff {};
    std::array<float, numSections> width {}, onyx {}, threshold {}, ratio {}, curve {};

    const float sr = (float)currentSampleRate;

    for (int s = 0; s < numSections; ++s)
    {
        on[(size_t)s] = read(apvts, sectionId(s, "on")) >= 0.5f;
        inputGain[(size_t)s] = toGain(read(apvts, sectionId(s, "input_db")));
        outputGain[(size_t)s] = toGain(read(apvts, sectionId(s, "output_db")));

        const float attackMs = juce::jmax(0.10f, read(apvts, sectionId(s, "attack_ms")));
        const float releaseMs = juce::jmax(5.0f, read(apvts, sectionId(s, "release_ms")));
        attackCoeff[(size_t)s] = std::exp(-1.0f / (0.001f * attackMs * sr));
        releaseCoeff[(size_t)s] = std::exp(-1.0f / (0.001f * releaseMs * sr));

        // Preserve the v1.1.1 parameter ID/range for project recall.
        // 100% stored = neutral; 150% stored = maximum +50% widening.
        // Old values outside that territory never narrow or over-expand.
        width[(size_t)s] = juce::jlimit(1.0f, 1.5f,
            read(apvts, sectionId(s, "width_pct")) * 0.01f);
        onyx[(size_t)s] = juce::jlimit(0.0f, 1.0f, read(apvts, sectionId(s, "onyx_pct")) * 0.01f);
        threshold[(size_t)s] = read(apvts, sectionId(s, "threshold_db"));
        ratio[(size_t)s] = juce::jlimit(1.0f, 20.0f, read(apvts, sectionId(s, "ratio")));
        curve[(size_t)s] = juce::jlimit(-1.0f, 1.0f, read(apvts, sectionId(s, "curve")));
    }

    float maxReduction = 0.0f;
    std::array<double, numSections> midEnergy {};
    std::array<double, numSections> sideEnergy {};
    // One-pole split for a very small input-derived high-frequency sheen.
    const float onyxToneCoeff = 1.0f - std::exp(
        -2.0f * juce::MathConstants<float>::pi * 1800.0f / sr);
    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);

    for (int i = 0; i < n; ++i)
    {
        std::array<float, numSections> l {};
        std::array<float, numSections> r {};

        float remL = left[i];
        float remR = right[i];

        for (int x = 0; x < numCrossovers; ++x)
        {
            // Phase-compensate already extracted lower sections for this later split.
            for (int lower = 0; lower < x; ++lower)
            {
                l[(size_t)lower] = compensators[(size_t)x][(size_t)lower][0].process(l[(size_t)lower]);
                r[(size_t)lower] = compensators[(size_t)x][(size_t)lower][1].process(r[(size_t)lower]);
            }

            float lowL = 0.0f, highL = 0.0f;
            float lowR = 0.0f, highR = 0.0f;
            splitters[(size_t)x][0].process(remL, lowL, highL);
            splitters[(size_t)x][1].process(remR, lowR, highR);

            l[(size_t)x] = lowL;
            r[(size_t)x] = lowR;
            remL = highL;
            remR = highR;
        }

        l[(size_t)(numSections - 1)] = remL;
        r[(size_t)(numSections - 1)] = remR;

        float outL = 0.0f;
        float outR = 0.0f;

        for (int s = 0; s < numSections; ++s)
        {
            const bool audible = on[(size_t)s] && (activeSolo < 0 || s == activeSolo);
            if (!audible)
                continue;

            float xL = l[(size_t)s] * inputGain[(size_t)s];
            float xR = r[(size_t)s] * inputGain[(size_t)s];

            auto& state = sectionStates[(size_t)s];
            const float detected = juce::jmax(std::abs(xL), std::abs(xR));
            const float envCoeff = detected > state.envelope
                ? attackCoeff[(size_t)s] : releaseCoeff[(size_t)s];
            state.envelope = envCoeff * state.envelope + (1.0f - envCoeff) * detected;

            const float levelDb = toDb(state.envelope + 1.0e-9f);
            const float overDb = juce::jmax(0.0f, levelDb - threshold[(size_t)s]);
            const float exponent = juce::jmap(curve[(size_t)s], -1.0f, 1.0f, 0.65f, 1.75f);
            const float shapedOver = overDb > 0.0f
                ? 24.0f * std::pow(overDb / 24.0f, exponent)
                : 0.0f;
            state.gainReductionDb = shapedOver * (1.0f - 1.0f / ratio[(size_t)s]);
            maxReduction = juce::jmax(maxReduction, state.gainReductionDb);

            const float dynamicGain = toGain(-state.gainReductionDb);
            xL *= dynamicGain;
            xR *= dynamicGain;

            const float a = onyx[(size_t)s];
            // Resonant Wax: gentle differential soft clipping gives fine,
            // input-dependent harmonics without a loud saturation jump.
            // The high-passed component adds restrained musical sparkle
            // from signal content, never noise or an oscillator.
            const float drive = 1.0f + 0.9f * a;
            const float bias = 0.065f * a;
            const float zero = std::tanh(bias * drive);
            auto wax = [&](float sample, float& low)
            {
                low += onyxToneCoeff * (sample - low);
                if (a <= 0.0001f)
                    return sample; // bit-exact ONYX bypass

                const float coloured = (std::tanh((sample + bias) * drive) - zero) / drive;
                const float sheen = (sample - low) * (0.045f * a);
                const float mix = 0.40f * a;
                return (sample + mix * (coloured - sample) + sheen) * (1.0f - 0.015f * a);
            };
            xL = wax(xL, state.onyxLow[0]);
            xR = wax(xR, state.onyxLow[1]);

            const float mid = 0.5f * (xL + xR);
            const float side = 0.5f * (xL - xR) * width[(size_t)s];
            xL = (mid + side) * outputGain[(size_t)s];
            xR = (mid - side) * outputGain[(size_t)s];

            // Meter actual processed stereo activity, not knob position.
            // This feeds only the screen; never feeds any DSP decision.
            const double outMid = 0.5 * ((double)xL + (double)xR);
            const double outSide = 0.5 * ((double)xL - (double)xR);
            midEnergy[(size_t)s] += outMid * outMid;
            sideEnergy[(size_t)s] += outSide * outSide;

            outL += xL;
            outR += xR;
        }

        left[i] = outL;
        right[i] = outR;
    }

    gainReductionDb.store(maxReduction);
    for (int s = 0; s < numSections; ++s)
    {
        const double m = midEnergy[(size_t)s];
        const double side = sideEnergy[(size_t)s];
        const float measured = (float)std::sqrt(side / (side + m + 1.0e-15));
        const float previous = stereoActivity[(size_t)s].load();
        stereoActivity[(size_t)s].store(0.72f * previous + 0.28f * measured);
    }
}

void PRISMVSTAudioProcessor::pushAnalyzerSamples(const juce::AudioBuffer<float>& buffer)
{
    const int requested = buffer.getNumSamples();
    const float* left = buffer.getReadPointer(0);
    const float* right = buffer.getReadPointer(1);

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    analyzerFifo.prepareToWrite(requested, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i){
        analyzerQueue[(size_t)(start1 + i)] = left[i];
        analyzerRightQueue[(size_t)(start1 + i)] = right[i];
    }

    for (int i = 0; i < size2; ++i)
    {
        const int source = size1 + i;
        analyzerQueue[(size_t)(start2 + i)] = left[source];
        analyzerRightQueue[(size_t)(start2 + i)] = right[source];
    }

    const int written = size1 + size2;
    analyzerFifo.finishedWrite(written);
    if (written > 0)
        analyzerEvent.signal();
}

void PRISMVSTAudioProcessor::runAnalyzerThread()
{
    while (!analyzerWorker.threadShouldExit())
    {
        analyzerEvent.wait(10);
        if (analyzerWorker.threadShouldExit())
            break;
        for (;;)
        {
            const int ready = analyzerFifo.getNumReady();
            if (ready <= 0)
                break;

            const int amount = juce::jmin(ready, 4096);
            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            analyzerFifo.prepareToRead(amount, start1, size1, start2, size2);

            for (int i = 0; i < size1; ++i)
                consumeAnalyzerSample(analyzerQueue[(size_t)(start1 + i)],analyzerRightQueue[(size_t)(start1 + i)]);
            for (int i = 0; i < size2; ++i)
                consumeAnalyzerSample(analyzerQueue[(size_t)(start2 + i)],analyzerRightQueue[(size_t)(start2 + i)]);

            analyzerFifo.finishedRead(size1 + size2);
            if (analyzerWorker.threadShouldExit())
                break;
        }
    }
}

void PRISMVSTAudioProcessor::consumeAnalyzerSample(float left, float right)
{
    fftFifo[(size_t)fftWritePos] = left;
    fftRightFifo[(size_t)fftWritePos] = right;
    fftWritePos = (fftWritePos + 1) % fftSize;

    if (++fftHopCounter >= fftHopSize)
    {
        renderSpectrumFrame();
        fftHopCounter = 0;
    }
}

void PRISMVSTAudioProcessor::renderSpectrumFrame()
{
    for (int i = 0; i < fftSize; ++i)
    {
        const int source = (fftWritePos + i) % fftSize;
        fftData[(size_t)i] = fftFifo[(size_t)source];
    }

    for (int i = fftSize; i < fftSize * 2; ++i)
        fftData[(size_t)i] = 0.0f;

    fftWindow.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());
    for (int bin = 0; bin <= fftSize / 2; ++bin)
        leftSpectralPower[(size_t)bin] = fftData[(size_t)bin] * fftData[(size_t)bin];
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t)i] = fftRightFifo[(size_t)((fftWritePos + i) % fftSize)];
    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
    fftWindow.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());
    // Stereo power average preserves mono calibration and cannot phase-cancel.
    for (int bin = 0; bin <= fftSize / 2; ++bin)
        fftData[(size_t)bin] = std::sqrt(0.5f * (leftSpectralPower[(size_t)bin]
                                             + fftData[(size_t)bin] * fftData[(size_t)bin]));

    // PRISM Analysis 256-bar field, Hann-coherent-gain calibrated.
    // Kept exclusively on the analyzer worker; no audio DSP interaction.
    labAnalyzer.processMagnitudes(fftData.data(), fftSize / 2 + 1,
                                  fftSize, currentSampleRate, (double)fftSize);
    const auto& labFrame = labAnalyzer.frame();
    for (int i = 0; i < labBars; ++i)
    {
        labDb[(size_t)i].store(labFrame[(size_t)i].db);
        labPeakDb[(size_t)i].store(labFrame[(size_t)i].peakDb);
    }

    const int maxBin = fftSize / 2;
    const double sr = juce::jmax(1.0, currentSampleRate);

    for (int i = 0; i < spectrumBins; ++i)
    {
        const float centrePos = (float)i / (float)(spectrumBins - 1);
        const float loPos = juce::jlimit(0.0f, 1.0f, ((float)i - 0.5f) / (float)(spectrumBins - 1));
        const float hiPos = juce::jlimit(0.0f, 1.0f, ((float)i + 0.5f) / (float)(spectrumBins - 1));

        const float centreHz = 20.0f * std::pow(1000.0f, centrePos);
        const float loHz = 20.0f * std::pow(1000.0f, loPos);
        const float hiHz = 20.0f * std::pow(1000.0f, hiPos);

        const int loBin = juce::jlimit(1, maxBin, (int)std::floor(loHz * (double)fftSize / sr));
        const int hiBin = juce::jlimit(loBin, maxBin, (int)std::ceil(hiHz * (double)fftSize / sr));

        float peakMagnitude = 0.0f;
        for (int bin = loBin; bin <= hiBin; ++bin)
            peakMagnitude = juce::jmax(peakMagnitude, (2.0f * fftData[(size_t)bin]) / (float)fftSize);

        if (peakMagnitude <= 0.0f)
        {
            const int nearest = juce::jlimit(1, maxBin,
                juce::roundToInt(centreHz * (float)fftSize / (float)sr));
            peakMagnitude = (2.0f * fftData[(size_t)nearest]) / (float)fftSize;
        }

        spectrum[(size_t)i].store(toDb(peakMagnitude + 1.0e-12f));
    }
    publishWeatherAnalysis();
}

void PRISMVSTAudioProcessor::copySpectrum(std::array<float, spectrumBins>& destination) const noexcept
{
    for (int i = 0; i < spectrumBins; ++i)
        destination[(size_t)i] = spectrum[(size_t)i].load();
}

void PRISMVSTAudioProcessor::copyPrismLabBars(
    std::array<float, labBars>& db,
    std::array<float, labBars>& peakValues) const noexcept
{
    for (int i = 0; i < labBars; ++i)
    {
        db[(size_t)i] = labDb[(size_t)i].load();
        peakValues[(size_t)i] = labPeakDb[(size_t)i].load();
    }
}

void PRISMVSTAudioProcessor::updateMeters(const juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    float peak = 0.0f;
    double weightedEnergy = 0.0;

    for (int c = 0; c < juce::jmin(2, buffer.getNumChannels()); ++c)
    {
        const float* x = buffer.getReadPointer(c);
        for (int i = 0; i < n; ++i)
        {
            peak = juce::jmax(peak, std::abs(x[i]));
            float y = loudHp[(size_t)c].processSample(x[i]);
            y = loudShelf[(size_t)c].processSample(y);
            weightedEnergy += (double)y * (double)y;
        }
    }

    peakDb.store(toDb(peak + 1.0e-9f));
    auto held=peakHoldDb.load();
    const float measured=peakDb.load();
    while(measured>held&&!peakHoldDb.compare_exchange_weak(held,measured)) {}
    double leftPower=0,rightPower=0,cross=0,midPower=0,sidePower=0;
    const auto* left=buffer.getReadPointer(0);const auto* right=buffer.getReadPointer(1);
    for(int i=0;i<n;++i){
        const double l=left[i],r=right[i],m=0.5*(l+r),s=0.5*(l-r);
        leftPower+=l*l;rightPower+=r*r;cross+=l*r;midPower+=m*m;sidePower+=s*s;
    }
    rootMeters[0].store(leftPower>1e-20&&rightPower>1e-20?
        std::clamp(cross/std::sqrt(leftPower*rightPower),-1.0,1.0):1.0);
    rootMeters[1].store(std::sqrt(midPower/n));rootMeters[2].store(std::sqrt(sidePower/n));
    if(bypassMix.getCurrentValue()>=1.0f){
        gainReductionDb.store(0);rootMeters[4].store(0);
        for(auto& band:rootBandPeakDb)band.store(-144.f);
    }

    const auto count = (uint64_t)n * 2u;
    integratedEnergy += weightedEnergy;
    integratedSamples += count;

    const double blockMean = count > 0 ? weightedEnergy / (double)count : 0.0;
    const double intMean = integratedSamples > 0 ? integratedEnergy / (double)integratedSamples : 0.0;

    lufsShort.store(blockMean > 1.0e-12
        ? (float)(-0.691 + 10.0 * std::log10(blockMean)) : kFloorDb);
    lufsIntegrated.store(intMean > 1.0e-12
        ? (float)(-0.691 + 10.0 * std::log10(intMean)) : kFloorDb);
}

void PRISMVSTAudioProcessor::processRootsEngine(juce::AudioBuffer<float>& buffer)
{
    using namespace prism::roots::v2;
    // Historical global SPACE/MONO parameters remain for session recall,
    // but they are no longer exposed or allowed to alter this audio path.
    rootsEngine.setSpace(50.0);
    rootsEngine.setMono(false);
    rootsEngine.setSoloRoot(getActiveRootsSolo());
    for(int root=0;root<numSections;++root){
        rootsEngine.setRootGain(root,rootParameters[(size_t)root].values[0]->load());
        rootsEngine.setRootMono(root,
            rootParameters[(size_t)root].values[1]->load()>=0.5f);
    }
    for(int i=0;i<kNodes;++i)
    {
        Config c;
        c.hz=nodeParameters[(size_t)i][0]->load();
        c.gainDb=0.0; // retained obsolete host parameter is ignored
        c.q=nodeParameters[(size_t)i][1]->load();
        const int root=i/2;
        c.enabled=rootParameters[(size_t)root].values[2]->load()>=0.5f;
        c.solo=false; // only root-level SOLO is authoritative
        c.domain=(Domain)juce::roundToInt(rootParameters[(size_t)root].values[3]->load());
        c.space=50.0; // deprecated invisible ROOT SPACE neutralized
        c.comp=(Compressor)juce::roundToInt(rootParameters[(size_t)root].values[4]->load());
        c.detector=(Detector)juce::roundToInt(rootParameters[(size_t)root].values[5]->load());
        c.thresholdDb=rootParameters[(size_t)root].values[6]->load();
        c.sparkle=juce::jlimit(0.0f,1.0f,rootParameters[(size_t)root].values[7]->load()*0.01f);
        c.shine=juce::jlimit(0.0f,1.0f,rootParameters[(size_t)root].values[8]->load()*0.01f);
        c.smooth=juce::jlimit(0.0f,1.0f,rootParameters[(size_t)root].values[9]->load()*0.01f);
        c.tape=juce::jlimit(0.0f,1.0f,rootParameters[(size_t)root].values[10]->load()*0.01f);
        c.dynamic=juce::jlimit(0.0f,1.0f,rootParameters[(size_t)root].values[12]->load()*0.01f);
        c.phase=rootParameters[(size_t)root].values[13]->load()>=0.5f;
        // The front panel has a dedicated LIMIT switch but no third rotary.
        // A previously neutral 0% hidden amount must not turn LIMIT into a
        // placebo button. Use a musical optical default; the retained host
        // parameter can still override it for saved advanced automation.
        const float amount=rootParameters[(size_t)root].values[14]->load();
        c.limit=rootParameters[(size_t)root].values[15]->load()>=0.5f?
            (amount<=0.001f?0.35f:juce::jlimit(0.0f,1.0f,amount*0.01f)):0.0f;
        rootsEngine.config(i,c);
    }
    if(buffer.getNumChannels()>=2)
        rootsEngine.process(buffer.getWritePointer(0),
                            buffer.getWritePointer(1),
                            (size_t)buffer.getNumSamples());
    // Bounds come from actual ROOT handle positions, with smooth acoustic tails.
    for(int root=0;root<surgery::kRoots;++root) {
        const float lo=nodeParameters[(size_t)(root*2)][0]->load();
        const float hi=nodeParameters[(size_t)(root*2+1)][0]->load();
        carveEngine.bounds(root,lo,hi);
        carveEngine.rootEnabled(root,
            rootParameters[(size_t)root].values[2]->load()>=0.5f);
        carveEngine.rootDomain(root,(surgery::Channel)juce::roundToInt(
            rootParameters[(size_t)root].values[3]->load()));
        for(int slot=0;slot<surgery::kSlots;++slot) {
            const auto& values = carveParameters[(size_t)(root*4+slot)];
            surgery::CarveConfig c;
            c.enabled=values[0]->load()>=0.5f;
            c.hz=values[1]->load();
            c.depthDb=values[2]->load();
            c.q=values[3]->load();
            c.thresholdDb=values[4]->load();
            c.dynamic=values[5]->load()>=0.5f;
            c.channel=(surgery::Channel)juce::roundToInt(values[6]->load());
            carveEngine.configure(root,slot,c);
        }
    }
    if(buffer.getNumChannels()>=2)
        carveEngine.process(buffer.getWritePointer(0),buffer.getWritePointer(1),
                            (size_t)buffer.getNumSamples(),getActiveRootsSolo());
    const auto meters = rootsEngine.meter();
    rootMeters[0].store(meters.correlation); rootMeters[1].store(meters.midRms);
    rootMeters[2].store(meters.sideRms); rootMeters[3].store(meters.spaceApplied);
    rootMeters[4].store(meters.largestGrDb);
    for(int root=0;root<numSections;++root)
        rootBandPeakDb[(size_t)root].store((float)meters.rootPeakDb[(size_t)root],std::memory_order_relaxed);
    // UI may consume a measurement only once. Otherwise stopping the host
    // after a loud block would refresh the red warning indefinitely.
    rootBandPeakRevision_.fetch_add(1,std::memory_order_release);
    gainReductionDb.store((float)meters.largestGrDb);
}

void PRISMVSTAudioProcessor::processOnyxFinish(juce::AudioBuffer<float>& b) noexcept
{
    const float sparkle=juce::jlimit(0.0f,1.0f,read(apvts,"surgery_onyx_sparkle")*0.01f);
    const float shine=juce::jlimit(0.0f,1.0f,read(apvts,"surgery_onyx_shine")*0.01f);
    const float smooth=juce::jlimit(0.0f,1.0f,read(apvts,"surgery_onyx_smooth")*0.01f);
    const float tape=juce::jlimit(0.0f,1.0f,read(apvts,"surgery_onyx_tape")*0.01f);
    if(sparkle+shine+smooth+tape<0.00001f)return;
    const float sr=(float)juce::jmax(8000.0,currentSampleRate);
    const auto alpha=[sr](float hz){return 1.0f-std::exp(-2.0f*juce::MathConstants<float>::pi*hz/sr);};
    const float a1=alpha(4200.0f),a2=alpha(1600.0f),a3=alpha(juce::jmap(smooth,16000.0f,2600.0f)),a4=alpha(8.0f);
    for(int c=0;c<juce::jmin(2,b.getNumChannels());++c){
        auto& s=onyxFinish[(size_t)c];auto* x=b.getWritePointer(c);
        for(int i=0;i<b.getNumSamples();++i){
            float y=x[i];
            s.air+=a1*(y-s.air);
            if(sparkle>0)y+=0.20f*sparkle*std::tanh((y-s.air)*(1.0f+sparkle));
            s.presence+=a2*(y-s.presence);
            if(shine>0)y+=0.30f*shine*(y-s.presence);
            s.smooth+=a3*(y-s.smooth);
            if(smooth>0)y+=0.88f*smooth*(s.smooth-y);
            if(tape>0){
                const float d=1.0f+3.5f*tape;
                y+=tape*(std::tanh(d*y)/std::tanh(d)-y);
                s.dc+=a4*(y-s.dc);y-=s.dc;
            }
            x[i]=y;
        }
    }
}
void PRISMVSTAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() < 2) return;
    for(int c=0;c<2;++c){
        auto* samples=buffer.getWritePointer(c);
        for(int i=0;i<buffer.getNumSamples();++i)
            if(!std::isfinite(samples[i]))samples[i]=0.0f;
    }

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear(c, 0, buffer.getNumSamples());

    float inputPeak = 0.0f;
    for (int c = 0; c < juce::jmin(2, buffer.getNumChannels()); ++c)
        inputPeak = juce::jmax(inputPeak, buffer.getMagnitude(c, 0, buffer.getNumSamples()));
    inputPeakDb.store(toDb(inputPeak + 1.0e-9f));

    outputTrimSmoother.setTargetValue(toGain(trimValue->load()));
    bypassMix.setTargetValue(bypassValue->load() >= 0.5f ? 1.0f : 0.0f);
    const int capacity = dryScratch.getNumSamples();
    if (capacity <= 0) return;
    for (int offset = 0; offset < buffer.getNumSamples(); offset += capacity)
    {
        const int count = juce::jmin(capacity, buffer.getNumSamples() - offset);
        for (int c = 0; c < 2; ++c)
            dryScratch.copyFrom(c, 0, buffer, c, offset, count);
        float* channels[] = { buffer.getWritePointer(0, offset), buffer.getWritePointer(1, offset) };
        juce::AudioBuffer<float> chunk(channels, 2, count);
        if (engineMode->load() >= 0.5f) processRootsEngine(chunk);
        else processSevenSectionEngine(chunk);
        for (int i = 0; i < count; ++i)
        {
            const float gain = outputTrimSmoother.getNextValue();
            const float bypass = bypassMix.getNextValue();
            for (int c = 0; c < 2; ++c)
            {
                const float dry = dryScratch.getSample(c, i);
                const float wet = channels[c][i] * gain;
                channels[c][i] = bypass >= 1.0f ? dry : wet + bypass * (dry - wet);
            }
        }
    }
    pushAnalyzerSamples(buffer);
    updateMeters(buffer);
}

void PRISMVSTAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void PRISMVSTAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())&&finiteParameterState(*xml,apvts))
        {
            // Legacy states always recall at unity, with processing engaged.
            for(const auto* id:{"surgery_bypass","surgery_output_db"})
                if(xml->getChildByAttribute("id",id)==nullptr){
                    auto* parameter=xml->createNewChildElement("PARAM");
                    parameter->setAttribute("id",id);parameter->setAttribute("value",0.0);
                }
            // Older sessions never opt into a new visual mode implicitly.
            for(const auto* id:{"sonar_weather_mode","sonar_reduced_motion"})
                if(xml->getChildByAttribute("id",id)==nullptr){auto* parameter=xml->createNewChildElement("PARAM");parameter->setAttribute("id",id);parameter->setAttribute("value",juce::String(id)=="sonar_reduced_motion"?1.0:0.0);}
            // Retain legacy two-choice automation unchanged. Missing new
            // selectors recall the old visuals rather than opting in silently.
            if(xml->getChildByAttribute("id","sonar_visual_style")==nullptr){auto* v=xml->createNewChildElement("PARAM");v->setAttribute("id","sonar_visual_style");v->setAttribute("value",0.0);}
            if(xml->getChildByAttribute("id","juke_mode")==nullptr){auto* v=xml->createNewChildElement("PARAM");v->setAttribute("id","juke_mode");const auto* old=xml->getChildByAttribute("id","sonar_weather_mode");v->setAttribute("value",old&&old->getDoubleAttribute("value")>=.5&&xml->getChildByAttribute("id","aura_color")&&xml->getChildByAttribute("id","aura_color")->getDoubleAttribute("value")>=.5?2.0:5.0);}
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            sanitizeCrossovers();
            // Resolve old sessions that persisted more than one SOLO flag.
            triggerAsyncUpdate();
        }
}

bool PRISMVSTAudioProcessor::savePreset(const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml()) {
        juce::TemporaryFile temporary(file);
        return xml->writeTo(temporary.getFile())&&temporary.overwriteTargetFileWithTemporary();
    }
    return false;
}

bool PRISMVSTAudioProcessor::loadPreset(const juce::File& file)
{
    if (!file.existsAsFile() || file.getSize() > 2 * 1024 * 1024) return false;
    auto xml = juce::XmlDocument::parse(file);
    if (!xml || !xml->hasTagName(apvts.state.getType())||!finiteParameterState(*xml,apvts)) return false;
    // Reject arbitrary/empty XML, rather than silently pretending it is a preset.
    if (!xml->getChildByAttribute("id", rootsNodeId(0, "hz"))) return false;
    juce::MemoryBlock data;
    copyXmlToBinary(*xml, data);
    setStateInformation(data.getData(), (int)data.getSize());
    return true;
}

void PRISMVSTAudioProcessor::switchComparison()
{
    getStateInformation(comparisonStates[(size_t)comparisonSlot_]);
    const int next = 1 - comparisonSlot_;
    if (comparisonStates[(size_t)next].isEmpty())
        comparisonStates[(size_t)next] = comparisonStates[(size_t)comparisonSlot_];
    const auto& state = comparisonStates[(size_t)next];
    setStateInformation(state.getData(), (int)state.getSize());
    comparisonSlot_ = next;
}

void PRISMVSTAudioProcessor::copyComparison()
{
    getStateInformation(comparisonStates[(size_t)(1 - comparisonSlot_)]);
}

juce::AudioProcessorEditor* PRISMVSTAudioProcessor::createEditor()
{
    return new RootsEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PRISMVSTAudioProcessor();
}

#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <memory>

namespace
{
bool setActual(PRISMVSTAudioProcessor& p, const juce::String& id, float value)
{
    if (auto* parameter = p.apvts.getParameter(id))
    {
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        return true;
    }
    return false;
}

void fillTone(juce::AudioBuffer<float>& buffer,
              float frequency,
              float amplitude,
              double sampleRate,
              int64_t sampleOffset,
              bool antiPhase = false)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float x = amplitude * std::sin(
            2.0f * juce::MathConstants<float>::pi * frequency
            * (float)(sampleOffset + i) / (float)sampleRate);

        buffer.setSample(0, i, x);
        buffer.setSample(1, i, antiPhase ? -x : x);
    }
}

double rms(const juce::AudioBuffer<float>& buffer)
{
    double energy = 0.0;
    const int count = buffer.getNumChannels() * buffer.getNumSamples();

    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const double x = buffer.getSample(c, i);
            energy += x * x;
        }

    return count > 0 ? std::sqrt(energy / (double)count) : 0.0;
}

bool finiteBuffer(const juce::AudioBuffer<float>& buffer)
{
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (!std::isfinite(buffer.getSample(c, i)))
                return false;
    return true;
}
}

bool runQualityTests();
bool runWeatherCalibration();
bool runWeatherFieldTests();
bool runStormChassisTests();
int main(int argc,char** argv)
{
    if(argc>1&&juce::String(argv[1])=="--weather-calibration")return runWeatherCalibration()?0:71;
    if(argc>1&&juce::String(argv[1])=="--weather-field")return runWeatherFieldTests()?0:72;
    if(argc>1&&juce::String(argv[1])=="--storm-chassis")return runStormChassisTests()?0:73;
    juce::ScopedJuceInitialiser_GUI gui;
    // Keep ROOTS distinct in FL Studio's plugin database / VST3 identity.
    {
        PRISMVSTAudioProcessor p;
        if (p.getName() != "SURGERY EQ v1.0")
        {
            std::cerr << "ROOTS plugin host-visible name mismatch\n";
            return 1;
        }
    }
    // The FFT visualizer never adds host-reported audio buffering latency.
    {
        PRISMVSTAudioProcessor p;
        if(p.getLatencySamples()!=0)return 31;
    }
    // Ultra Q must be host-editable, not just an extended rotary visual.
    {
        PRISMVSTAudioProcessor p;
        const auto id=PRISMVSTAudioProcessor::carveId(3,0,"q");
        if(!setActual(p,id,95.333f))return 29;
        const auto* v=p.apvts.getRawParameterValue(id);
        if(v==nullptr||std::abs(v->load()-95.333f)>0.0011f)return 30;
    }
    // Q parameter has milli-Q resolution and a recalled heatmap switch.
    {
        PRISMVSTAudioProcessor p;
        const auto id=PRISMVSTAudioProcessor::carveId(0,0,"q");
        const auto* q=p.apvts.getParameter(id);
        if(q==nullptr)return 26;
        setActual(p,id,1.234f);
        const auto* raw=p.apvts.getRawParameterValue(id);
        if(raw==nullptr||std::abs(raw->load()-1.234f)>0.0011f)return 27;
        const auto* view=p.apvts.getRawParameterValue("surgery_heat_view");
        if(view==nullptr||view->load()>=0.5f)return 28;
    }
    // SURGERY CARVE: a new instance owns 28 distinct but initially OFF slots.
    // Host IDs and parameter defaults must agree; selecting a ROOT is not activation.
    {
        PRISMVSTAudioProcessor p;
        int off=0;
        for(int root=0;root<surgery::kRoots;++root){
            for(int slot=0;slot<surgery::kSlots;++slot){
                const auto id=PRISMVSTAudioProcessor::carveId(root,slot,"on");
                auto* parameter=p.apvts.getParameter(id);
                const auto* raw=p.apvts.getRawParameterValue(id);
                if(parameter==nullptr||raw==nullptr||
                   raw->load()>=0.5f||parameter->getDefaultValue()>=0.5f){
                    std::cerr<<"CARVE must start OFF: ROOT "<<root+1
                             <<" C"<<slot+1<<"\n";return 23;
                }
                ++off;
            }
        }
        if(off!=28)return 24;
        const auto target=PRISMVSTAudioProcessor::carveId(2,3,"on");
        const auto neighbor=PRISMVSTAudioProcessor::carveId(2,2,"on");
        if(!setActual(p,target,1.0f) ||
           p.apvts.getRawParameterValue(target)->load()<0.5f ||
           p.apvts.getRawParameterValue(neighbor)->load()>=0.5f)
            return 25;
        std::cerr<<"SURGERY 28 CARVEs default OFF; C4 enables independently\n";
    }
    // ROOTS parent/child test: node selection is not SOLO activation.
    {
        PRISMVSTAudioProcessor p;
        const auto soloId=[](int root){return PRISMVSTAudioProcessor::rootsRootSoloId(root);};
        auto value=[&](int root){
            auto* v=p.apvts.getRawParameterValue(soloId(root));
            return v?v->load():-1.0f;
        };
        p.transferRootsSoloTo(5);
        if(p.getActiveRootsSolo()!=-1)return 1; // selection alone is inert
        if(!setActual(p,soloId(0),1.0f)||p.getActiveRootsSolo()!=0)return 1;
        p.transferRootsSoloTo(1);               // second node of same ROOT
        if(p.getActiveRootsSolo()!=0||value(0)<0.5f)return 1;
        p.transferRootsSoloTo(5);               // node index 5 is ROOT #3
        if(p.getActiveRootsSolo()!=2||value(0)>=0.5f||value(2)<0.5f)return 1;
        p.transferRootsSoloTo(4);               // sibling of same ROOT
        if(p.getActiveRootsSolo()!=2)return 1;
        if(!setActual(p,soloId(4),1.0f)||p.getActiveRootsSolo()!=4)return 1;
        p.transferRootsSoloTo(11);              // selects ROOT #6
        for(int root=0;root<PRISMVSTAudioProcessor::numSections;++root)
            if((root==5)!=(value(root)>=0.5f))return 1;
        if(!setActual(p,soloId(5),0.0f)||p.getActiveRootsSolo()!=-1)return 1;
        std::cerr<<"ROOTS parent SOLO proof passed: sibling stable, root transfer, exclusive OFF\n";
    }
    // ROOTS UI/host contract: every effect belongs to one of seven ROOTS,
    // never to one of fourteen frequency-boundary handles.
    {
        PRISMVSTAudioProcessor p;
        for(int root=0;root<PRISMVSTAudioProcessor::numSections;++root){
            for(const char* suffix:{"on","space","threshold","comp","domain","detector"})
                if(p.apvts.getParameter(
                    PRISMVSTAudioProcessor::rootsRootControlId(root,suffix))==nullptr){
                    std::cerr<<"ROOT parent parameter missing: "<<suffix<<"\n";
                    return 1;
                }
            if(p.apvts.getParameter(PRISMVSTAudioProcessor::rootsRootGainId(root))==nullptr)
                return 1;
        }
        const auto root0=PRISMVSTAudioProcessor::rootsRootControlId(0,"space");
        const auto root1=PRISMVSTAudioProcessor::rootsRootControlId(1,"space");
        if(!setActual(p,root0,5.0f) ||
           p.apvts.getRawParameterValue(root0)->load()!=5.0f ||
           p.apvts.getRawParameterValue(root1)->load()!=50.0f)
            return 1;
        // The global stereo-space control is independent of selected ROOT.
        if(!setActual(p,"roots_space",0.0f) ||
           p.apvts.getRawParameterValue("roots_space")->load()!=0.0f)
            return 1;
        std::cerr<<"ROOTS parent control IDs, independence, and stereo-space passed\n";
    }
    std::cerr << "TEST 1 schema" << std::endl;
    // -------------------------------------------------------------------------
    // Contract: seven named sections, six crossovers, no legacy master bypass.
    {
        auto p = std::make_unique<PRISMVSTAudioProcessor>();
        // These established regression proofs deliberately exercise the protected 1.2 DSP path.
        setActual(*p, "roots_engine_mode", 0.0f);

        if (PRISMVSTAudioProcessor::numSections != 7
            || PRISMVSTAudioProcessor::numCrossovers != 6)
        {
            std::cerr << "PRISM must expose seven sections and six crossovers\n";
            return 1;
        }

        p->prepareToPlay(48000.0, 512);
        if (p->getLatencySamples() != 0)
        {
            std::cerr << "PRISM must report zero intentional latency\n";
            return 11;
        }

        for (int s = 0; s < PRISMVSTAudioProcessor::numSections; ++s)
        {
            for (const auto& suffix : {
                     "on", "solo", "input_db", "output_db",
                     "attack_ms", "release_ms", "width_pct", "onyx_pct",
                     "threshold_db", "ratio", "curve" })
            {
                if (p->apvts.getParameter(PRISMVSTAudioProcessor::sectionId(s, suffix)) == nullptr)
                {
                    std::cerr << "Missing section parameter: "
                              << PRISMVSTAudioProcessor::sectionId(s, suffix) << "\n";
                    return 2;
                }
            }
        }

        for (int x = 0; x < PRISMVSTAudioProcessor::numCrossovers; ++x)
            if (p->apvts.getParameter(PRISMVSTAudioProcessor::crossoverId(x)) == nullptr)
            {
                std::cerr << "Missing crossover parameter\n";
                return 3;
            }

        if (p->apvts.getParameter("master_bypass") != nullptr
            || p->apvts.getParameter("master_trim") != nullptr
            || p->apvts.getParameter("ceiling") != nullptr
            || p->apvts.getParameter("band1_freq") != nullptr)
        {
            std::cerr << "Legacy master/node parameters survived migration\n";
            return 4;
        }
    }

    std::cerr << "TEST 2 off mute" << std::endl;
    // -------------------------------------------------------------------------
    // OFF means muted. A soloed-but-OFF section must produce silence.
    {
        auto p = std::make_unique<PRISMVSTAudioProcessor>();
        // These established regression proofs deliberately exercise the protected 1.2 DSP path.
        setActual(*p, "roots_engine_mode", 0.0f);
        p->prepareToPlay(48000.0, 512);

        setActual(*p, PRISMVSTAudioProcessor::sectionId(1, "solo"), 1.0f);
        setActual(*p, PRISMVSTAudioProcessor::sectionId(1, "on"), 0.0f);

        juce::AudioBuffer<float> b(2, 512);
        juce::MidiBuffer midi;
        fillTone(b, 90.0f, 0.6f, 48000.0, 0);
        p->processBlock(b, midi);

        if (b.getMagnitude(0, 0, b.getNumSamples()) > 1.0e-7f
            || b.getMagnitude(1, 0, b.getNumSamples()) > 1.0e-7f)
        {
            std::cerr << "OFF section is not muted\n";
            return 5;
        }
    }

    std::cerr << "TEST 3 solo" << std::endl;
    // -------------------------------------------------------------------------
    // SOLO means audible isolation, and the section path must remain finite.
    {
        auto p = std::make_unique<PRISMVSTAudioProcessor>();
        // These established regression proofs deliberately exercise the protected 1.2 DSP path.
        setActual(*p, "roots_engine_mode", 0.0f);
        p->prepareToPlay(48000.0, 512);
        setActual(*p, PRISMVSTAudioProcessor::sectionId(1, "solo"), 1.0f);

        juce::AudioBuffer<float> b(2, 512);
        juce::MidiBuffer midi;

        for (int block = 0; block < 12; ++block)
        {
            fillTone(b, 90.0f, 0.5f, 48000.0, (int64_t)block * 512);
            p->processBlock(b, midi);
        }

        if (!finiteBuffer(b) || rms(b) < 0.005)
        {
            std::cerr << "SOLO section is silent or unstable\n";
            return 6;
        }
    }

    std::cerr << "TEST 4 neutral" << std::endl;
    // -------------------------------------------------------------------------
    // Neutral seven-way reconstruction must not grossly change amplitude.
    {
        auto p = std::make_unique<PRISMVSTAudioProcessor>();
        // These established regression proofs deliberately exercise the protected 1.2 DSP path.
        setActual(*p, "roots_engine_mode", 0.0f);
        p->prepareToPlay(48000.0, 512);

        juce::AudioBuffer<float> b(2, 512);
        juce::AudioBuffer<float> dry(2, 512);
        juce::MidiBuffer midi;

        double dryRms = 0.0;
        double wetRms = 0.0;

        for (int block = 0; block < 48; ++block)
        {
            fillTone(b, 1000.0f, 0.35f, 48000.0, (int64_t)block * 512);
            dry.makeCopyOf(b);
            p->processBlock(b, midi);

            if (block == 47)
            {
                dryRms = rms(dry);
                wetRms = rms(b);
            }
        }

        if (!finiteBuffer(b) || dryRms <= 1.0e-9)
        {
            std::cerr << "Neutral reconstruction test invalid\n";
            return 7;
        }

        const double ratio = wetRms / dryRms;
        if (ratio < 0.80 || ratio > 1.20)
        {
            std::cerr << "Neutral crossover reconstruction amplitude drift: " << ratio << "\n";
            return 8;
        }
    }

    std::cerr << "TEST 5 50-percent widen cap and real stereo activity" << std::endl;
    // -------------------------------------------------------------------------
    // The legacy width_pct host parameter remains 0..200 for old sessions;
    // 100 is neutral, 150 is maximum +50% stereo widening.
    {
        const int mid = 4;
        const auto measure = [mid](float storedWidth, float* stereoMeter = nullptr)
        {
            auto p = std::make_unique<PRISMVSTAudioProcessor>();
        // These established regression proofs deliberately exercise the protected 1.2 DSP path.
        setActual(*p, "roots_engine_mode", 0.0f);
            p->prepareToPlay(48000.0, 512);
            setActual(*p, PRISMVSTAudioProcessor::sectionId(mid, "solo"), 1.0f);
            setActual(*p, PRISMVSTAudioProcessor::sectionId(mid, "width_pct"), storedWidth);
            juce::AudioBuffer<float> b(2, 512);
            juce::MidiBuffer midi;
            for (int block = 0; block < 24; ++block)
            {
                fillTone(b, 1000.0f, 0.35f, 48000.0, (int64_t)block * 512, true);
                p->processBlock(b, midi);
            }
            if (stereoMeter != nullptr)
                *stereoMeter = p->getStereoActivity(mid);
            return rms(b);
        };

        float measuredStereo = 0.0f;
        const double neutral = measure(100.0f);
        const double minLegacy = measure(0.0f);
        const double expanded = measure(150.0f, &measuredStereo);
        const double capped = measure(200.0f);

        if (neutral < 0.005 || std::abs(minLegacy / neutral - 1.0) > 0.04
            || expanded / neutral < 1.35 || expanded / neutral > 1.65
            || std::abs(capped / expanded - 1.0) > 0.04
            || measuredStereo < 0.7f)
        {
            std::cerr << "WIDTH cap/neutral/real-signal stereo meter failed\n";
            return 9;
        }
    }

    std::cerr << "TEST 6 sanitize" << std::endl;
    // -------------------------------------------------------------------------
    // Invalid crossover ordering must repair without changing the 7-section law.
    {
        auto p = std::make_unique<PRISMVSTAudioProcessor>();
        // These established regression proofs deliberately exercise the protected 1.2 DSP path.
        setActual(*p, "roots_engine_mode", 0.0f);
        setActual(*p, PRISMVSTAudioProcessor::crossoverId(4), 11000.0f);
        setActual(*p, PRISMVSTAudioProcessor::crossoverId(5), 900.0f);
        p->prepareToPlay(48000.0, 512);

        float previous = 20.0f;
        for (int x = 0; x < PRISMVSTAudioProcessor::numCrossovers; ++x)
        {
            const auto* value = p->apvts.getRawParameterValue(PRISMVSTAudioProcessor::crossoverId(x));
            if (value == nullptr || value->load() <= previous)
            {
                std::cerr << "Crossover sanitizer failed\n";
                return 10;
            }
            previous = value->load();
        }
    }

    std::cerr << "TEST 7 exclusive SOLO" << std::endl;
    {
        auto p = std::make_unique<PRISMVSTAudioProcessor>();
        // These established regression proofs deliberately exercise the protected 1.2 DSP path.
        setActual(*p, "roots_engine_mode", 0.0f);
        p->prepareToPlay(48000.0, 512);
        setActual(*p, PRISMVSTAudioProcessor::sectionId(1, "solo"), 1.0f);
        setActual(*p, PRISMVSTAudioProcessor::sectionId(4, "solo"), 1.0f);

        if (p->getActiveSoloSection() != 4)
        {
            std::cerr << "Latest SOLO should take exclusive priority\n";
            return 12;
        }
        setActual(*p, PRISMVSTAudioProcessor::sectionId(4, "solo"), 0.0f);
        if (p->getActiveSoloSection() != 1)
        {
            std::cerr << "SOLO release should restore the remaining active selection\n";
            return 13;
        }
    }

    std::cerr << "TEST 8 ONYX sonic safety and silence" << std::endl;
    {
        const auto measure = [](float onyx, bool silence)
        {
            auto p = std::make_unique<PRISMVSTAudioProcessor>();
        // These established regression proofs deliberately exercise the protected 1.2 DSP path.
        setActual(*p, "roots_engine_mode", 0.0f);
            p->prepareToPlay(48000.0, 512);
            setActual(*p, PRISMVSTAudioProcessor::sectionId(4, "solo"), 1.0f);
            setActual(*p, PRISMVSTAudioProcessor::sectionId(4, "onyx_pct"), onyx);
            juce::AudioBuffer<float> b(2, 512);
            juce::MidiBuffer midi;
            for (int block = 0; block < 24; ++block)
            {
                fillTone(b, 1000.0f, silence ? 0.0f : 0.35f,
                         48000.0, (int64_t)block * 512);
                p->processBlock(b, midi);
            }
            return rms(b);
        };

        const double dry = measure(0.0f, false);
        const double onyx = measure(100.0f, false);
        const double silence = measure(100.0f, true);
        if (dry < 0.005 || onyx / dry < 0.75 || onyx / dry > 1.15
            || std::abs(onyx / dry - 1.0) < 0.0005 || silence > 1.0e-7)
        {
            std::cerr << "ONYX became excessively loud, ineffective or generated noise\n";
            return 14;
        }
    }

    // Double-click reset actions route through these host-aware processor methods.
    // Resetting one ROOT frequency cannot reset gain, SOLO or a neighboring boundary.
    {
        PRISMVSTAudioProcessor p;
        const auto value=[&](const juce::String& id) {
            if(const auto* v=p.apvts.getRawParameterValue(id))return v->load();
            return -999.0f;
        };
        const auto left=PRISMVSTAudioProcessor::rootsNodeId(3,"hz");
        const auto sibling=PRISMVSTAudioProcessor::rootsNodeId(2,"hz");
        const auto rootGain=PRISMVSTAudioProcessor::rootsRootGainId(1);
        setActual(p,left,92.0f);setActual(p,sibling,65.0f);
        setActual(p,rootGain,7.0f);
        if(!p.resetRootBoundary(3)||std::abs(value(left)-120.0f)>0.02f
            ||std::abs(value(sibling)-65.0f)>0.02f
            ||std::abs(value(rootGain)-7.0f)>0.02f
            ||p.resetRootBoundary(-1)||p.resetRootBoundary(14)){
            std::cerr<<"ROOT boundary double-click reset failed\n";return 21;
        }
        const auto id=[](int root,int slot,const char* name) {
            return PRISMVSTAudioProcessor::carveId(root,slot,name);
        };
        setActual(p,id(1,2,"on"),1.0f);
        setActual(p,id(1,2,"hz"),1470.0f);
        setActual(p,id(1,2,"depth"),14.0f);
        setActual(p,id(1,2,"q"),9.0f);
        setActual(p,id(1,2,"threshold"),-45.0f);
        setActual(p,id(1,2,"dynamic"),1.0f);
        setActual(p,id(1,2,"domain"),2.0f);
        setActual(p,id(1,1,"depth"),5.0f);
        // Third CARVE slot resets to its own log-spaced position, not ROOT centre.
        const float midpoint=(float)std::exp(std::log(65.0)+
            (3.0/5.0)*std::log(120.0/65.0));
        if(!p.resetCarveNode(1,2)
            ||std::abs(value(id(1,2,"hz"))-midpoint)>0.1f
            ||std::abs(value(id(1,2,"depth")))>0.001f
            ||std::abs(value(id(1,2,"q"))-2.0f)>0.02f
            ||std::abs(value(id(1,2,"threshold"))+24.0f)>0.11f
            ||value(id(1,2,"dynamic"))>0.5f
            ||value(id(1,2,"domain"))>0.5f
            ||value(id(1,2,"on"))<0.5f
            ||std::abs(value(id(1,1,"depth"))-5.0f)>0.02f
            ||p.resetCarveNode(7,0)||p.resetCarveNode(0,-1)){
            std::cerr<<"CARVE node double-click neutral reset failed\n";return 22;
        }
    }
    std::cout << "PRISM ROOTS PROTO 001 legacy DSP compatibility tests passed\n";
    if(!runQualityTests())return 70;
    return 0;
}

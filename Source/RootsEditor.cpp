#include "RootsEditor.h"
#include "RootsSpectrumColours.h"
#include "SurgerySpectralColours.h"
#include <BinaryData.h>
#include <cmath>
#include <numbers>

namespace {
float pval(const juce::AudioProcessorValueTreeState& ap,const juce::String& id) {
    if(const auto* v=ap.getRawParameterValue(id))return v->load();
    return 0.0f;
}
void change(PRISMVSTAudioProcessor& p,const juce::String& id,float val) {
    if(auto* param=p.apvts.getParameter(id))
        param->setValueNotifyingHost(param->convertTo0to1(val));
}
juce::String hzText(float f) {
    if(f>=1000.0f)return juce::String(f/1000.0f,f>=10000?1:2)+"k";
    if(f<10.0f)return juce::String(f,2);
    return juce::String(juce::roundToInt(f));
}
juce::String pitchText(float hz){
    const auto p=surgery::spectrum::pitchAt(hz);
    return p.valid?juce::String(surgery::spectrum::noteNames[(size_t)p.note])+juce::String(p.octave):juce::String("--");
}
juce::Colour territory(int i){
    static constexpr unsigned hues[7]{
        0xFF6BBDD4u,0xFFDFA871u,0xFFE6BD6Au,0xFF8DCEA8u,
        0xFF9DADEAu,0xFFD2A2E5u,0xFFE89EADu
    };
    return juce::Colour(hues[(size_t)juce::jlimit(0,6,i)]);
}
void rotary(juce::Slider& s,juce::String suffix,double resetValue) {
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,22);
    s.setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xffe4eef1));
    s.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff0d1b23));
    s.setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff47616a));
    s.setTextValueSuffix(suffix);
    s.setDoubleClickReturnValue(true,resetValue);
}
}
RootsGraph::RootsGraph(PRISMVSTAudioProcessor& owner):processor_(owner) {
    processor_.beginWeatherConsumer();
    bands_.fill(-144.0f);peaks_.fill(-144.0f);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}
RootsGraph::~RootsGraph(){
    processor_.endWeatherConsumer();
    if(dragging_>=0)
        if(auto* parameter=processor_.apvts.getParameter(PRISMVSTAudioProcessor::rootsNodeId(dragging_,"hz")))
            parameter->endChangeGesture();
    if(draggingCarve_>=0)
        for(const char* suffix:{"hz","depth"})
            if(auto* parameter=processor_.apvts.getParameter(PRISMVSTAudioProcessor::carveId(selected_/2,draggingCarve_,suffix)))
                parameter->endChangeGesture();
}
juce::Rectangle<float> RootsGraph::area() const {
    // Reserve a header rail for ROOT-specific C1-C4 foreground selectors.
    return getLocalBounds().toFloat().reduced(46.0f,46.0f);
}
float RootsGraph::freqToX(float f) const {
    const auto r=area();
    return r.getX()+r.getWidth()*(float)prism::roots::visual::graphUnit(f);
}
float RootsGraph::xToFreq(float x)const {
    const auto r=area();
    return (float)prism::roots::visual::graphFrequency(
        juce::jlimit(0.0f,1.0f,(x-r.getX())/juce::jmax(1.0f,r.getWidth())));
}
float RootsGraph::param(int i,const char* suffix)const {
    return pval(processor_.apvts,PRISMVSTAudioProcessor::rootsNodeId(i,suffix));
}
void RootsGraph::write(int i,const char* suffix,float value) {
    change(processor_,PRISMVSTAudioProcessor::rootsNodeId(i,suffix),value);
}
int RootsGraph::hitNode(juce::Point<float> p)const{
    // Adjacent ROOT boundary handles may occupy the identical x/y point.
    // Cycle among geometrically coincident handles instead of making the
    // later ROOT permanently impossible to select.
    float nearest=18.0f;
    std::array<float,prism::roots::v2::kNodes> distances{};
    for(int i=0;i<prism::roots::v2::kNodes;++i){
        const auto x=freqToX(param(i,"hz")),y=area().getCentreY();
        distances[(size_t)i]=p.getDistanceFrom({x,y});
        nearest=juce::jmin(nearest,distances[(size_t)i]);
    }
    if(nearest>=18.0f)return -1;
    int first=-1,next=-1;
    for(int i=0;i<prism::roots::v2::kNodes;++i){
        if(distances[(size_t)i]>nearest+1.25f)continue;
        if(first<0)first=i;
        if(i>selected_&&next<0)next=i;
    }
    return next>=0?next:first;
}
juce::Colour RootsGraph::colour(int root) const {
    const auto id="surgery_root"+juce::String(root+1)+"_rgb";
    return juce::Colour(0xff000000u |
        (unsigned)juce::roundToInt(pval(processor_.apvts,id)));
}
void RootsGraph::updateAnalyzer(
    const std::array<float,PRISMVSTAudioProcessor::labBars>& values,
    const std::array<float,PRISMVSTAudioProcessor::labBars>& pk){
    const std::array<float,3> options{pval(processor_.apvts,"aura_color"),pval(processor_.apvts,"analyzer_depth"),pval(processor_.apvts,"analyzer_slope")};
    if(options!=paintOptions_){paintOptions_=options;repaint();}
    if(frozen_)return;
    // Analyzer worker owns the FFT. Only 256 display values arrive here.
    // Message-thread smoothing is fast-attack with calibrated decay; never
    // change the underlying audio or the diagnostic measurement data.
    const auto previous=bands_;
    const auto previousPeaks=peaks_;
    spectrumEnvelope_.update(values,1.0/60.0,
        pval(processor_.apvts,"aura_memory"));
    bands_=spectrumEnvelope_.frame();
    peaks_=pk;
    // Retain 60 Hz decay semantics; settled/unchanged frames need no paint.
    if(previous!=bands_||previousPeaks!=peaks_)repaint();
}
void RootsGraph::updateWeather(const surgery::weather::Snapshot* snapshot,double elapsed,bool active,bool reduced,double memory,int style){
    style=style>0?7:0; // All retained legacy values resolve to Standard+.
    const bool visible=active&&style>0;
    const bool changed=visible!=weatherActive_||style!=weatherStyle_;
    weatherActive_=visible;
    // Off gates visibility immediately. Freeze keeps simulation and mode held.
    if(!visible&&changed)repaint();
    if(frozen_)return;
    if(changed){heatValid_=false;weatherStyle_=style;weatherField_.clear();repaint();}
    if(!active){weatherInterpretation_.update(elapsed,memory,true);return;}
    if(snapshot)weatherInterpretation_.accept(*snapshot);
    auto key=[](const surgery::weather::Meaning& f){return std::array<float,16>{f.bass,f.centroid,f.flux,f.width,f.rate,f.density,f.level,f.storm,f.flash,f.flashPosition,f.musicEnergy[0],f.musicEnergy[1],f.musicEnergy[2],f.musicEnergy[3],f.rms,f.crest};};
    const auto previous=key(weatherInterpretation_.state());const auto previousPhase=visualPhase_;
    const auto& meaning=weatherInterpretation_.update(elapsed,memory,reduced);
    if(!visible){
        // X-RAY is independent of SONAR mode. Keep its mercury simulation
        // updating from measured music even with SONAR presentation OFF.
        if(xrayActive_&&(changed||previous!=key(meaning)||previousPhase!=visualPhase_))repaint();
        return;
    }
    if(visible||xrayActive_)
        visualPhase_=std::fmod(visualPhase_+(float)(std::clamp(elapsed,0.,.25)*meaning.level*(reduced?.035:1.)),64.f);
    const bool fieldChanged=style==1&&weatherField_.update(weatherInterpretation_,elapsed,memory,reduced,false);
    if(!changed&&!fieldChanged&&previous==key(meaning)&&previousPhase==visualPhase_)return;
    {
        juce::Image::BitmapData pixels(weatherImage_,juce::Image::BitmapData::writeOnly);
        const auto& field=weatherField_.pixels();
        for(int y=0;y<64;++y)for(int x=0;x<128;++x){const auto c=surgery::visual::style(style,meaning,visualPhase_,x/127.f,y/63.f,field[(size_t)(y*128+x)]);pixels.setPixelColour(x,y,juce::Colour(c.r,c.g,c.b));}
    }
    repaint();
}
void RootsGraph::selectNode(int i){
    const int target=juce::jlimit(0,prism::roots::v2::kNodes-1,i);
    if(target==selected_)return;
    selected_=target;
    // Once solo audition is engaged it travels with the selected root.
    processor_.transferRootsSoloTo(selected_);
    if(onSelectNode)onSelectNode(selected_);
    repaint();
}
void RootsGraph::showRootMenu(int root){
    selectNode(juce::jlimit(0,6,root)*2);
    popup();
}
void RootsGraph::paint(juce::Graphics& g){
    ++paintedFrames_;
    const auto full=getLocalBounds().toFloat();
    const auto r=area();

    const juce::Colour top(0xff0e151c),bottom(0xff03070c);
    juce::ColourGradient bg(top,full.getX(),full.getY(),
                            bottom,full.getX(),full.getBottom(),false);
    g.setGradientFill(bg);g.fillRoundedRectangle(full.reduced(1.0f),10.0f);
    g.setColour(juce::Colour::fromRGB(58,74,86));
    g.drawRoundedRectangle(full.reduced(1.0f),10.0f,1.0f);
    {
        g.setColour(juce::Colour(0xff71848a));g.strokePath(surgery::rack::cut(full.reduced(1.5f),10),juce::PathStrokeType(1.2f));
        g.setColour(juce::Colour(0xff02090c));g.strokePath(surgery::rack::cut(full.reduced(4),8),juce::PathStrokeType(2));
    }
    g.setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(145,172,182));
    if(weatherActive_){
        juce::Graphics::ScopedSaveState layer(g);g.reduceClipRegion(r.toNearestInt());
        g.setOpacity(1.f);g.drawImage(weatherImage_,r);
    }
    // Measured frequency/intensity colouring, independent of ROOT or theme.
    g.drawText(frozen_?"X-RAY / FROZEN":(xrayActive_?"X-RAY / LIVE FREQUENCY ENERGY":"SPECTRUM / LIVE FREQUENCY ENERGY"),
               juce::Rectangle<float>(r.getX(),7.0f,r.getWidth(),16.0f).toNearestInt(),juce::Justification::centredLeft);
    // Frequency and calibrated dBFS dimensions.
    static constexpr std::array<float,11> freqs{
        20,40,80,160,315,630,1250,2500,5000,10000,20000
    };
    for(const float f:freqs){
        const float x=freqToX(f);
        g.setColour(juce::Colour::fromRGB(42,55,69).withAlpha(0.74f));
        g.drawVerticalLine(juce::roundToInt(x),r.getY(),r.getBottom());
        g.setColour(juce::Colour::fromRGB(135,161,175));
        g.drawText(hzText(f),juce::roundToInt(x-19),juce::roundToInt(r.getBottom()+6),
                   38,17,juce::Justification::centred);
    }
    static constexpr std::array<int,11> dbTicks{0,-5,-10,-15,-20,-25,-30,-35,-40,-50,-60};
    for(const int db:dbTicks){
        const float y=r.getY()-r.getHeight()*db/60.0f;
        g.setColour(juce::Colour::fromRGB(53,67,84).withAlpha(db==-30?0.74f:0.34f));
        g.drawHorizontalLine(juce::roundToInt(y),r.getX(),r.getRight());
        g.setColour(juce::Colour::fromRGB(131,155,169));
        g.drawText(juce::String(db),juce::roundToInt(r.getRight()+5),
                   juce::roundToInt(y-8),34,16,juce::Justification::centredLeft);
    }
    // ROOT handle height is constant; vertical axis is exclusively dBFS.
    // ROOT gain is a separate, single control in the bottom panel.
    g.setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::plain)));
    g.setColour(juce::Colour::fromRGB(184,197,209));
    g.drawText("Hz",8,16,34,16,juce::Justification::centredLeft);
    g.drawText("dBFS",juce::roundToInt(r.getRight()),16,44,16,juce::Justification::centredRight);
    std::array<prism::roots::visual::Node,prism::roots::visual::kNodes> zones{};
    for(int node=0;node<prism::roots::visual::kNodes;++node)
    {
        zones[(size_t)node].hz=param(node,"hz");
        zones[(size_t)node].q=param(node,"q");
        zones[(size_t)node].enabled=pval(processor_.apvts,
            PRISMVSTAudioProcessor::rootsRootControlId(node/2,"on"))>=0.5f;
    }
    // The visible coverage is defined by each ROOT's two boundary handles,
    // not Q distance or a fixed index/37 palette. Regions can intersect.
    for(int root=0;root<prism::roots::visual::kRoots;++root) {
        const auto interval=prism::roots::visual::region(root,zones);
        if(!interval.enabled)continue;
        const float x1=freqToX((float)interval.lowHz);
        const float x2=freqToX((float)interval.highHz);
        g.setColour(colour(root).withAlpha(root==selected_/2?0.075f:0.025f));
        g.fillRect(x1,r.getY(),juce::jmax(0.0f,x2-x1),r.getHeight());
    }
    // Single live FFT envelope. X is 20-20k logarithmic Hz; Y is measured dBFS.
    // The area beneath the one curve is closed at the -rangeDb graph floor.
    // Display slope or spectral colouring never changes the audio path.
    // Measurement still extends below -144 dBFS. X-RAY uses the locked
    // 0..-60 dBFS presentation scale without modifying analyzer precision.
    const float rangeDb=60.0f;
    const float slope=pval(processor_.apvts,"analyzer_slope");
    const bool heatOn=pval(processor_.apvts,"aura_color")>=0.5f&&(weatherStyle_<=2||weatherStyle_==7);
    juce::Path spectrumPath,filledPath,peakPath;
    const int N=PRISMVSTAudioProcessor::labBars;
    for(int i=0;i<N;++i){
        const float frequency=prism::analysis::LabBands::centreFrequency(i);
        const float x=freqToX(frequency);
        const float db=surgery::displayDb(bands_[(size_t)i],frequency,slope,rangeDb);
        const float y=r.getY()-db*r.getHeight()/rangeDb;
        const float peak=surgery::displayDb(peaks_[(size_t)i],frequency,slope,rangeDb);
        const float py=r.getY()-peak*r.getHeight()/rangeDb;
        if(i==0){
            spectrumPath.startNewSubPath(x,y);
            peakPath.startNewSubPath(x,py);
            filledPath.startNewSubPath(x,r.getBottom());
            filledPath.lineTo(x,y);
        }else{
            spectrumPath.lineTo(x,y);
            peakPath.lineTo(x,py);
            filledPath.lineTo(x,y);
        }
    }
    filledPath.lineTo(freqToX(20000.0f),r.getBottom());
    filledPath.closeSubPath();
    g.saveState();
    g.reduceClipRegion(r.toNearestInt());
    if(heatOn){
        // One small, reusable colour field replaces 256 gradient draw calls
        // through a complex path clip. The measured envelope is still exact.
        if(!heatValid_||heatLevels_!=bands_){
            juce::Image::BitmapData pixels(spectralHeat_,juce::Image::BitmapData::writeOnly);
            for(int i=0;i<N;++i){
                const float energy=surgery::spectrum::brightness(bands_[(size_t)i]);
                const auto measuredColour=surgery::synesthesia::read(weatherInterpretation_.state(),i/(float)(N-1));
                const auto legacyColour=surgery::spectrum::colourAt(prism::analysis::LabBands::centreFrequency(i));
                const auto rgb=weatherStyle_>=2?measuredColour:surgery::synesthesia::RGB{legacyColour.r,legacyColour.g,legacyColour.b};
                const auto tint=juce::Colour::fromFloatRGBA(rgb.r,rgb.g,rgb.b,energy>0?.28f+.62f*energy:0.f);
                for(int y=0;y<64;++y)pixels.setPixelColour(i,y,tint.withMultipliedAlpha(1.f-.7f*y/63.f));
            }
            heatLevels_=bands_;heatValid_=true;
        }
        g.saveState();g.reduceClipRegion(filledPath);
        // Image drawing multiplies by the CURRENT brush alpha. Territory
        // shading left it at .025: reset before compositing cached pixels.
        g.setOpacity(1.f);g.drawImage(spectralHeat_,r);
        g.restoreState();
    }
    if(xrayActive_){
        // Mercury tar-pit renderer. Only the already-measured FFT envelope
        // defines the silhouette; no synthetic peaks or audio DSP coupling.
        // Bubbles live INSIDE the bounded fill and are driven by the shared
        // weather/synesthesia state (level, bass, flux, width).
        juce::Graphics::ScopedSaveState mercury(g);
        g.reduceClipRegion(r.toNearestInt());
        const auto& m=weatherInterpretation_.state();
        const float energy=juce::jlimit(0.f,1.f,
            .30f*m.level+.35f*m.bass+.20f*m.flux+.15f*m.width);
        juce::ColourGradient tar(juce::Colour(0xff3d4241).withAlpha(.87f),
            r.getCentreX(),r.getY(),juce::Colour(0xff030506).withAlpha(.96f),
            r.getCentreX(),r.getBottom(),false);
        g.setGradientFill(tar);g.fillPath(filledPath);
        g.saveState();g.reduceClipRegion(filledPath);
        const float motion=visualPhase_*4.1f;
        for(int i=0;i<31;++i){
            const float t=(i+.5f)/31.f;
            const float x=r.getX()+t*r.getWidth();
            const int bin=juce::jlimit(0,N-1,juce::roundToInt(t*(N-1)));
            const float db=surgery::displayDb(bands_[(size_t)bin],
                prism::analysis::LabBands::centreFrequency(bin),slope,rangeDb);
            const float top=r.getY()-db*r.getHeight()/rangeDb;
            const float sway=std::sin(i*2.431f+motion*.34f);
            const float radius=(1.1f+energy*6.f)*(0.42f+.58f*std::abs(sway));
            const float depth=(.10f+.70f*std::abs(std::sin(i*1.19f+motion*.2f)))*
                              juce::jmax(0.f,r.getBottom()-top);
            const float y=top+depth;
            if(y+radius>=r.getBottom()||y-radius<=top)continue;
            const auto tint=surgery::synesthesia::read(m,t);
            const auto sheen=juce::Colour::fromFloatRGBA(tint.r,tint.g,tint.b,1.f);
            g.setColour(juce::Colour(0xff07090a).withAlpha(.72f));
            g.fillEllipse(x-radius,y-radius,radius*2.1f,radius*1.7f);
            g.setColour(sheen.withAlpha(.13f+.13f*energy));
            g.drawEllipse(x-radius,y-radius,radius*2.f,radius*1.6f,.75f);
            g.setColour(juce::Colours::white.withAlpha(.09f+.12f*energy));
            g.fillEllipse(x-radius*.50f,y-radius*.66f,radius*.56f,radius*.22f);
        }
        g.restoreState();
    }
    // Two restrained, stationary glow layers follow ONLY the measured path.
    // Existing attack/release supplies persistence; no clock-driven shimmer,
    // independent objects or fabricated motion is introduced during silence.
    const float luminous=surgery::spectrum::brightness(*std::max_element(bands_.begin(),bands_.end()));
    if(heatOn&&luminous>0){
        juce::ColourGradient foil(juce::Colour(0xffb29da5).withAlpha(.10f*luminous),r.getX(),r.getY(),
                                  juce::Colour(0xff9baeb5).withAlpha(.12f*luminous),r.getRight(),r.getY(),false);
        g.setGradientFill(foil);g.strokePath(spectrumPath,juce::PathStrokeType(4.2f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xffd3b8d1).withAlpha(.13f*luminous));
        g.strokePath(spectrumPath,juce::PathStrokeType(2.4f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
    // A single fine readout follows the spectrum edge, never a 2D waterfall.
    g.setColour(juce::Colour(0xffe3d4d8));
    g.strokePath(spectrumPath,juce::PathStrokeType(weatherStyle_==7?1.7f:1.3f,
        juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    // A restrained peak-hold readout; the audio measurements are unchanged.
    g.setColour(juce::Colour(0xffe3edf2).withAlpha(0.31f));
    g.strokePath(peakPath,juce::PathStrokeType(0.8f));
    g.restoreState();

    // Predict the EXACT static RBJ peak-filter response used in the CARVE DSP.
    // It shows the maximum possible response for dynamic nodes, not measured GR.
    const int selectedRoot=selected_/2;
    const double sr=processor_.getSampleRate()>8000.0?
                    processor_.getSampleRate():48000.0;
    const float baseY=r.getCentreY()+23.0f;
    const float lowHz=param(selectedRoot*2,"hz");
    const float highHz=param(selectedRoot*2+1,"hz");
    std::array<double,32> overlayKey{sr,(double)getWidth(),(double)getHeight(),(double)selectedRoot,lowHz,highHz,(double)slot_,(double)colour(selectedRoot).getARGB()};
    for(int i=0;i<4;++i){
        int index=8+i*4;
        for(const char* suffix:{"on","hz","q","depth"})overlayKey[(size_t)index++]=pval(processor_.apvts,PRISMVSTAudioProcessor::carveId(selectedRoot,i,suffix));
    }
    const float overlayScale=juce::jlimit(.5f,4.f,g.getInternalContext().getPhysicalPixelScaleFactor());
    overlayKey[24]=overlayScale;
    if(!responseOverlayValid_||responseOverlayKey_!=overlayKey){
        responseOverlay_=juce::Image(juce::Image::ARGB,juce::roundToInt(getWidth()*overlayScale),juce::roundToInt(getHeight()*overlayScale),true);
        juce::Graphics overlayGraphics(responseOverlay_);overlayGraphics.addTransform(juce::AffineTransform::scale(overlayScale));
    for(int slot=0;slot<surgery::kSlots;++slot){
        const auto id=[selectedRoot,slot](const char* suffix){
            return PRISMVSTAudioProcessor::carveId(selectedRoot,slot,suffix);
        };
        if(pval(processor_.apvts,id("on"))<0.5f)continue;
        const double fc=surgery::precision::boundedFrequency(
            pval(processor_.apvts,id("hz")),lowHz,highHz);
        const double q=pval(processor_.apvts,id("q"));
        const double depth=pval(processor_.apvts,id("depth"));
        if(depth<0.001)continue;
        auto& cache=responseCache_[(size_t)slot];
        const std::array<double,9> key{sr,fc,q,depth,lowHz,highHz,(double)getWidth(),(double)getHeight(),(double)selectedRoot};
        auto& contour=cache.contour;auto& footprint=cache.footprint;
        if(!cache.valid||cache.key!=key){
        contour.clear();footprint.clear();
        const float firstX=freqToX(20.0f);
        footprint.startNewSubPath(firstX,baseY);
        // High Q requires sub-pixel-aware sampling; old 256 samples across
        // 10 octaves could miss the entire ultra-narrow notch!
        for(int i=0;i<=2048;++i){
            const float f=20.0f*std::pow(1000.0f,(float)i/2048.0f);
            const float x=freqToX(f);
            const double scale=surgery::CarveEngine::boundaryWeight(fc,
                std::min(lowHz,highHz),std::max(lowHz,highHz));
            // Static-response preview: approximate dB interpolation for dry/wet
            // blend; exact RBJ biquad curve tested separately.
            const double response=surgery::carveMagnitudeDb(sr,fc,q,depth,f)*scale;
            const float y=baseY-(float)response*1.10f;
            if(i==0)contour.startNewSubPath(x,y);
            else contour.lineTo(x,y);
            footprint.lineTo(x,y);
        }
        footprint.lineTo(freqToX(20000.0f),baseY);
        footprint.closeSubPath();
        cache.key=key;cache.valid=true;
        }
        const auto tint=colour(selectedRoot).brighter(slot==slot_?0.6f:0.10f);
        overlayGraphics.setColour(tint.withAlpha(slot==slot_?0.20f:0.08f));
        overlayGraphics.fillPath(footprint);
        overlayGraphics.setColour(tint.withAlpha(slot==slot_?0.96f:0.58f));
        overlayGraphics.strokePath(contour,juce::PathStrokeType(slot==slot_?2.4f:1.35f));
    }
        responseOverlayKey_=overlayKey;responseOverlayValid_=true;
    }
    g.setOpacity(1.f);g.drawImage(responseOverlay_,getLocalBounds().toFloat());
    g.setColour(juce::Colour(0xffacc3cf));
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    const auto& meaning=weatherInterpretation_.state();
    const auto& measured=weatherInterpretation_.measured();
    const juce::String caption=weatherActive_?
        "SONAR 2.1 / STANDARD+ / R"+juce::String(selected_/2+1)+" PWR "+juce::String(surgery::weather::dbPower(measured.power[(size_t)(selected_/2)]),1)+" dBFS  B "+juce::String(meaning.bass,2)+"  C "+juce::String(meaning.centroid,2)+"  F "+juce::String(meaning.flux,2)+"  W "+juce::String(meaning.width,2)+"  R "+juce::String(meaning.rate,2)+"  D "+juce::String(meaning.density,2)+"  CORR "+juce::String(measured.correlation,2)+(measured.correlation<-.2f?" / MONO CAUTION":""):
        "SYNESTHESIA V1 / FREQUENCY > HUE / LEVEL > LIGHT     CARVE / STATIC RESPONSE";
    g.drawText(caption,
               juce::Rectangle<float>(r.getX(),r.getBottom()+23.0f,r.getWidth(),16.0f).toNearestInt(),
               juce::Justification::centredLeft);
    // Both boundary handles always live on one horizontal rail.
    g.setColour(juce::Colour::fromRGB(159,182,191).withAlpha(0.14f));
    g.drawHorizontalLine(juce::roundToInt(r.getCentreY()),r.getX(),r.getRight());
    // Two-pass z-order keeps the selected ROOT boundary pair above neighbors.
    for(int pass=0;pass<2;++pass)
    for(int i=0;i<prism::roots::v2::kNodes;++i){
        if((i/2==selectedRoot)!=(pass==1))continue;
        const float hz=param(i,"hz");
        const float x=freqToX(hz),y=r.getCentreY();
        const bool enabled=pval(processor_.apvts,
            PRISMVSTAudioProcessor::rootsRootControlId(i/2,"on"))>=0.5f;
        const bool solo=pval(processor_.apvts,
             PRISMVSTAudioProcessor::rootsRootSoloId(i/2))>=0.5f;
        const auto col=colour(i/2);
        // Vertical guides represent frequency boundaries, not gain movements.
        const bool focused=i/2==selectedRoot;
        g.setColour(col.withAlpha(enabled?(focused?0.55f:0.15f):0.045f));
        g.drawVerticalLine(juce::roundToInt(x),r.getY(),r.getBottom());
        g.setColour(col.withAlpha(enabled?(focused?0.19f:0.045f):0.025f));
        g.fillEllipse(x-10,y-10,20,20);
        g.setColour(enabled?col:juce::Colour::fromRGB(79,90,97));
        g.fillEllipse(x-5.5f,y-5.5f,11,11);
        if(i==selected_||solo||focused){
            g.setColour(solo?juce::Colours::white:col.brighter(0.65f));
            g.drawEllipse(x-9,y-9,18,18,i==selected_?2.0f:1.2f);
        }
        // Precise ROOT selection details live in the contextual control shelf,
        // not in floating text that masks live spectrum or CARVE contours.
    }
    // Focused ROOT foreground rail: all four slots remain identifiable.
    // OFF slots are selectable but never represented as active audio filters.
    const auto focusedColor=colour(selectedRoot);
    g.setColour(focusedColor.withAlpha(0.18f));
    g.fillRoundedRectangle(r.getX(),24.0f,171.0f,20.0f,4.0f);
    g.setColour(focusedColor.brighter(0.45f));
    g.setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::bold)));
    g.drawFittedText("ROOT "+juce::String(selectedRoot+1)+"  /  C1–C4",
        juce::Rectangle<int>((int)r.getX()+6,25,160,18),
        juce::Justification::centredLeft,1);

    // Selected ROOT CARVE markers always render AFTER boundary handles and heat.
    // Active cuts use their actual host frequency and depth; inactive slots
    // exist only as OFF selectors in the top rail (no fabricated filter plot).
    for(int slot=0;slot<surgery::ui::kSlotsPerRoot;++slot){
        const auto id=[selectedRoot,slot](const char* suffix){
            return PRISMVSTAudioProcessor::carveId(selectedRoot,slot,suffix);
        };
        const bool enabled=pval(processor_.apvts,id("on"))>=0.5f;
        if(!enabled)continue; // Disabled CARVEs are genuinely invisible on the graph.
        const float x=freqToX(surgery::precision::boundedFrequency(
            pval(processor_.apvts,id("hz")),
            param(selectedRoot*2,"hz"),param(selectedRoot*2+1,"hz")));
        const float depth=pval(processor_.apvts,id("depth"));
        const float y=surgery::ui::carveY(r.getCentreY(),depth);
        const auto outline=focusedColor.brighter(0.35f);
        if(enabled) {
            g.setColour(focusedColor.withAlpha(0.42f));
            g.drawVerticalLine(juce::roundToInt(x),y,r.getBottom());
        }
        g.setColour(juce::Colour(0xff0c1820));
        g.fillEllipse(x-10.5f,y-10.5f,21.0f,21.0f);
        g.setColour(enabled?outline:juce::Colour(0xff788b93));
        g.drawEllipse(x-10.5f,y-10.5f,21.0f,21.0f,slot==slot_?2.8f:1.8f);
        if(enabled) {
            g.setColour(slot==slot_?juce::Colours::white:focusedColor);
            g.fillEllipse(x-6.0f,y-6.0f,12.0f,12.0f);
        }
        // The marker centre stays at the measured host parameter position.
        // Small nearby identification plates may overlap, not the node data.
        const int labelX=juce::jlimit((int)r.getX(),(int)r.getRight()-30,
                                    juce::roundToInt(x)+9+(slot%2)*2);
        const int labelY=juce::jlimit((int)r.getY(),(int)r.getBottom()-17,
                                    juce::roundToInt(y)-25-(slot%2)*17);
        g.setColour(juce::Colour(0xff07151c).withAlpha(0.94f));
        g.fillRoundedRectangle((float)labelX,(float)labelY,29.0f,17.0f,4.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(10.0f,juce::Font::bold)));
        g.drawText("C"+juce::String(slot+1),labelX,labelY,29,17,
                   juce::Justification::centred);
    }
    for(int slot=0;slot<surgery::ui::kSlotsPerRoot;++slot){
        const auto bounds=surgery::ui::carveTabBounds((float)getWidth(),slot);
        const auto id=PRISMVSTAudioProcessor::carveId(selectedRoot,slot,"on");
        const bool enabled=pval(processor_.apvts,id)>=0.5f;
        const bool chosen=slot==slot_;
        g.setColour(juce::Colour(0xff09151b).withAlpha(0.95f));
        g.fillRoundedRectangle(bounds.x,bounds.y,bounds.w,bounds.h,4.0f);
        g.setColour(enabled?focusedColor.withAlpha(chosen?1.0f:0.75f):
                             juce::Colour(0xff68838a).withAlpha(chosen?0.95f:0.55f));
        g.drawRoundedRectangle(bounds.x,bounds.y,bounds.w,bounds.h,4.0f,chosen?2.0f:1.0f);
        surgery::rack::bezel(g,{bounds.x,bounds.y,bounds.w,bounds.h},enabled&&chosen,2);
        g.setColour(enabled?juce::Colours::white:juce::Colour(0xff9bb0b6));
        g.setFont(juce::Font(juce::FontOptions(10.0f,juce::Font::bold)));
        const auto label="C"+juce::String(slot+1)+(enabled?"  ON":"  OFF");
        g.drawText(label,juce::roundToInt(bounds.x),juce::roundToInt(bounds.y),
                   juce::roundToInt(bounds.w),juce::roundToInt(bounds.h),
                   juce::Justification::centred);
    }
    if(hover_&&r.contains(mouse_)){
        const int bar=prism::analysis::LabBands::indexForFrequency(xToFreq(mouse_.x));
        const float freq=xToFreq(mouse_.x);
        g.setColour(juce::Colours::white.withAlpha(0.26f));
        g.drawVerticalLine(juce::roundToInt(mouse_.x),r.getY(),r.getBottom());
        g.drawHorizontalLine(juce::roundToInt(mouse_.y),r.getX(),r.getRight());
        juce::String text=hzText(freq)+" Hz ~"+pitchText(freq)+" / "+
                juce::String(bands_[(size_t)bar],1)+" dBFS / HOLD "+
                juce::String(peaks_[(size_t)bar],1)+" dBFS";
        const int hoveredCarve=hitCarveNode(mouse_);
        if(hoveredCarve>=0) {
            const int root=selected_/2;
            const auto id=[root,hoveredCarve](const char* suffix){
                return PRISMVSTAudioProcessor::carveId(root,hoveredCarve,suffix);
            };
            text="C"+juce::String(hoveredCarve+1)+" / "+
                 hzText(pval(processor_.apvts,id("hz")))+" Hz / Q "+
                 juce::String(pval(processor_.apvts,id("q")),2)+" / CUT "+
                 juce::String(pval(processor_.apvts,id("depth")),1)+" dB";
        }
        const float left=juce::jlimit(r.getX(),r.getRight()-274.0f,mouse_.x+12);
        const float hoverTop=juce::jlimit(r.getY(),r.getBottom()-29.0f,mouse_.y-39.0f);
        g.setColour(juce::Colour::fromRGB(6,11,18).withAlpha(0.97f));
        g.fillRoundedRectangle(left,hoverTop,274,27,4.0f);
        g.setColour(juce::Colour::fromRGB(236,243,247));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawFittedText(text,(int)left+6,(int)hoverTop+3,262,21,
                         juce::Justification::centredLeft,1);
    }
}
int RootsGraph::hitCarveTab(juce::Point<float> point) const {
    for(int slot=0;slot<surgery::ui::kSlotsPerRoot;++slot)
        if(surgery::ui::carveTabBounds((float)getWidth(),slot).contains(point.x,point.y))
            return slot;
    return -1;
}
int RootsGraph::hitCarveNode(juce::Point<float> point) const {
    const int root=selected_/2;
    float nearest=15.0f;
    int target=-1;
    for(int slot=0;slot<surgery::ui::kSlotsPerRoot;++slot){
        const auto id=[root,slot](const char* suffix){
            return PRISMVSTAudioProcessor::carveId(root,slot,suffix);
        };
        if(pval(processor_.apvts,id("on"))<0.5f)continue; // Invisible means not hit-testable.
        const float x=freqToX(surgery::precision::boundedFrequency(
            pval(processor_.apvts,id("hz")),
            param(root*2,"hz"),param(root*2+1,"hz")));
        const float y=surgery::ui::carveY(area().getCentreY(),
                                          pval(processor_.apvts,id("depth")));
        const float distance=point.getDistanceFrom({x,y});
        // Prefer the selected slot for overlapping real audio nodes.
        if(distance<nearest-0.5f || (slot==slot_&&distance<=nearest+0.5f)){
            nearest=distance;target=slot;
        }
    }
    return target;
}
void RootsGraph::mouseDown(const juce::MouseEvent& e){
    mouse_=e.position;hover_=true;
    if(!e.mods.isPopupMenu()){
        const int tab=hitCarveTab(e.position);
        if(tab>=0){
            if(onSelectCarve)onSelectCarve(tab);
            repaint();
            return;
        }
        const int slot=hitCarveNode(e.position);
        if(slot>=0){
            // Dragging only moves an explicitly enabled, visible CARVE.
            draggingCarve_=slot;
            const int root=selected_/2;
            dragStartX_=e.position.x;
            dragStartHz_=pval(processor_.apvts,
                PRISMVSTAudioProcessor::carveId(root,slot,"hz"));
            fineDragging_=e.mods.isCtrlDown();
            if(onSelectCarve)onSelectCarve(slot);
            for(const auto* suffix:{"hz","depth"})
                if(auto* parameter=processor_.apvts.getParameter(
                       PRISMVSTAudioProcessor::carveId(root,slot,suffix)))
                    parameter->beginChangeGesture();
            return;
        }
    }
    const int hit=hitNode(e.position);
    if(hit>=0)selectNode(hit);
    if(e.mods.isPopupMenu()){
        if(hit<0){
            const float f=xToFreq(e.position.x);
            int target=6;
            for(int root=0;root<7;++root){
                const float low=param(root*2,"hz");
                const float high=param(root*2+1,"hz");
                if(f>=low&&f<=high){target=root;break;}
            }
            selectNode(target*2);
        }
        popup();return;
    }
    dragging_=hit;
    if(dragging_>=0){
        if(auto* p=processor_.apvts.getParameter(
             PRISMVSTAudioProcessor::rootsNodeId(dragging_,"hz")))
            p->beginChangeGesture();
    }
}
void RootsGraph::mouseDrag(const juce::MouseEvent& e){
    if(draggingCarve_>=0){
        const int root=selected_/2,slot=draggingCarve_;
        const auto id=[root,slot](const char* s){
            return PRISMVSTAudioProcessor::carveId(root,slot,s);
        };
        const float a=param(root*2,"hz"),b=param(root*2+1,"hz");
        const bool fine=e.mods.isCtrlDown();
        if(fine&&!fineDragging_){
            // Ctrl may be pressed in the middle of a drag: anchor current value.
            dragStartX_=e.position.x;
            dragStartHz_=pval(processor_.apvts,id("hz"));
        }
        fineDragging_=fine;
        const float next=surgery::precision::scrubFrequency(
            dragStartHz_,e.position.x-dragStartX_,xToFreq(e.position.x),
            fine,a,b);
        change(processor_,id("hz"),next);
        if(!fine)
            change(processor_,id("depth"),surgery::ui::depthFromY(
                area().getCentreY(),e.position.y));
        repaint();return;
    }
    if(dragging_<0)return;
    const float newHz=xToFreq(e.position.x);
    // Deliberately ignore e.position.y: frequency handles have no gain axis.
    write(dragging_,"hz",newHz);
    repaint();
}
void RootsGraph::mouseUp(const juce::MouseEvent&){
    if(draggingCarve_>=0){
        const int root=selected_/2,slot=draggingCarve_;
        for(const auto* name:{"hz","depth"})
            if(auto* p=processor_.apvts.getParameter(
                PRISMVSTAudioProcessor::carveId(root,slot,name)))
                p->endChangeGesture();
        draggingCarve_=-1;
        fineDragging_=false;
    }
    if(dragging_>=0){
        if(auto* p=processor_.apvts.getParameter(
             PRISMVSTAudioProcessor::rootsNodeId(dragging_,"hz")))
            p->endChangeGesture();
    }
    if(dragging_>=0)clampCarvesToRoot(dragging_/2);
    dragging_=-1;
}
void RootsGraph::mouseMove(const juce::MouseEvent& e){mouse_=e.position;hover_=true;repaint();}
void RootsGraph::mouseExit(const juce::MouseEvent&){hover_=false;repaint();}
void RootsGraph::mouseDoubleClick(const juce::MouseEvent& e){
    if(e.mods.isPopupMenu())return;
    // Finish any gesture opened by the second mouse-down before resetting.
    const int tab=hitCarveTab(e.position);
    const int carve=tab>=0?tab:hitCarveNode(e.position);
    const int boundary=carve<0?hitNode(e.position):-1;
    if(draggingCarve_>=0||dragging_>=0)mouseUp(e);
    if(tab>=0&&pval(processor_.apvts,
        PRISMVSTAudioProcessor::carveId(selected_/2,tab,"on"))<0.5f){
        if(onSelectCarve)onSelectCarve(tab);
        return; // An OFF selector cannot resurrect its invisible node.
    }
    if(carve>=0){
        if(onSelectCarve)onSelectCarve(carve);
        processor_.resetCarveNode(selected_/2,carve);
        repaint();
        return;
    }
    if(boundary>=0){
        selectNode(boundary);
        processor_.resetRootBoundary(boundary);
        clampCarvesToRoot(boundary/2);
        repaint();
    }
}
void RootsGraph::mouseWheelMove(const juce::MouseEvent& e,
                                const juce::MouseWheelDetails& wheel){
    const int slot=hitCarveNode(e.position);
    if(slot<0)return; // wheel over ROOT boundary never changes another parameter
    const int root=selected_/2;
    const auto id=[root,slot](const char* suffix){
        return PRISMVSTAudioProcessor::carveId(root,slot,suffix);
    };
    if(onSelectCarve)onSelectCarve(slot);
    if(e.mods.isCtrlDown()){
        const float a=param(root*2,"hz"),b=param(root*2+1,"hz");
        const auto frequency=surgery::precision::scrubWheelFrequency(
            pval(processor_.apvts,id("hz")),wheel.deltaY*10.0f,a,b);
        change(processor_,id("hz"),frequency);
    }else{
        // Wheel changes mathematical quality factor; Shift grants finer steps.
        const float q=surgery::precision::adjustQ(
            pval(processor_.apvts,id("q")),wheel.deltaY*10.0f,
            e.mods.isShiftDown());
        change(processor_,id("q"),q);
    }
    repaint();
}
void RootsGraph::clampCarvesToRoot(int root){
    if(root<0||root>=7)return;
    const auto a=param(root*2,"hz"),b=param(root*2+1,"hz");
    for(int slot=0;slot<4;++slot){
        const auto id=PRISMVSTAudioProcessor::carveId(root,slot,"hz");
        const float raw=pval(processor_.apvts,id);
        const auto bound=surgery::precision::boundedFrequency(raw,a,b);
        if(std::abs(raw-bound)>0.005f)change(processor_,id,bound);
    }
}
void RootsGraph::popup(){
    const int root=selected_/2;
    const auto rid=[root](const char* suffix){
        return PRISMVSTAudioProcessor::rootsRootControlId(root,suffix);
    };
    const auto current=[this,&rid](const char* suffix){
        return pval(processor_.apvts,rid(suffix));
    };
    juce::PopupMenu menu;
    menu.addSectionHeader(PRISMVSTAudioProcessor::sectionName(root));
    menu.addItem(1,"ROOT ENABLED",true,current("on")>=0.5f);
    const bool solo=pval(processor_.apvts,
        PRISMVSTAudioProcessor::rootsRootSoloId(root))>=0.5f;
    menu.addItem(2,"SOLO ENTIRE ROOT / OFF",true,solo);
    menu.addSeparator();
    menu.addSectionHeader("ROOT GAIN + CARVE / EQ DOMAIN");
    menu.addItem(10,"STEREO",true,juce::roundToInt(current("domain"))==0);
    menu.addItem(11,"MID",true,juce::roundToInt(current("domain"))==1);
    menu.addItem(12,"SIDE",true,juce::roundToInt(current("domain"))==2);
    menu.addSeparator();
    menu.addSectionHeader("ROOT COMPRESSOR CHANNEL");
    static const char* dn[]{"STEREO LINK","DUAL MID/SIDE","MID ONLY","SIDE ONLY"};
    for(int i=0;i<4;++i)
        menu.addItem(30+i,dn[i],true,juce::roundToInt(current("detector"))==i);
    menu.addSeparator();
    menu.addItem(15,"MONO SELECTED ROOT",true,current("mono")>=0.5f);
    menu.addSeparator();
    juce::PopupMenu palette;
    static constexpr const char* names[]{"CYAN","AMBER","JADE","VIOLET","ROSE","BLUE","LIME","WHITE"};
    for(int i=0;i<8;++i)palette.addItem(80+i,names[i]);
    palette.addItem(99,"CUSTOM HEX...");
    menu.addSubMenu("ROOT COLOUR",palette);
    const juce::Component::SafePointer<RootsGraph> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [safe,root](int id){
            if(safe==nullptr||id<=0)return;
            auto& graph=*safe;
            const auto rid=[root](const char* suffix){
                return PRISMVSTAudioProcessor::rootsRootControlId(root,suffix);
            };
            const auto value=[&graph,&rid](const char* suffix){
                return pval(graph.processor_.apvts,rid(suffix));
            };
            const auto set=[&graph,&rid](const char* suffix,float v){
                change(graph.processor_,rid(suffix),v);
            };
            if(id==1)set("on",value("on")>=0.5f?0.0f:1.0f);
            if(id==2){
                const auto key=PRISMVSTAudioProcessor::rootsRootSoloId(root);
                const bool on=pval(graph.processor_.apvts,key)>=0.5f;
                change(graph.processor_,key,on?0.0f:1.0f);
            }
            if(id>=10&&id<=12)set("domain",(float)(id-10));
            if(id==15)set("mono",value("mono")>=0.5f?0.0f:1.0f);
            if(id>=30&&id<=33)set("detector",(float)(id-30));

            const auto colorId="surgery_root"+juce::String(root+1)+"_rgb";
            static constexpr unsigned swatches[]{
                0x6BBDD4,0xDFA871,0x8DCEA8,0xD2A2E5,
                0xE89EAD,0x9DADEA,0xBFD678,0xE5F4F3};
            if(id>=80&&id<=87)change(graph.processor_,colorId,(float)swatches[id-80]);
            if(id==99){
                auto* dialog=new juce::AlertWindow(
                    "ROOT COLOUR","Enter a 6-digit RGB hexadecimal value",
                    juce::MessageBoxIconType::NoIcon);
                dialog->addTextEditor("hex","6BBDD4","RRGGBB");
                dialog->addButton("APPLY",1);
                dialog->addButton("CANCEL",0);
                dialog->enterModalState(true,
                    juce::ModalCallbackFunction::create(
                        [safe,dialog,colorId](int result) {
                            if(result!=1||safe==nullptr)return;
                            auto str=dialog->getTextEditorContents("hex").trim().removeCharacters("#");
                            if(str.length()!=6||!str.containsOnly("0123456789aAbBcCdDeEfF"))return;
                            const unsigned value=(unsigned)str.getHexValue32()&0x00ffffffu;
                            change(safe->processor_,colorId,(float)value);
                            safe->repaint();
                        }),true);
            }
            graph.repaint();
        });
}
RootsEditor::RootsEditor(PRISMVSTAudioProcessor& p)
: juce::AudioProcessorEditor(&p),processor_(p),graph_(p) {
    setLookAndFeel(&lookAndFeel_);
    rackChassis_=juce::ImageCache::getFromMemory(BinaryData::chassis1_0_3_png,BinaryData::chassis1_0_3_pngSize);

    addAndMakeVisible(graph_);
    graph_.onSelectNode=[this](int n){bindNode(n);};
    graph_.onSelectCarve=[this](int slot){
        carveSlot_=juce::jlimit(0,3,slot);
        bindCarve();
        if(mode_!=1)setMode(1); // Foreground slot selection opens its real controls.
    };
    for(int root=0;root<7;++root){
        auto& b=rootButtons_[(size_t)root];
        b.setButtonText("BAND "+juce::String(root+1));
        b.onClick=[this,root](){graph_.selectNode(root*2);};
        b.onContext=[this,root](){graph_.showRootMenu(root);};
        b.setColour(juce::TextButton::textColourOffId,
                    juce::Colour::fromRGB(192,213,220));
        addAndMakeVisible(b);
    }
    for(auto* label:{&title_,&selection_,&status_,&gainLabel_,
                     &thresholdLabel_,&carveFrequencyLabel_,&carveDepthLabel_,
                     &carveQLabel_,&analyzerDepthLabel_,
                     &analyzerSlopeLabel_,&analyzerMemoryLabel_,&analyzerLegend_}){
        addAndMakeVisible(*label);
        label->setColour(juce::Label::textColourId,juce::Colour::fromRGB(205,224,229));
        label->setFont(juce::Font(juce::FontOptions(12.0f,juce::Font::plain)));
        label->setJustificationType(juce::Justification::centredLeft);
    }
    title_.setText("SCALPEL EQ",juce::dontSendNotification);
    analyzerLegend_.setFont(juce::Font(juce::FontOptions(12.0f,juce::Font::bold)));
    analyzerLegend_.setText("SYNESTHESIA V1   /   HUE x ENERGY",
                            juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions(24.0f,juce::Font::bold)));
    selection_.setFont(juce::Font(juce::FontOptions(12.0f,juce::Font::bold)));
    status_.setFont(juce::Font(juce::FontOptions(11.5f,juce::Font::plain)));
    status_.setJustificationType(juce::Justification::centredRight);
    for(auto* s:{&gain_,&threshold_,&limitAmount_}){
        addAndMakeVisible(*s);rotary(*s,"",0.0);
        s->setColour(juce::Slider::rotarySliderFillColourId,juce::Colour::fromRGB(143,192,204));
        s->setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour::fromRGB(38,55,65));
        s->setColour(juce::Slider::textBoxTextColourId,juce::Colour::fromRGB(239,243,245));
        s->setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour::fromRGB(13,19,25));
        s->setColour(juce::Slider::textBoxOutlineColourId,juce::Colour::fromRGB(38,55,65));
    }
    gainLabel_.setText("ROOT GAIN (dB)",juce::dontSendNotification);
    gain_.setTooltip("Entire ROOT frequency range gain: -60 to +60 dB");

    thresholdLabel_.setText("COMP THRESHOLD",juce::dontSendNotification);
    threshold_.setTooltip("Compression stays OFF until you choose a COMPRESSION profile. Double-click resets to 0 dBFS.");
    limitAmount_.setTextValueSuffix(" %");
    limitAmount_.setDoubleClickReturnValue(true,0.0);
    limitAmount_.setTooltip("LA-2A-inspired optical leveling depth. Program-dependent attack/release; not brickwall limiting.");
    for(auto* b:{&mono_,&on_,&solo_,&phase_,&limit_}){
        b->setClickingTogglesState(true);
        b->setColour(juce::TextButton::buttonColourId,juce::Colour::fromRGB(24,38,47));
        b->setColour(juce::TextButton::buttonOnColourId,juce::Colour::fromRGB(43,153,166));
        b->setColour(juce::TextButton::textColourOffId,juce::Colours::white);
        b->setColour(juce::TextButton::textColourOnId,juce::Colours::white);
        addAndMakeVisible(*b);
    }
    static constexpr const char* labels[]{"BAND MODE","CARVE","COLOR","ANALYZE"};
    for(int i=0;i<4;++i){
        auto& b=modeButtons_[(size_t)i];
        b.setButtonText(labels[i]);
        b.onClick=[this,i]{setMode(i);};
        addAndMakeVisible(b);
    }
    for(int i=0;i<4;++i){
        auto& b=carveSlotButtons_[(size_t)i];
        b.setButtonText("C"+juce::String(i+1));
        b.onClick=[this,i]{carveSlot_=i;bindCarve();graph_.setPresentation(mode_,carveSlot_);};
        addAndMakeVisible(b);
    }
    for(auto* s:{&carveFrequency_,&carveDepth_,&carveQ_}){
        rotary(*s,"",0.0);addAndMakeVisible(*s);
    }
    carveDepth_.setDoubleClickReturnValue(true,0.0);
    carveQ_.setDoubleClickReturnValue(true,2.0);
    carveFrequency_.setMouseDragSensitivity(2000);
    carveQ_.setMouseDragSensitivity(2800);
    carveQ_.setNumDecimalPlacesToDisplay(3);
    carveFrequency_.setTooltip("Exact Hz value: type in the field. CTRL+drag a CARVE node for fine frequency scrubbing.");
    carveQ_.setTooltip("Ultra-surgical Q 0.300 to 96.000. Higher = tighter notch; 0-100% tightness shown above knob. SHIFT+wheel = fine Q.");
    carveFrequencyLabel_.setText("FREQUENCY Hz",juce::dontSendNotification);
    carveDepthLabel_.setText("CUT DEPTH dB",juce::dontSendNotification);
    carveQLabel_.setText("Q / TIGHTNESS",juce::dontSendNotification);
    for(auto* b:{&carveOn_,&carveDynamic_}){
        b->setClickingTogglesState(true);
        addAndMakeVisible(*b);
    }
    const std::array<const char*,5> names{"DYNAMIC","SPARKLE","SHINE","SMOOTHENING","VINTAGE TAPE"};
    for(int i=0;i<5;++i){
        auto& s=onyxSliders_[(size_t)i];
        rotary(s," %",0.0);s.setRange(0.0,100.0,0.1);
        s.setMouseDragSensitivity(1200);
        static const char* tips[]{"Independent 3 ms / 75 ms transient contrast, +3 dB / -2 dB bounded.","Transient harmonic shimmer; zero is neutral.","Smooth bounded linear presence shelf, max +2.5 dB.","Dynamic HF edge taming; max about -3 dB.","Gentle ADAA tape warmth and high-frequency rounding."};
        s.setTooltip(tips[i]);
        s.setDoubleClickReturnValue(true,0.0);
        addAndMakeVisible(s);
        onyxLabels_[(size_t)i].setText(names[(size_t)i],juce::dontSendNotification);
        onyxLabels_[(size_t)i].setJustificationType(juce::Justification::centred);
        onyxLabels_[(size_t)i].setColour(juce::Label::textColourId,juce::Colours::white);
        addAndMakeVisible(onyxLabels_[(size_t)i]);
    }
    mono_.setTooltip("Frequency-selective MONO on the selected ROOT only");
    phase_.setTooltip("Automatic per-band coherence guard: gently reduces anti-correlated SIDE power, with smoothed engagement.");
    limit_.setTooltip("Enable per-band optical leveling; adjust amount with the adjacent single knob.");
    analyzerDepthLabel_.setText("VIEW RANGE",juce::dontSendNotification);
    analyzerSlopeLabel_.setText("PIVOT / dB PER OCT",juce::dontSendNotification);
    analyzerMemoryLabel_.setText("VISUAL DECAY / SEC",juce::dontSendNotification);
    analyzerDepth_.addItem("MIX -60 dBFS",1);
    analyzerDepth_.addItem("DEEP -120 dBFS",2);
    analyzerDepth_.addItem("FORENSIC -144 dBFS",3);
    addAndMakeVisible(analyzerDepth_);
    analyzerDepthA_=std::make_unique<ComboA>(processor_.apvts,"analyzer_depth",analyzerDepth_);
    for(auto* s:{&analyzerSlope_,&analyzerMemory_}){
        rotary(*s,"",s==&analyzerSlope_?0.0:1.5);
        addAndMakeVisible(*s);
    }
    analyzerSlopeA_=std::make_unique<SliderA>(processor_.apvts,"analyzer_slope",analyzerSlope_);
    analyzerMemoryA_=std::make_unique<SliderA>(processor_.apvts,"aura_memory",analyzerMemory_);
    heatToggle_.setTooltip("Synesthesia V1: stationary frequency-to-hue / measured dBFS-to-brightness. Full rainbow, no flashing. Prototype visual mapping, not human synesthesia detection.");
    heatToggle_.setClickingTogglesState(true);
    addAndMakeVisible(heatToggle_);
    heatA_=std::make_unique<ButtonA>(processor_.apvts,"aura_color",heatToggle_);
    xrayButton_.setClickingTogglesState(true);
    xrayButton_.setTooltip("X-RAY: restrained sound-reactive black mercury and synesthesia. Visual-only, no changes to audio DSP.");
    addAndMakeVisible(xrayButton_);
    xrayAttachment_=std::make_unique<ButtonA>(processor_.apvts,"scalpel_xray",xrayButton_);
    freezeButton_.setClickingTogglesState(true);
    freezeButton_.setTooltip("Freeze measured spectrum and SONAR history; DSP remains live");
    freezeButton_.onClick=[this]{
        graph_.setFrozen(freezeButton_.getToggleState());
        freezeButton_.setButtonText(graph_.isFrozen()?"RESUME":"FREEZE");
    };
    addAndMakeVisible(freezeButton_);
    weatherMode_.addItem("Standard+",8);addAndMakeVisible(weatherMode_);
    weatherMode_.setTooltip("Standard+ is the sole SONAR presentation. Historical mode values recall as Standard+; SONAR ON/OFF remains independent of JUKE.");
    weatherModeA_=std::make_unique<SurgeryVisualChoiceAttachment>(*processor_.apvts.getParameter("sonar_visual_style"),*processor_.apvts.getParameter("sonar_weather_mode"),weatherMode_);
    jukeMode_.addItemList({"Real Jukebox","Storm Tube","Symbiote Flow","Pulse Rail","Ambient Idle","Off"},1);addAndMakeVisible(jukeMode_);
    jukeModeA_=std::make_unique<ComboA>(processor_.apvts,"juke_mode",jukeMode_);
    jukeMode_.setTooltip("Independent hollow JUKE chambers: shared music analysis, separate visual style. Off deactivates the border energy.");
    jukeModeLabel_.setText("JUKE MODE",juce::dontSendNotification);addAndMakeVisible(jukeModeLabel_);
    jukeModeLabel_.setFont(juce::Font(juce::FontOptions(11.f)));jukeModeLabel_.setColour(juce::Label::textColourId,juce::Colour(0xffb8c5d1));
    reducedMotion_.setClickingTogglesState(true);addAndMakeVisible(reducedMotion_);
    reducedMotion_.setTooltip("Suppresses weather drift and lightning. Enabled by default. Full motion still limits local flashes to one per 0.75 seconds.");
    reducedMotionA_=std::make_unique<ButtonA>(processor_.apvts,"sonar_reduced_motion",reducedMotion_);
    analyzerLegend_.setText("SONAR 2.1 / VISUAL MODE",juce::dontSendNotification);
    compressor_.addItemList({ "COMP OFF", "OPTICAL 3:1", "OPTICAL 4:1", "TUBE 3:1", "FAST 4:1" },1);
    domain_.addItemList({ "STEREO", "MID", "SIDE" },1);
    detector_.addItemList({ "LINKED", "DUAL M/S", "MID ONLY", "SIDE ONLY" },1);
    for(auto* box:{&compressor_,&domain_,&detector_})addAndMakeVisible(*box);
    compLabel_.setText("COMPRESSION",juce::dontSendNotification);
    domainLabel_.setText("EQ DOMAIN",juce::dontSendNotification);
    detectorLabel_.setText("DYNAMIC",juce::dontSendNotification);
    for(auto* label:{&compLabel_,&domainLabel_,&detectorLabel_,&outputLabel_}){
        label->setFont(juce::Font(juce::FontOptions(11.0f)));
        label->setColour(juce::Label::textColourId,juce::Colour(0xffafc8cf));
        addAndMakeVisible(*label);
    }
    for(auto* button:{&bypass_,&compare_,&copy_,&save_,&load_,&peakHold_})addAndMakeVisible(*button);
    peakHold_.onClick=[this]{processor_.resetPeakHold();};
    peakHold_.setTooltip("Maximum output sample peak. Click to reset. CLIP warns of digital full-scale overload.");
    bypass_.setClickingTogglesState(true);
    bypass_.setTooltip("5 ms dry bypass crossfade; processing stays warm. Fully bypassed output is exact input.");
    bypassA_=std::make_unique<ButtonA>(processor_.apvts,"surgery_bypass",bypass_);
    compare_.onClick=[this]{processor_.switchComparison();timerCallback();};
    copy_.onClick=[this]{processor_.copyComparison();};
    compare_.setTooltip("Switch between two complete settings for this plugin instance. Save presets to keep either setting.");
    copy_.setTooltip("Copy the active comparison into the other slot.");
    save_.onClick=[this]{choosePreset(true);};
    load_.onClick=[this]{choosePreset(false);};
    output_.setSliderStyle(juce::Slider::LinearHorizontal);
    output_.setTextBoxStyle(juce::Slider::TextBoxRight,false,68,22);
    output_.setTextValueSuffix(" dB");output_.setDoubleClickReturnValue(true,0.0);
    output_.setTooltip("Output trim with 20 ms smoothing. Double-click for unity.");
    outputLabel_.setText("OUTPUT",juce::dontSendNotification);
    addAndMakeVisible(output_);
    outputA_=std::make_unique<SliderA>(processor_.apvts,"surgery_output_db",output_);
    bindNode(0);setMode(2);
    setResizable(true,true);setResizeLimits(840,610,1800,1200);setSize(1320,880);
    timerCallback();startTimerHz(60);
}
RootsEditor::~RootsEditor(){stopTimer();setLookAndFeel(nullptr);}
void RootsEditor::choosePreset(bool saving){
    presetChooser_=std::make_unique<juce::FileChooser>(saving?"Save Scalpel EQ preset":"Load Scalpel EQ preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Scalpel EQ.surgerypreset"),"*.surgerypreset");
    const auto chooserFlags=saving?(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting):
        (juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles);
    presetChooser_->launchAsync(chooserFlags,[safe=juce::Component::SafePointer<RootsEditor>(this),saving](const juce::FileChooser& chooser){
        if(safe==nullptr)return;
        auto file=chooser.getResult();if(file==juce::File{})return;
        const bool ok=saving?safe->processor_.savePreset(file.withFileExtension("surgerypreset")):safe->processor_.loadPreset(file);
        if(!ok)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Preset could not be opened",
            saving?"Could not write this preset. Choose a writable folder.":"Choose a valid Scalpel EQ preset file.");
        safe->timerCallback();
    });
}
void RootsEditor::bindNode(int n){
    node_=juce::jlimit(0,prism::roots::v2::kNodes-1,n);
    for(auto& a:onyxAttachments_)a.reset();
    const std::array<const char*,5> names{"color_dynamic","onyx_sparkle","onyx_shine","onyx_smooth","onyx_tape"};
    for(int i=0;i<5;++i)
        onyxAttachments_[(size_t)i]=std::make_unique<SliderA>(
            processor_.apvts,
            PRISMVSTAudioProcessor::rootsRootControlId(node_/2,names[(size_t)i]),
            onyxSliders_[(size_t)i]);
    compressorA_.reset();domainA_.reset();detectorA_.reset();
    gainA_.reset();thresholdA_.reset();limitAmountA_.reset();monoA_.reset();
    onA_.reset();soloA_.reset();phaseA_.reset();limitA_.reset();
    const auto rid=[this](const char* suffix){
        return PRISMVSTAudioProcessor::rootsRootControlId(node_/2,suffix);
    };
    compressorA_=std::make_unique<ComboA>(processor_.apvts,rid("comp"),compressor_);
    domainA_=std::make_unique<ComboA>(processor_.apvts,rid("domain"),domain_);
    detectorA_=std::make_unique<ComboA>(processor_.apvts,rid("detector"),detector_);
    gainA_=std::make_unique<SliderA>(processor_.apvts,
       PRISMVSTAudioProcessor::rootsRootGainId(node_/2),gain_);

    thresholdA_=std::make_unique<SliderA>(processor_.apvts,rid("threshold"),threshold_);
    limitAmountA_=std::make_unique<SliderA>(processor_.apvts,rid("limit_amount"),limitAmount_);
    phaseA_=std::make_unique<ButtonA>(processor_.apvts,rid("phase"),phase_);
    limitA_=std::make_unique<ButtonA>(processor_.apvts,rid("limit_on"),limit_);
    monoA_=std::make_unique<ButtonA>(processor_.apvts,rid("mono"),mono_);
    onA_=std::make_unique<ButtonA>(processor_.apvts,rid("on"),on_);
    soloA_=std::make_unique<ButtonA>(processor_.apvts,
        PRISMVSTAudioProcessor::rootsRootSoloId(node_/2),solo_);
    selection_.setText("ROOT "+juce::String(node_/2+1)+
        "  /  BOUNDARY "+juce::String(node_%2+1),juce::dontSendNotification);
    bindCarve();
    updateRootButtons();
}
void RootsEditor::bindCarve(){
    carveFrequencyA_.reset();carveDepthA_.reset();
    carveQA_.reset();
    carveOnA_.reset();carveDynamicA_.reset();
    const int root=node_/2,slot=carveSlot_;
    const float a=pval(processor_.apvts,PRISMVSTAudioProcessor::rootsNodeId(root*2,"hz"));
    const float b=pval(processor_.apvts,PRISMVSTAudioProcessor::rootsNodeId(root*2+1,"hz"));
    const double low=juce::jmax(20.0f,std::min(a,b));
    const double high=juce::jmax((float)low,std::max(a,b));
    carveFrequency_.setDoubleClickReturnValue(true,
        std::exp(std::log(low)+(slot+1.0)/5.0*std::log(high/low)));
    const auto id=[root,slot](const char* s){
        return PRISMVSTAudioProcessor::carveId(root,slot,s);
    };
    carveFrequency_.onValueChange=[this]{
        const int root=node_/2;
        const auto a=pval(processor_.apvts,
            PRISMVSTAudioProcessor::rootsNodeId(root*2,"hz"));
        const auto b=pval(processor_.apvts,
            PRISMVSTAudioProcessor::rootsNodeId(root*2+1,"hz"));
        const double value=carveFrequency_.getValue();
        const auto limited=surgery::precision::boundedFrequency((float)value,a,b);
        if(std::abs(value-limited)>0.001)
            carveFrequency_.setValue(limited,juce::sendNotificationSync);
    };
    carveFrequencyA_=std::make_unique<SliderA>(processor_.apvts,id("hz"),carveFrequency_);
    carveDepthA_=std::make_unique<SliderA>(processor_.apvts,id("depth"),carveDepth_);
    carveQA_=std::make_unique<SliderA>(processor_.apvts,id("q"),carveQ_);
    carveOnA_=std::make_unique<ButtonA>(processor_.apvts,id("on"),carveOn_);
    carveDynamicA_=std::make_unique<ButtonA>(processor_.apvts,id("dynamic"),carveDynamic_);
    carveOn_.setButtonText(pval(processor_.apvts,id("on"))>=0.5f?
                           "CARVE ON":"CARVE OFF");
    for(int i=0;i<4;++i){
        carveSlotButtons_[(size_t)i].setToggleState(i==slot,juce::dontSendNotification);
        carveSlotButtons_[(size_t)i].setColour(juce::TextButton::buttonOnColourId,
            territory(root).withAlpha(0.66f));
    }
    graph_.setPresentation(mode_,carveSlot_);
}
void RootsEditor::setMode(int next){
    mode_=juce::jlimit(0,3,next);
    for(int i=0;i<4;++i){
        auto& b=modeButtons_[(size_t)i];
        b.setToggleState(i==mode_,juce::dontSendNotification);
        b.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff1a2933));
        b.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff247a83));
        b.setColour(juce::TextButton::textColourOffId,juce::Colours::white);
        b.setColour(juce::TextButton::textColourOnId,juce::Colours::white);
    }
    const bool carve=mode_==1;
    for(auto* s:{&carveFrequency_,&carveDepth_,&carveQ_})s->setVisible(carve);
    for(auto* l:{&carveFrequencyLabel_,&carveDepthLabel_,&carveQLabel_})l->setVisible(carve);
    for(auto& b:carveSlotButtons_)b.setVisible(carve);
    carveOn_.setVisible(carve);carveDynamic_.setVisible(carve);
    analyzerLegend_.setVisible(mode_==3);
    weatherMode_.setVisible(mode_==3);jukeMode_.setVisible(mode_==3);jukeModeLabel_.setVisible(mode_==3);reducedMotion_.setVisible(mode_==3);
    for(auto* box:{&compressor_,&domain_,&detector_})box->setVisible(mode_==0);
    for(auto* label:{&compLabel_,&domainLabel_,&detectorLabel_})label->setVisible(mode_==0);
    gain_.setVisible(mode_==0);gainLabel_.setVisible(mode_==0);
    threshold_.setVisible(mode_==0);
    thresholdLabel_.setVisible(mode_==0);
    limitAmount_.setVisible(mode_==0);
    phase_.setVisible(mode_==0);
    limit_.setVisible(mode_==0);
    for(int i=0;i<5;++i){
        onyxSliders_[(size_t)i].setVisible(mode_==2);
        onyxLabels_[(size_t)i].setVisible(mode_==2);
    }
    on_.setVisible(mode_==0);
    solo_.setVisible(mode_==0);
    mono_.setVisible(mode_==0);
    const bool analyze=mode_==3;
    analyzerDepth_.setVisible(analyze);
    analyzerDepthLabel_.setVisible(analyze);
    analyzerSlope_.setVisible(analyze);
    analyzerSlopeLabel_.setVisible(analyze);
    analyzerMemory_.setVisible(analyze);
    analyzerMemoryLabel_.setVisible(analyze);
    heatToggle_.setVisible(analyze);
    freezeButton_.setVisible(analyze);
    graph_.setPresentation(mode_,carveSlot_);
    resized();
    timerCallback();
    repaint();
}
void RootsEditor::updateRootButtons(){
    const double nowMs=juce::Time::getMillisecondCounterHiRes();
    for(int root=0;root<7;++root){
        const bool selected=graph_.selectedNode()/2==root;
        auto& button=rootButtons_[(size_t)root];
        button.setToggleState(selected,juce::dontSendNotification);
        if(processor_.getRootBandPeakDb(root)>=-0.01f)
            peakWarningUntilMs_[(size_t)root]=nowMs+750.0;
        const bool peak=nowMs<peakWarningUntilMs_[(size_t)root];
        const float pulse=.35f+.25f*(float)std::sin(nowMs*.0043);
        button.setColour(juce::TextButton::buttonOnColourId,
            peak?juce::Colour(0xffdf1715).withAlpha(pulse):territory(root).withAlpha(.52f));
        button.setColour(juce::TextButton::buttonColourId,
            peak?juce::Colour(0xffde1714).withAlpha(pulse):juce::Colour(0xffdbc5ac));
    }
}
void RootsEditor::timerCallback(){
    const bool b=processor_.comparisonSlot()==1;
    compare_.setButtonText(b?"A / B: B":"A / B: A");
    copy_.setButtonText(b?"COPY B > A":"COPY A > B");
    const float held=processor_.getPeakHoldDb();
    peakHold_.setButtonText((held>=0.0f?"CLIP ":"PEAK ")+juce::String(held,1)+" dB");
    peakHold_.setToggleState(held>=0.0f,juce::dontSendNotification);
    const auto labelColour=juce::Colour(0xff352820);
    for(auto* label:{&title_,&selection_,&status_,&gainLabel_,&thresholdLabel_,
                    &carveFrequencyLabel_,&carveDepthLabel_,&carveQLabel_,
                    &analyzerDepthLabel_,&analyzerSlopeLabel_,&analyzerMemoryLabel_,&analyzerLegend_,
                    &compLabel_,&domainLabel_,&detectorLabel_,&outputLabel_})
        label->setColour(juce::Label::textColourId,labelColour);
    for(auto& label:onyxLabels_)label.setColour(juce::Label::textColourId,labelColour);
    // Threshold remains editable while OFF without activating compression.
    threshold_.setEnabled(true);
thresholdLabel_.setText("COMP THRESHOLD",
                            juce::dontSendNotification);
    updateRootButtons();
    const float factor=pval(processor_.apvts,
        PRISMVSTAudioProcessor::carveId(node_/2,carveSlot_,"q"));
    const float tight=surgery::precision::tightnessPercent(factor);
    carveQLabel_.setText("Q / "+juce::String(tight,1)+"% TIGHT",
                         juce::dontSendNotification);
    carveOn_.setButtonText(pval(processor_.apvts,
       PRISMVSTAudioProcessor::carveId(node_/2,carveSlot_,"on"))>=0.5f?
       "CARVE ON":"CARVE OFF");
    heatToggle_.setButtonText(pval(processor_.apvts,"aura_color")>=0.5f?
                              "SONAR ON":"SONAR OFF");
    graph_.setPresentation(mode_,carveSlot_);
    std::array<float,PRISMVSTAudioProcessor::labBars> bars{},peaks{};
    processor_.copyPrismLabBars(bars,peaks);
    graph_.updateAnalyzer(bars,peaks);
    const bool xray=pval(processor_.apvts,"scalpel_xray")>=0.5f;
    graph_.setXray(xray);
    const double now=juce::Time::getMillisecondCounterHiRes()/1000.;
    const double elapsed=weatherTick_>0?now-weatherTick_:1./60.;weatherTick_=now;
    surgery::weather::Snapshot weatherSnapshot;
    const bool newWeather=processor_.readWeather(weatherSnapshot);
    constexpr int style=7; // Standard+ is the only active SONAR mode.
    const int juke=juce::roundToInt(pval(processor_.apvts,"juke_mode"));
    const bool sonar=pval(processor_.apvts,"aura_color")>=.5f,reduced=pval(processor_.apvts,"sonar_reduced_motion")>=.5f;
    if(weatherMode_.getSelectedId()!=style+1)weatherMode_.setSelectedId(style+1,juce::dontSendNotification);
    graph_.updateWeather(newWeather?&weatherSnapshot:nullptr,elapsed,(sonar&&style>0)||juke<5||xray,reduced,pval(processor_.apvts,"aura_memory"),sonar?style:0);
    if(stormChassis_.update(graph_.weatherMeaning(),elapsed,pval(processor_.apvts,"aura_memory"),graph_.isFrozen(),reduced,juke<5,juke)){
        repaint(surgery::rack::StormChassis::top(getWidth()).expanded(6).toNearestInt());
        for(bool right:{false,true})repaint(surgery::rack::StormChassis::side(getWidth(),getHeight(),right).expanded(6).toNearestInt());
    }
    const auto state=processor_.getRootsMeter();
    const juce::String status=
        "IN "+juce::String(processor_.getInputPeakDb(),1)+" dBFS  /  OUT "+
        juce::String(processor_.getPeakDb(),1)+" dBFS  /  CORR "+
        juce::String(state.correlation,2)+"  /  ROOT GR "+
        juce::String(state.largestGrDb,1)+" dB";
    if(mode_==3){
        const auto metrics=graph_.spectralMetrics();
        const juce::String line=metrics.signalPresent?
            "SYN V1 / BIN ~"+hzText(metrics.loudestHz)+"Hz "+pitchText(metrics.loudestHz)+" / "+
            juce::String(metrics.loudestDb,1)+" dBFS  /  CENTROID ~"+
            hzText(metrics.approximateCentroidHz)+"Hz":
            "SYN V1 / SILENCE OR BELOW DISPLAY FLOOR";
        status_.setText(line,juce::dontSendNotification);
    } else status_.setText(status,juce::dontSendNotification);
}
void RootsEditor::paint(juce::Graphics& g){
    // Cache in physical pixels. Crossing a DPI boundary or changing host UI
    // scale rebuilds the surface even when logical editor dimensions match.
    const float dpi=juce::jlimit(.5f,4.f,g.getInternalContext().getPhysicalPixelScaleFactor());
    const int pw=juce::roundToInt(getWidth()*dpi),ph=juce::roundToInt(getHeight()*dpi);
    if(rackBackground_.getWidth()!=pw||rackBackground_.getHeight()!=ph||backgroundMode_!=mode_||backgroundScale_!=dpi){
        rackBackground_=juce::Image(juce::Image::RGB,pw,ph,false);
        juce::Graphics background(rackBackground_);background.addTransform(juce::AffineTransform::scale(dpi));
        paintRack(background);backgroundMode_=mode_;backgroundScale_=dpi;
    }
    g.setOpacity(1.f);g.drawImage(rackBackground_,getLocalBounds().toFloat());
    stormChassis_.paintLiquid(g,getWidth(),getHeight());
}
void RootsEditor::resized(){rackBackground_={};backgroundScale_=0;layoutRack();repaint();}

void RootsEditor::paintRack(juce::Graphics& g){
    const int width=getWidth(),height=getHeight();
    g.fillAll(juce::Colour(0xffceb79f));
    juce::ColourGradient caseLight(juce::Colour(0xfff0ddc4),0,0,
                                   juce::Colour(0xffc8ae92),(float)width,(float)height,false);
    g.setGradientFill(caseLight);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f),12.0f);
    g.setColour(juce::Colour(0xfff8ebd7).withAlpha(.8f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(3.f),10.f,2.f);
    g.setColour(juce::Colour(0xff675446).withAlpha(.7f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(7.f),8.f,1.f);
    // Four black, recessed Phillips screws. Pure geometry avoids visual
    // drift; no extra screws or tick marks appear on the instrument face.
    auto screw=[&](float cx,float cy){
        g.setColour(juce::Colour(0xff806951));g.fillEllipse(cx-12,cy-12,24,24);
        g.setColour(juce::Colour(0xff17191a));g.fillEllipse(cx-10,cy-10,20,20);
        g.setColour(juce::Colour(0xff5a5b59));g.drawEllipse(cx-9,cy-9,18,18,.75f);
        g.setColour(juce::Colour(0xff040608));
        g.drawLine(cx-5,cy,cx+5,cy,3.1f);g.drawLine(cx,cy-5,cx,cy+5,3.1f);
        g.setColour(juce::Colour(0xff80827e).withAlpha(.3f));
        g.drawLine(cx-5,cy-1,cx+5,cy-1,.7f);
    };
    screw(25,25);screw((float)width-25,25);
    screw(25,(float)height-25);screw((float)width-25,(float)height-25);
    juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(surgery::rack::contentTransform(width,height));
    auto inset=[&](juce::Rectangle<float> r,bool dark){
        g.setColour(juce::Colour(0xff6e5542).withAlpha(.57f));g.fillRoundedRectangle(r,4.f);
        g.setColour(juce::Colour(dark?0xff131516:0xffe4d0b7));g.fillRoundedRectangle(r.reduced(2.f),3.f);
        g.setColour(juce::Colour(dark?0xff575d5e:0xfffdf0db).withAlpha(.8f));
        g.drawRoundedRectangle(r.reduced(2.f),3.f,.9f);
    };
    inset({12,7,1096,44},false);
    inset({12,55,1096,39},false);
    inset({12,96,1096,429},true);
    inset({12,525,1096,38},false);
    inset({12,570,1096,151},false);
    inset({12,724,1096,29},false);
    g.setColour(juce::Colour(0xff403026));
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawText("SPIRAL ETHER TECH  /  PRECISION AUDIO",770,17,315,23,juce::Justification::centredRight);
    // COLOR panel is deliberately free of decorative hash/notch marks.
    if(mode_==1)inset({928,598,161,104},false);
}

void RootsEditor::layoutRack(){
    // Fixed design coordinates, uniformly scaled. JUCE inversely transforms
    // pointer/drag events, so graphics, hit targets and text remain aligned.
    const auto transform=surgery::rack::contentTransform(getWidth(),getHeight());
    for(auto* child:getChildren()){
        // JUCE owns this host-sized resize handle; transforming it with the
        // instrument panel made it float over controls after a window resize.
        if(dynamic_cast<juce::ResizableCornerComponent*>(child)!=nullptr)child->setTransform({});
        else child->setTransform(transform);
    }
    for(auto* label:{&gainLabel_,&thresholdLabel_,&carveFrequencyLabel_,&carveDepthLabel_,&carveQLabel_,&analyzerDepthLabel_,&analyzerSlopeLabel_,&analyzerMemoryLabel_})
        label->setJustificationType(juce::Justification::centred);
    title_.setBounds(29,12,415,35);
    for(int root=0;root<7;++root)rootButtons_[(size_t)root].setBounds(18+root*155,59,151,30);
    graph_.setBounds(12,98,1096,421);
    for(int i=0;i<4;++i)modeButtons_[(size_t)i].setBounds(18+i*273,529,267,30);
    selection_.setBounds(28,573,220,23);
    status_.setBounds(440,703,650,16);
    on_.setBounds(28,610,146,28);solo_.setBounds(28,646,146,28);
    gainLabel_.setBounds(186,588,142,18);gain_.setBounds(186,607,142,94);
    thresholdLabel_.setBounds(340,588,142,18);threshold_.setBounds(340,607,142,94);
    const std::array<juce::ComboBox*,3> boxes{&compressor_,&domain_,&detector_};
    const std::array<juce::Label*,3> labels{&compLabel_,&domainLabel_,&detectorLabel_};
    for(int i=0;i<3;++i){labels[(size_t)i]->setBounds(496+i*150,610,146,18);boxes[(size_t)i]->setBounds(496+i*150,633,144,34);}
    phase_.setBounds(938,601,71,25);limit_.setBounds(1015,601,73,25);
    mono_.setBounds(944,637,142,24);
    limitAmount_.setBounds(942,660,147,44);
    carveOn_.setBounds(28,610,155,28);carveDynamic_.setBounds(28,646,155,28);
    const std::array<juce::Slider*,3> cs{&carveFrequency_,&carveDepth_,&carveQ_};
    const std::array<juce::Label*,3> cl{&carveFrequencyLabel_,&carveDepthLabel_,&carveQLabel_};
    for(int i=0;i<3;++i){cl[(size_t)i]->setBounds(211+i*235,588,202,18);cs[(size_t)i]->setBounds(211+i*235,607,202,94);}
    for(int i=0;i<4;++i)carveSlotButtons_[(size_t)i].setBounds(938+(i%2)*72,612+(i/2)*42,66,30);
    for(int i=0;i<5;++i){onyxLabels_[(size_t)i].setBounds(34+i*217,588,210,18);onyxSliders_[(size_t)i].setBounds(34+i*217,607,210,94);}
    xrayButton_.setBounds(981,109,103,33);
    analyzerLegend_.setBounds(28,606,239,20);
    jukeModeLabel_.setBounds(285,574,175,14);jukeMode_.setBounds(285,590,175,22);
    weatherMode_.setBounds(28,631,228,32);reducedMotion_.setBounds(28,673,228,26);
    analyzerDepthLabel_.setBounds(285,612,175,18);analyzerDepth_.setBounds(285,638,175,34);
    analyzerSlopeLabel_.setBounds(492,588,190,18);analyzerSlope_.setBounds(492,607,190,94);
    analyzerMemoryLabel_.setBounds(697,588,190,18);analyzerMemory_.setBounds(697,607,190,94);
    heatToggle_.setBounds(935,611,156,30);freezeButton_.setBounds(935,652,156,30);
    bypass_.setBounds(20,727,78,23);compare_.setBounds(103,727,78,23);copy_.setBounds(186,727,110,23);
    save_.setBounds(301,727,57,23);load_.setBounds(363,727,57,23);peakHold_.setBounds(425,727,120,23);
    outputLabel_.setBounds(780,727,64,23);output_.setBounds(849,727,246,23);
}

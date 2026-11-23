#include "RootsEditor.h"
#include <optional>
#include "RootsSpectrumColours.h"
#include "ScalpelXRayChroma.h"
#include "ScalpelUI105.h"
#include "ScalpelDbAxis.h"
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
    if(auto* param=p.apvts.getParameter(id)){
        const auto normalized=param->convertTo0to1(val);
        if(param->getValue()!=normalized)
            param->setValueNotifyingHost(normalized);
    }
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
        0xFFE05246u,0xFFD99045u,0xFF6EAA78u,0xFF55BFCBu,
        0xFF425D9Bu,0xFF9A63B8u,0xFFD97A9Cu
    };
    return juce::Colour(hues[(size_t)juce::jlimit(0,6,i)]);
}
void rotary(juce::Slider& s,juce::String suffix,double resetValue) {
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,106,30);
    s.setColour(juce::Slider::textBoxTextColourId,scalpel::finish::ice);
    s.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    s.setTextValueSuffix(suffix);
    s.setDoubleClickReturnValue(true,resetValue);
}
}
RootsGraph::RootsGraph(PRISMVSTAudioProcessor& owner):processor_(owner) {
    processor_.beginWeatherConsumer();
    bands_.fill(-144.0f);peaks_.fill(-144.0f);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    bindCarveRepaints();
    peakReadouts_.fill(-144.f);
}
RootsGraph::~RootsGraph(){
    for(auto& attachment:carveRepaintAttachments_)attachment.reset();
    processor_.endWeatherConsumer();
    endBandGesture();
    if(draggingCarve_>=0)
        for(const char* suffix:{"hz","gain_db"})
            if(auto* parameter=processor_.apvts.getParameter(PRISMVSTAudioProcessor::carveId(selected_/2,draggingCarve_,suffix)))
                parameter->endChangeGesture();
}
juce::Rectangle<float> RootsGraph::area() const {
    // 1.0.5 reserves a true utility bay for PEAK and independent L/R meters.
    // The spectrum remains logarithmic and interactive inside the left lens.
    const auto b=getLocalBounds().toFloat();
    (void)b;
    return scalpel::reference::plotLocal();
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
juce::Point<float> RootsGraph::boundaryPoint(int node,const scalpel::bands::Model& model) const {
    const auto graph=area();
    using scalpel::bands::Anchor;
    const int root=node/2;
    const auto anchor=node%2==0?Anchor::left:Anchor::right;
    const float x=freqToX((float)model.frequency(root,anchor));
    return {juce::jlimit(graph.getX()+10.f,graph.getRight()-10.f,x),graph.getCentreY()};
}
int RootsGraph::hitNode(juce::Point<float> p,bool cycleCoincidentBand)const{
    float nearest=18.0f;
    const auto model=loadBandModel();
    std::array<float,prism::roots::v2::kNodes> distances{};
    for(int i=0;i<prism::roots::v2::kNodes;++i){
        if(!model.bands[(size_t)(i/2)].present){
            distances[(size_t)i]=100000.f;continue;
        }
        distances[(size_t)i]=p.getDistanceFrom(boundaryPoint(i,model));
        nearest=juce::jmin(nearest,distances[(size_t)i]);
    }
    if(nearest>=18.0f)return -1;
    std::array<bool,prism::roots::v2::kNodes> candidates{};
    for(int i=0;i<prism::roots::v2::kNodes;++i)
        candidates[(size_t)i]=distances[(size_t)i]<=nearest+1.25f;
    return surgery::ui::chooseBoundaryNode(candidates,selected_,
                                            cycleCoincidentBand);
}
scalpel::bands::Model RootsGraph::loadBandModel() const {
    return processor_.magneticBandModel();
}
void RootsGraph::storeBandModel(const scalpel::bands::Model& model){
    if(!model.valid())return;
    if(dragging_>=0||draggingInterior_>=0){
        for(int node=0;node<prism::roots::v2::kNodes;++node){
            const auto anchor=node%2==0?scalpel::bands::Anchor::left:scalpel::bands::Anchor::right;
            if(bandGestures_[(size_t)node]
                ||(float)model.frequency(node/2,anchor)==param(node,"hz"))continue;
            if(auto* parameter=processor_.apvts.getParameter(PRISMVSTAudioProcessor::rootsNodeId(node,"hz"))){
                parameter->beginChangeGesture();
                bandGestures_[(size_t)node]=true;
            }
        }
    }
    processor_.setMagneticBandModel(model);
}
void RootsGraph::endBandGesture(){
    for(int node=0;node<prism::roots::v2::kNodes;++node){
        if(!bandGestures_[(size_t)node])continue;
        if(auto* parameter=processor_.apvts.getParameter(PRISMVSTAudioProcessor::rootsNodeId(node,"hz")))
            parameter->endChangeGesture();
        bandGestures_[(size_t)node]=false;
    }
}
int RootsGraph::hitBandInterior(juce::Point<float> point) const {
    const auto graph=area();
    if(!graph.contains(point))return -1;
    const auto model=loadBandModel();
    const double hz=xToFreq(point.x);
    const int focused=selected_/2;
    const auto contains=[&](int root){
        const auto& band=model.bands[(size_t)root];
        return band.present&&hz>band.left&&hz<band.right;
    };
    if(contains(focused))return focused;
    for(int root=0;root<7;++root)if(contains(root))return root;
    return -1;
}
double RootsGraph::snappedFrequency(double frequency,bool disableSnap,
                                    int movingBand,
                                    scalpel::bands::Anchor movingAnchor){
    if(disableSnap){snapTargetHz_=0.0;return frequency;}
    static constexpr double grid[]{20,40,80,160,315,630,1250,2500,5000,10000,20000};
    const auto model=loadBandModel();
    const auto seam=model.touchingNeighbor(movingBand,movingAnchor);
    const auto pixelDistance=[this,frequency](double candidate){
        return std::abs(freqToX((float)candidate)-freqToX((float)frequency));
    };
    if(seam.band<0&&snapTargetHz_>0.0&&pixelDistance(snapTargetHz_)<14.f){
        auto trial=model;
        if(trial.moveSeam(movingBand,movingAnchor,snapTargetHz_))
            return snapTargetHz_;
    }
    snapTargetHz_=0.0;
    double nearest=frequency;
    float distance=8.f;
    const auto consider=[&](double candidate){
        const float pixels=pixelDistance(candidate);
        if(pixels>=distance)return;
        auto trial=model;
        if(!trial.moveSeam(movingBand,movingAnchor,candidate))return;
        distance=pixels;
        nearest=candidate;
        snapTargetHz_=candidate;
    };
    if(seam.band<0){
        const auto opposite=movingAnchor==scalpel::bands::Anchor::left?
            scalpel::bands::Anchor::right:scalpel::bands::Anchor::left;
        for(int root=0;root<scalpel::bands::count;++root){
            if(root==movingBand||!model.bands[(size_t)root].present
                ||model.bands[(size_t)root].domain!=model.bands[(size_t)movingBand].domain
                ||model.link(root,opposite).band>=0)continue;
            consider(model.frequency(root,opposite));
        }
    }
    for(const double value:grid)consider(value);
    return nearest;
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
    if(!frozen_&&displayDomain_!=processor_.getAnalyzerDomain()){
        displayDomain_=processor_.getAnalyzerDomain();
        spectrumEnvelope_.reset();
        repaint();
    }
    if(frozen_)return;
    bool peakChanged=false;
    for(int root=0;root<surgery::kRoots;++root){
        const auto peak=juce::roundToInt(processor_.getHeldRootBandPeakDb(root)*10.0f)*0.1f;
        auto& shown=peakReadouts_[(size_t)root];
        peakChanged|=shown!=peak;
        shown=peak;
    }

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
    auto dynamics=processor_.getBandDynamics(selected_/2);
    for(auto& value:dynamics)value=std::round(10.f*value)*0.1f;
    const bool dynamicsChanged=dynamics!=dynamicsReadout_;
    dynamicsReadout_=dynamics;
    if(previous!=bands_||previousPeaks!=peaks_||peakChanged||dynamicsChanged)repaint();
}
void RootsGraph::updateWeather(const surgery::weather::Snapshot* snapshot,double elapsed,bool active,bool reduced,double memory,int style){
    style=style>0?7:0; // All retained legacy values resolve to Standard+.
    if(xrayActive_)mercuryElapsed_=elapsed;
    const bool visible=active&&style>0;
    const bool changed=visible!=weatherActive_||style!=weatherStyle_;
    weatherActive_=visible;
    // Off gates visibility immediately. Freeze keeps simulation and mode held.
    if(!visible&&changed)repaint();
    if(frozen_)return;
    if(changed){heatValid_=false;weatherStyle_=style;weatherField_.clear();repaint();}
    if(!active){
        weatherInterpretation_.update(elapsed,memory,true);
        // X-RAY is independent of SONAR, even on sustained steady notes.
        if(xrayActive_){
            mercurySurface_.advance(bands_,elapsed,reduced);
            repaint();
        }
        return;
    }
    if(snapshot)weatherInterpretation_.accept(*snapshot);
    auto key=[](const surgery::weather::Meaning& f){return std::array<float,16>{f.bass,f.centroid,f.flux,f.width,f.rate,f.density,f.level,f.storm,f.flash,f.flashPosition,f.musicEnergy[0],f.musicEnergy[1],f.musicEnergy[2],f.musicEnergy[3],f.rms,f.crest};};
    const auto previous=key(weatherInterpretation_.state());const auto previousPhase=visualPhase_;
    const auto& meaning=weatherInterpretation_.update(elapsed,memory,reduced);
    if(xrayActive_)mercurySurface_.advance(bands_,elapsed,reduced);
    if(xrayActive_&&meaning.level>0.01f)
        mercuryPhase_=std::fmod(mercuryPhase_+(float)(std::clamp(elapsed,0.,.25)*
            (.24f+1.1f*meaning.level+1.8f*meaning.flux)*(reduced?.5f:1.f)),64.f);
    if(visible||xrayActive_)
        visualPhase_=std::fmod(visualPhase_+(float)(std::clamp(elapsed,0.,.25)*
            meaning.level*(reduced?.035:1.)),64.f);
    if(!visible){
        // X-RAY is independent of SONAR mode. Keep its mercury simulation
        // updating from measured music even with SONAR presentation OFF.
        if(xrayActive_&&(changed||previous!=key(meaning)||previousPhase!=visualPhase_))repaint();
        return;
    }
    const bool fieldChanged=style==1&&weatherField_.update(weatherInterpretation_,elapsed,memory,reduced,false);
    if(!changed&&!fieldChanged&&previous==key(meaning)&&previousPhase==visualPhase_)return;
    {
        juce::Image::BitmapData pixels(weatherImage_,juce::Image::BitmapData::writeOnly);
        const auto& field=weatherField_.pixels();
        for(int y=0;y<64;++y)for(int x=0;x<128;++x){const auto c=surgery::visual::style(style,meaning,visualPhase_,x/127.f,y/63.f,field[(size_t)(y*128+x)]);pixels.setPixelColour(x,y,juce::Colour(c.r,c.g,c.b));}
    }
    repaint();
}
void RootsGraph::bindCarveRepaints(){
    for(auto& attachment:carveRepaintAttachments_)attachment.reset();
    size_t index=0;
    const auto observe=[this,&index](const juce::String& id){
        if(auto* parameter=processor_.apvts.getParameter(id))
            carveRepaintAttachments_[index++]=std::make_unique<juce::ParameterAttachment>(
                *parameter,[this](float){repaint();},nullptr);
    };
    const int root=selected_/2;
    for(int slot=0;slot<surgery::kSlots;++slot)
        for(const char* suffix:carveRepaintFields_)
            observe(PRISMVSTAudioProcessor::carveId(root,slot,suffix));
    observe(PRISMVSTAudioProcessor::rootsNodeId(root*2,"hz"));
    observe(PRISMVSTAudioProcessor::rootsNodeId(root*2+1,"hz"));
    observe(PRISMVSTAudioProcessor::rootsRootGainId(root));
    for(const auto* field:{"on","present","domain","dynamics_mode","comp_offset"})
        observe(PRISMVSTAudioProcessor::rootsRootControlId(root,field));
}
void RootsGraph::selectNode(int i){
    const int target=juce::jlimit(0,prism::roots::v2::kNodes-1,i);
    if(target==selected_)return;
    const bool changedRoot=selected_/2!=target/2;
    selected_=target;
    if(changedRoot){
        bindCarveRepaints();
        dynamicsReadout_=processor_.getBandDynamics(selected_/2);
    }
    // Once solo audition is engaged it travels with the selected root.
    processor_.transferRootsSoloTo(selected_);
    if(onSelectNode)onSelectNode(selected_);
    repaint();
}
void RootsGraph::showRootMenu(int root,juce::Component& button){
    selectNode(juce::jlimit(0,6,root)*2);
    popup(&button);
}
void RootsGraph::paintStaticFrame(juce::Graphics& g) const {
    const auto full=getLocalBounds().toFloat(),r=area();
    scalpel::finish::analyzerHousing(g,full);
    g.setFont(scalpel::finish::font(14,false));
    g.setColour(scalpel::finish::text);
    // No inherited cyan spectrum title, old SONAR weather image or C1-C4
    // badges: the graph is a continuous, smoked-glass Scalpel lens.
    // Frequency and calibrated dBFS dimensions.
    static constexpr std::array<float,11> freqs{
        20,40,80,160,315,630,1250,2500,5000,10000,20000
    };
    for(const float f:freqs){
        const float x=freqToX(f);
        g.setFont(scalpel::finish::font(13.5f,false));
        g.setColour(juce::Colour(0xff707b86).withAlpha(.20f));
        g.drawVerticalLine(juce::roundToInt(x),r.getY(),r.getBottom());
        g.setColour(scalpel::finish::text);
        g.drawText(hzText(f),juce::roundToInt(x-25),juce::roundToInt(r.getBottom()+5),
                   50,20,juce::Justification::centred);
    }
    for(const int db:scalpel::dbaxis::labels){
        const float y=scalpel::dbaxis::toY((float)db,r.getY(),r.getHeight());
        g.setColour(juce::Colour(0xff78818b).withAlpha(db==-30?.34f:.19f));
        g.drawHorizontalLine(juce::roundToInt(y),r.getX(),r.getRight());
    }
    scalpel::dbaxis::draw(g,r);
}
void RootsGraph::paint(juce::Graphics& g){
    ++paintedFrames_;
    const auto full=getLocalBounds().toFloat();
    const auto r=area();

    const bool polished=scalpel::finish::enabledForMode(mode_);
    const auto transform=getTransform();
    const float controlScale=std::sqrt(transform.mat00*transform.mat00+transform.mat10*transform.mat10);
    const float frameScale=juce::jlimit(.5f,6.f,controlScale*g.getInternalContext().getPhysicalPixelScaleFactor());
    const int frameWidth=juce::jmax(1,juce::roundToInt(getWidth()*frameScale));
    const int frameHeight=juce::jmax(1,juce::roundToInt(getHeight()*frameScale));
    if(staticFrame_.getWidth()!=frameWidth||staticFrame_.getHeight()!=frameHeight||staticFrameScale_!=frameScale){
        staticFrame_=juce::SoftwareImageType{}.convert(juce::Image(juce::Image::ARGB,frameWidth,frameHeight,true));
        juce::Graphics frame(staticFrame_);frame.addTransform(juce::AffineTransform::scale(frameScale));
        paintStaticFrame(frame);staticFrameScale_=frameScale;
    }
    g.setOpacity(1);g.drawImage(staticFrame_,full);
    const auto bandModel=loadBandModel();
    for(int root=0;root<scalpel::bands::count;++root) {
        const auto& band=bandModel.bands[(size_t)root];
        if(!band.present||pval(processor_.apvts,
            PRISMVSTAudioProcessor::rootsRootControlId(root,"on"))<0.5f)continue;
        const float x1=freqToX((float)band.left);
        const float x2=freqToX((float)band.right);
        const auto warning=peakWarnings_[(size_t)root];
        const auto accent=warning>0.f?juce::Colour(0xffd71916):
            (root==selected_/2?colour(root):juce::Colour(0xff85909b));
        g.setColour(accent.withAlpha(warning>0.f?warning*.33f:
            root==selected_/2?.095f:.017f));
        g.fillRect(x1,r.getY(),juce::jmax(0.0f,x2-x1),r.getHeight());
    }
    // Single live FFT envelope. X is 20-20k logarithmic Hz; Y is measured dBFS.
    // The area beneath the one curve is closed at the -rangeDb graph floor.
    // Display slope or spectral colouring never changes the audio path.
    // Measurement still extends below -144 dBFS. X-RAY uses the locked
    // 0..-60 dBFS presentation scale without modifying analyzer precision.
    const float rangeDb=scalpel::dbaxis::rangeDb;
    constexpr float slope=0.f; // The dBFS ruler reports the measured spectrum without hidden tilt.
    const bool heatOn=pval(processor_.apvts,"aura_color")>=0.5f&&(weatherStyle_<=2||weatherStyle_==7);
    juce::Path spectrumPath,filledPath,peakPath;
    const int N=PRISMVSTAudioProcessor::labBars;
    for(int i=0;i<N;++i){
        const float frequency=prism::analysis::LabBands::centreFrequency(i);
        const float x=freqToX(frequency);
        const float db=surgery::displayDb(bands_[(size_t)i],frequency,slope,rangeDb);
        const float y=scalpel::dbaxis::toY(db,r.getY(),r.getHeight());
        const float peak=surgery::displayDb(peaks_[(size_t)i],frequency,slope,rangeDb);
        const float py=scalpel::dbaxis::toY(peak,r.getY(),r.getHeight());
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
        // Fixed log-frequency columns beneath the measured display contour.
        // Each column is fixed to a logarithmic Hz address: NO time scroll,
        // circular icons, waterfall, simulated energy, or animated x motion.
        // Intensity uses the calibrated FFT dBFS BEFORE presentation tilt.
        juce::Image::BitmapData heatPixels(spectralHeat_,
            juce::Image::BitmapData::writeOnly);
        const int width=spectralHeat_.getWidth();
        const int height=spectralHeat_.getHeight();
        for(int x=0;x<width;++x){
            const float u=(x+.5f)/(float)width;
            const float hz=(float)prism::roots::visual::heatmapFrequencyAt(x,width);
            const int bin=prism::analysis::LabBands::indexForFrequency(hz);
            const float contour=surgery::displayDb(bands_[(size_t)bin],hz,slope,rangeDb);
            const auto tone=surgery::synesthesia::palette(u);
            for(int y=0;y<height;++y){
                const float v=(y+.5f)/(float)height;
                const float alpha=prism::roots::visual::underCurveAlpha(
                    bands_[(size_t)bin],contour,v);
                heatPixels.setPixelColour(x,y,juce::Colour::fromFloatRGBA(
                    tone.r,tone.g,tone.b,alpha));
            }
        }
    }
    if(!xrayActive_){
        // Standard Scalpel graph is smoked graphite, never Surgery's bright
        // cyan/blue filled histogram. HEAT MAP adds frequency-locked spectral colour.
        juce::ColourGradient graphite(juce::Colour(0xff282d33).withAlpha(.64f),
            r.getX(),r.getY(),juce::Colour(0xff0a0c0f).withAlpha(.9f),
            r.getX(),r.getBottom(),false);
        g.setGradientFill(graphite);g.fillPath(filledPath);
    } else {
        // Floor 01 stability baseline: the experimental D3D11 volume is
        // deliberately disabled. It ran GPU dispatch/readback during paint,
        // and Windows hosts reported an unload crash. The CPU presentation
        // remains available; never initialize a GPU device on this path.
        constexpr bool experimentalGpuEnabled = false;
        // The clipping path prevents any decorative peaks outside the data.
        juce::Graphics::ScopedSaveState mercury(g);
        // The cinematic volume is decorative; its visible material must
        // remain beneath the authoritative FFT silhouette.
        const auto& m=weatherInterpretation_.state();
        const float energy=juce::jlimit(0.f,1.f,
            .30f*m.level+.35f*m.bass+.20f*m.flux+.15f*m.width);
        // A charcoal base remains legible even when the measured crest is
        // far below 0 dBFS. Previously the brightest stop lived above the
        // audio silhouette, making the real Windows X-RAY render look flat.
        juce::ColourGradient tar(juce::Colour(polished?0xff030d19:0xff101619).withAlpha(.96f),
            r.getCentreX(),r.getY(),juce::Colour(polished?0xff01060d:0xff020304).withAlpha(.99f),
            r.getCentreX(),r.getBottom(),false);
        tar.addColour(.43,juce::Colour(polished?0xff050f1a:0xff0b1418));
        tar.addColour(.78,juce::Colour(polished?0xff020a13:0xff081018));
        g.setGradientFill(tar);g.fillPath(filledPath);
        // Material sheen is a reflection field, not an independent waveform.
        // Its apparent ridges are clipped to the actual FFT silhouette.
        juce::ColourGradient oblique((polished?scalpel::finish::ice:juce::Colour(0xffb2c8c5)).withAlpha(polished?.018f:.075f),
            r.getX(),r.getBottom(),(polished?scalpel::finish::silver:juce::Colour(0xffba916f)).withAlpha(polished?.020f:.10f),
            r.getRight(),r.getY(),false);
        oblique.addColour(.38,juce::Colour(polished?0xff10151a:0xff282026).withAlpha(.016f));
        oblique.addColour(.73,juce::Colour(polished?0xffa6eeff:0xffb9a5c2).withAlpha(polished?.012f:.048f));
        g.setGradientFill(oblique);g.fillPath(filledPath);
        // Approximate an oily normal/thin-film field at 30 Hz; the 60 Hz
        // surface curves below interpolate the measured FFT at every frame.
        // Both the material and the secondary crests stay inside filledPath.
        // A frozen frame must render from the exact same surface/material
        // state every time. Never show a cached pre-freeze frame on one
        // capture and a newly shaded frame on the next paint.
        // Shade each active 60 Hz visual frame, not alternate paints (~30 Hz).
        // FREEZE shades once after toggling, then reuses the exact texture.
        const bool needsNewFrame=!frozen_||mercuryPaintCount_++==0;
        if(needsNewFrame){
            std::array<scalpel::xray::FluidColumn,scalpel::xray::FluidVolume3D::gridWidth> source{};
            const auto pitch=scalpel::xray::chroma::estimate(bands_);
            float measuredLevel=0.f,measuredBass=0.f;
            for(int column=0;column<(int)source.size();++column){
                const float u=(column+.5f)/(float)source.size();
                const int bin=juce::jlimit(0,N-1,juce::roundToInt(u*(N-1)));
                const auto syn=surgery::synesthesia::read(m,u);
                const float local=juce::jlimit(0.f,1.f,(bands_[(size_t)bin]+72.f)/65.f);
                measuredLevel=juce::jmax(measuredLevel,local);
                if(column<22)measuredBass=juce::jmax(measuredBass,local);
                // Musical pitch-class estimates transport coloured liquid
                // through the volume. Confidence-gating keeps noise from
                // inventing specific notes; measured spectrum stays intact.
                const double f=prism::analysis::LabBands::centreFrequency(bin);
                const int note=juce::roundToInt(69.+12.*std::log2(f/440.));
                const int pitchClass=(note%12+12)%12;
                const float tint=.66f*pitch.confidence*pitch.weight[(size_t)pitchClass];
                const auto musical=scalpel::xray::chroma::palette[(size_t)pitchClass];
                source[(size_t)column]={local,
                    syn.r*(1.f-tint)+musical.r*tint,
                    syn.g*(1.f-tint)+musical.g*tint,
                    syn.b*(1.f-tint)+musical.b*tint};
            }
            // GPU velocity/pressure/phase transport is restricted to this UI path.
            // The measured contour, parameter IDs and all audio code are unchanged.
            if (experimentalGpuEnabled)
                mercury3D_.render(source,juce::jmax(m.bass,measuredBass),
                                  m.flux,juce::jmax(m.level,measuredLevel*.85f),
                                  mercuryElapsed_,
                                  pval(processor_.apvts,"sonar_reduced_motion")>=.5f,
                                  mercury3DImage_);
        }
        if(experimentalGpuEnabled && mercury3D_.hasFrame()){
            juce::Graphics::ScopedSaveState measuredClip(g);
            g.reduceClipRegion(filledPath);
            g.setOpacity(.88f);g.drawImage(mercury3DImage_,r);g.setOpacity(1.f);
        }else{
        // Restrict the old CPU fallback, not the actual 3D volume.
        juce::Graphics::ScopedSaveState fallbackClip(g);
        g.reduceClipRegion(filledPath);
        if(needsNewFrame){
            juce::Image::BitmapData pixels(mercuryMaterial_,juce::Image::BitmapData::writeOnly);
            const float time=mercuryPhase_;
            for(int px=0;px<mercuryMaterial_.getWidth();++px){
                const float u=(px+.5f)/(float)mercuryMaterial_.getWidth();
                const int bin=juce::jlimit(0,N-1,juce::roundToInt(u*(N-1)));
                const float spectrumDb=surgery::displayDb(bands_[(size_t)bin],
                    prism::analysis::LabBands::centreFrequency(bin),slope,rangeDb);
                const float surfaceTop=juce::jlimit(0.f,1.f,-spectrumDb/rangeDb);
                const float local=juce::jlimit(0.f,1.f,(bands_[(size_t)bin]+72.f)/65.f);
                const float fluid=mercurySurface_.at(u);
                const float surfaceSlope=mercurySurface_.at(juce::jmin(1.f,u+.008f))-
                    mercurySurface_.at(juce::jmax(0.f,u-.008f));
                const auto syn=surgery::synesthesia::read(m,u);
                for(int py=0;py<mercuryMaterial_.getHeight();++py){
                    const float v=(py+.5f)/(float)mercuryMaterial_.getHeight();
                    // The true decibel contour gates the entire liquid. The
                    // shading normals never create peaks in the analyzer.
                    if(local<.005f||v<surfaceTop){pixels.setPixelColour(px,py,juce::Colour(0xff020304));continue;}
                    const float d=juce::jlimit(0.f,1.f,(v-surfaceTop)/juce::jmax(.015f,1.f-surfaceTop));
                    const float bend=std::sin(18.f*u-2.1f*time+3.8f*d+fluid*9.f);
                    const float fold=std::sin(40.f*u+12.f*d-.9f*time+1.2f*bend);
                    const float wave=std::sin(13.f*u+8.6f*d+1.7f*time+2.1f*fold);
                    const float nx=.52f*std::cos(18.f*u+3.8f*d-2.1f*time)+
                                   .31f*std::cos(40.f*u+12.f*d-.9f*time)+surfaceSlope*4.f;
                    const float ny=.45f*std::cos(13.f*u+8.6f*d+1.7f*time)+.16f*wave;
                    const float highlight=juce::jlimit(0.f,1.f,.48f+.47f*nx-.26f*ny);
                    // Exact integer-power specular lobe without a costly per-pixel pow().
                    const float h2=highlight*highlight, h4=h2*h2, h8=h4*h4, h16=h8*h8;
                    const float spec=h16*h8;
                    // Rounded menisci at irregular fluid depths. These
                    // streaks move with the spectrum-driven liquid surface.
                    const float ribbon=.12f+fluid*.25f+.07f*std::sin(15.f*u-time+2.5f*wave);
                    const float meniscusDistance=(d-ribbon)/.035f;
                    const float meniscus=std::exp(-meniscusDistance*meniscusDistance);
                    const float lower=.53f+.07f*std::sin(12.f*u+time*.67f+wave);
                    const float secondDistance=(d-lower)/.047f;
                    const float second=std::exp(-secondDistance*secondDistance);
                    const float crest=std::exp(-d*19.f);
                    const float gloss=(.52f*spec+.26f*meniscus+.13f*second+.22f*crest)*
                        juce::jlimit(0.f,1.f,.24f+.61f*local+.15f*energy);
                    const float foil=.5f+.5f*std::sin(22.f*d+35.f*u+3.1f*wave-time*.45f);
                    const float iridescence=.13f+.31f*foil;
                    const float ambient=.009f+.018f*juce::jlimit(0.f,1.f,.5f+.5f*fold);
                    const float body=.070f*local*(.5f+.5f*fold);
                    const float red=juce::jlimit(0.f,1.f,ambient+gloss*(.17f+1.40f*iridescence*syn.r)+body*syn.r);
                    const float green=juce::jlimit(0.f,1.f,ambient+gloss*(.18f+1.40f*iridescence*syn.g)+body*syn.g);
                    const float bCol=juce::jlimit(0.f,1.f,ambient*1.13f+gloss*(.21f+1.48f*iridescence*syn.b)+body*syn.b);
                    pixels.setPixelColour(px,py,juce::Colour::fromFloatRGBA(red,green,bCol,1.f));
                }
            }
        }
        g.setOpacity(.94f);
        g.drawImage(mercuryMaterial_,r);
        g.setOpacity(1.f);
        } // CPU fallback when D3D11 compute is unavailable
        // Layered capillary crests follow the real FFT surface and settle
        // when the music stops; unlike rigid colored lines they bend and
        // uncoil with the damped liquid surface.
        if(!experimentalGpuEnabled || !mercury3D_.hasFrame()){
        for(int layer=0;layer<3;++layer){
            juce::Path ribbon;
            bool open=false;
            for(int i=0;i<192;++i){
                const float u=(i+.5f)/192.f;
                const int bin=juce::jlimit(0,N-1,juce::roundToInt(u*(N-1)));
                const float db=surgery::displayDb(bands_[(size_t)bin],
                    prism::analysis::LabBands::centreFrequency(bin),slope,rangeDb);
                const float h=juce::jlimit(0.f,1.f,(bands_[(size_t)bin]+60.f)/56.f);
                const float top=scalpel::dbaxis::toY(db,r.getY(),r.getHeight());
                if(h<.035f||top>=r.getBottom()-3.f){open=false;continue;}
                const float uWave=mercurySurface_.at(u);
                const float oscillation=std::sin((12.f+layer*4.1f)*u-mercuryPhase_*(.66f+layer*.16f)+uWave*9.f);
                const float fraction=juce::jlimit(.05f,.87f,.12f+.19f*layer+.065f*oscillation+uWave*.19f);
                const float x=r.getX()+u*r.getWidth();
                const float y=top+(r.getBottom()-top)*fraction;
                if(!open){ribbon.startNewSubPath(x,y);open=true;}else ribbon.lineTo(x,y);
            }
            const auto glaze=juce::Colour(layer==0?0xffc4c5bc:layer==1?0xff5d9f9f:0xffa3799d);
            g.setColour(glaze.withAlpha((.36f-.07f*layer)*juce::jlimit(0.f,1.f,.35f+.65f*energy)));
            g.strokePath(ribbon,juce::PathStrokeType(layer==0?1.75f:1.05f,
                juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }
        // No dotted bubble trail in CPU fallback: the original circles
        // resembled heat-map markers instead of internal liquid movement.
        } // The 3D volume already contains its own internal folds and bubbles.
    }
    // If all bins are below the visible -60 dBFS floor, the entire heat
    // layer must be invisible. Even a dark backing plate at silence would
    // manufacture a visual signal and fail the live/silence recovery proof.
    const bool heatHasSignal=std::any_of(bands_.begin(),bands_.end(),
        [](float db){return prism::roots::visual::heatmapStrength(db)>0.f;});
    if(heatOn&&heatHasSignal){
        // Full-height transparent image: each pixel is gated by measured
        // energy and by its own column's visible FFT contour.
        juce::Graphics::ScopedSaveState heatLayer(g);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.setOpacity(xrayActive_?.92f:.85f);
        g.drawImage(spectralHeat_,r);
        g.setOpacity(1.f);
    }
    // Two restrained, stationary glow layers follow ONLY the measured path.
    // Existing attack/release supplies persistence; no clock-driven shimmer,
    // independent objects or fabricated motion is introduced during silence.
    const float luminous=surgery::spectrum::brightness(*std::max_element(bands_.begin(),bands_.end()));
    if(luminous>0){
        // The optical emission is generated from the exact measured contour,
        // not a second decorative waveform. Floor bins therefore emit zero.
        juce::ColourGradient emission(juce::Colour(0xff9cafc2).withAlpha(.24f*luminous),
            r.getCentreX(),r.getY(),juce::Colour(0xff222c38).withAlpha(.035f*luminous),
            r.getCentreX(),r.getBottom(),false);
        g.setGradientFill(emission);g.fillPath(filledPath);
        g.setColour(juce::Colour(0xff9cafc2).withAlpha(.18f*luminous));
        g.strokePath(spectrumPath,juce::PathStrokeType(5.2f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xffe6edf4).withAlpha(.34f*luminous));
        g.strokePath(spectrumPath,juce::PathStrokeType(2.1f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
    // A single fine readout follows the spectrum edge, never a 2D waterfall.
    g.setColour(juce::Colour(xrayActive_?0xffe6edf4:0xffe6edf4));
    g.strokePath(spectrumPath,juce::PathStrokeType(weatherStyle_==7?1.7f:1.3f,
        juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    // A restrained peak-hold readout; the audio measurements are unchanged.
    g.setColour(juce::Colour(0xffd0d7e0).withAlpha(0.24f));
    g.strokePath(peakPath,juce::PathStrokeType(0.8f));
    // Intentionally no outlined "heat bubbles": they obscured the true
    // contour and made HEAT MAP resemble a trail of rigid copper coins.
    g.restoreState();
    // No GPU readiness label in Floor 01: it was misleading and no GPU
    // device is initialized by the active X-RAY paint path.

    // The output FFT uses the right dBFS scale. The selected band's static
    // EQ target uses an independent signed dB scale and the actual filters.
    const int selectedRoot=selected_/2;
    const double sr=processor_.getSampleRate()>=8000.0?processor_.getSampleRate():48000.0;
    const auto& selectedBand=bandModel.bands[(size_t)selectedRoot];
    const bool selectedPresent=selectedBand.present;
    const float lowHz=(float)selectedBand.left,highHz=(float)selectedBand.right;
    const auto rid=[selectedRoot](const char* id){
        return PRISMVSTAudioProcessor::rootsRootControlId(selectedRoot,id);
    };
    const bool bandEnabled=selectedPresent&&pval(processor_.apvts,rid("on"))>=.5f;
    const double gain=pval(processor_.apvts,PRISMVSTAudioProcessor::rootsRootGainId(selectedRoot));
    const bool showResponse=mode_!=3&&selectedPresent;
    const auto responseY=[r](float db){return scalpel::dbaxis::responseY(db,r.getY(),r.getHeight());};
    const float baseY=responseY(0.f);
    const float overlayScale=juce::jlimit(.5f,4.f,g.getInternalContext().getPhysicalPixelScaleFactor());
    std::array<double,40> overlayKey{sr,(double)getWidth(),(double)getHeight(),(double)selectedRoot,
        lowHz,highHz,(double)slot_,(double)mode_,gain,(double)bandEnabled,overlayScale,(double)showResponse};
    for(int slot=0;slot<4;++slot){
        int i=12+slot*4;
        for(const auto* id:{"on","hz","q"})
            overlayKey[(size_t)i++]=pval(processor_.apvts,PRISMVSTAudioProcessor::carveId(selectedRoot,slot,id));
        overlayKey[(size_t)i]=processor_.getCarveGainDb(selectedRoot,slot);
    }
    if(showResponse&&(!responseOverlayValid_||responseOverlayKey_!=overlayKey)){
        const int width=juce::jmax(1,juce::roundToInt(getWidth()*overlayScale));
        const int height=juce::jmax(1,juce::roundToInt(getHeight()*overlayScale));
        responseOverlay_=juce::SoftwareImageType{}.convert(juce::Image(juce::Image::ARGB,width,height,true));
        const std::array<double,3> probeKey{sr,(double)getWidth(),(double)getHeight()};
        if(responseProbeKey_!=probeKey){
            for(size_t i=0;i<responseProbes_.size();++i){
                const float f=20.f*std::pow(1000.f,(float)i/2048.f);
                responseProbes_[i]={freqToX(f),std::polar(1.0,
                    -2.0*std::numbers::pi*std::clamp((double)f,10.0,sr*.49)/sr)};
            }
            responseProbeKey_=probeKey;
        }
        juce::Graphics overlay(responseOverlay_);
        overlay.addTransform(juce::AffineTransform::scale(overlayScale));
        overlay.reduceClipRegion(r.toNearestInt());
        const scalpel::bands::GainResponse bell(scalpel::bands::GainBand::fromEdges(sr,lowHz,highHz),
                                               bandEnabled?gain:0.0);
        scalpel::experimental::BandStop mute;
        const bool muted=!bandEnabled&&mute.configure(sr,lowHz,highHz);
        std::array<std::optional<surgery::PreparedCarveResponse>,4> carves;
        std::array<double,4> weights{};
        for(int slot=0;slot<4;++slot){
            const auto i=(size_t)(12+slot*4);
            if(!bandEnabled||overlayKey[i]<.5||std::abs(overlayKey[i+3])<.001)continue;
            const auto fc=std::clamp(overlayKey[i+1],(double)lowHz,(double)highHz);
            carves[(size_t)slot].emplace(sr,fc,overlayKey[i+2],overlayKey[i+3],true);
            weights[(size_t)slot]=surgery::CarveEngine::boundaryWeight(fc,lowHz,highHz);
        }
        const auto magnitudeDb=[](std::complex<double> h){
            return (float)(20.0*std::log10(std::max(1e-12,std::abs(h))));
        };
        juce::Path target,fill;
        std::array<juce::Path,4> nodes;
        fill.startNewSubPath(r.getX(),baseY);
        for(size_t i=0;i<responseProbes_.size();++i){
            const auto& probe=responseProbes_[i];
            auto response=bell.transfer(probe.z1);
            for(size_t slot=0;slot<carves.size();++slot){
                if(!carves[slot])continue;
                const auto contribution=1.0+weights[slot]*(carves[slot]->transfer(probe.z1)-1.0);
                response*=contribution;
                if(mode_==1){
                    const auto y=responseY(magnitudeDb(contribution));
                    if(i==0)nodes[slot].startNewSubPath(probe.x,y);
                    else nodes[slot].lineTo(probe.x,y);
                }
            }
            if(muted){
                const double hz=20.0*std::pow(1000.0,(double)i/2048.0);
                response=mute.response(sr,hz);
            }
            const float db=magnitudeDb(response);
            responseDb_[i]=db;
            const auto y=responseY(db);
            if(i==0)target.startNewSubPath(probe.x,y);
            else target.lineTo(probe.x,y);
            fill.lineTo(probe.x,y);
        }
        fill.lineTo(r.getRight(),baseY);
        fill.closeSubPath();
        overlay.setColour(scalpel::finish::ice.withAlpha(.10f));overlay.fillPath(fill);
        for(size_t slot=0;slot<nodes.size();++slot){
            if(nodes[slot].isEmpty())continue;
            overlay.setColour(colour(selectedRoot).brighter(.65f).withAlpha(slot==(size_t)slot_?.82f:.35f));
            overlay.strokePath(nodes[slot],juce::PathStrokeType(slot==(size_t)slot_?1.6f:1.1f));
        }
        overlay.setColour(scalpel::finish::ice.withAlpha(.94f));
        overlay.strokePath(target,juce::PathStrokeType(1.5f));
        responseOverlayKey_=overlayKey;
        responseOverlayValid_=true;
    }
    if(showResponse){g.setOpacity(1.f);g.drawImage(responseOverlay_,full);}
    const bool compActive=bandEnabled&&pval(processor_.apvts,rid("dynamics_mode"))>=2.f;
    const auto threshold=scalpel::dynamics::threshold(pval(processor_.apvts,rid("comp_offset")));
    if(mode_==0&&compActive){
        juce::Graphics::ScopedSaveState clipped(g);
        g.reduceClipRegion(r.toNearestInt());
        const float left=freqToX(lowHz),right=freqToX(highHz);
        const float thresholdY=scalpel::dbaxis::toY((float)threshold,r.getY(),r.getHeight());
        g.setColour(scalpel::finish::accent.withAlpha(.8f));
        const float dashes[]{3.f,3.f};
        g.drawDashedLine({left,thresholdY,right,thresholdY},dashes,2,1.f);
        if(dynamicsReadout_[0]>-60.f){
            const float levelY=scalpel::dbaxis::toY(std::min(0.f,dynamicsReadout_[0]),r.getY(),r.getHeight());
            g.fillEllipse((left+right)*.5f-2.f,levelY-2.f,4.f,4.f);
        }
    }
    // No engineering telemetry or legacy SONAR prose over the primary lens.
    // Detailed numbers are available through the parameter shelf and hover.
    {
        juce::Graphics::ScopedSaveState selectedGlass(g);
        g.reduceClipRegion(r.toNearestInt());
        for(int root=0;root<7;++root){
            if(!bandModel.bands[(size_t)root].present)continue;
            const float lo=freqToX((float)bandModel.bands[(size_t)root].left);
            const float hi=freqToX((float)bandModel.bands[(size_t)root].right);
            const float warning=peakWarnings_[(size_t)root];
            const bool focused=root==selectedRoot;
            const auto tint=warning>0.f?juce::Colour(0xffed2d22):colour(root);
            juce::ColourGradient selection(
                tint.withAlpha(warning>0.f?.22f:(focused?.105f:.052f)),lo,r.getY(),
                tint.withAlpha(focused?.035f:.018f),hi,r.getY(),false);
            g.setGradientFill(selection);
            g.fillRect(lo,r.getY(),hi-lo,r.getHeight());
        }
        if(selectedPresent){
            const float warning=peakWarnings_[(size_t)selectedRoot];
            const auto tint=warning>0.f?juce::Colour(0xffed2d22):colour(selectedRoot);
            g.setColour(tint.withAlpha(warning>0.f?.95f:.60f));
            g.drawVerticalLine(juce::roundToInt(freqToX(selected_%2==0?lowHz:highHz)),
                               r.getY(),r.getBottom());
        }
    }
    // Both boundary handles always live on one horizontal rail.
    g.setColour(scalpel::finish::text.withAlpha(.65f));
    g.drawHorizontalLine(juce::roundToInt(r.getCentreY()),r.getX(),r.getRight());
    // Two-pass z-order keeps the selected ROOT boundary pair above neighbors.
    for(int pass=0;pass<2;++pass)
    for(int i=0;i<prism::roots::v2::kNodes;++i){
        const int root=i/2;
        if(!bandModel.bands[(size_t)root].present)continue;
        const auto anchor=i%2?scalpel::bands::Anchor::right:
                                scalpel::bands::Anchor::left;
        const auto persistentLink=bandModel.link(root,anchor);
        const auto linked=bandModel.touchingNeighbor(root,anchor);
        const int linkedNode=linked.band<0?-1:
            linked.band*2+(linked.anchor==scalpel::bands::Anchor::right);
        if(linkedNode>=0&&i>linkedNode)continue; // one fused shared magnet
        const bool focused=root==selectedRoot||linked.band==selectedRoot;
        if(focused!=(pass==1))continue;
        const float hz=(float)bandModel.frequency(root,anchor);
        const float guideX=freqToX(hz);
        const auto magnet=boundaryPoint(i,bandModel);
        const float x=magnet.x,y=magnet.y;
        const bool enabled=pval(processor_.apvts,
            PRISMVSTAudioProcessor::rootsRootControlId(root,"on"))>=0.5f;
        const bool solo=pval(processor_.apvts,
             PRISMVSTAudioProcessor::rootsRootSoloId(root))>=0.5f;
        // Vertical guides represent frequency boundaries, not gain movements.
        const auto col=root==selectedRoot&&peakWarnings_[(size_t)root]>0.f?
            juce::Colour(0xffff4f38):
            colour(root);
        g.setColour(col.withAlpha(enabled?(focused?0.85f:0.65f):0.045f));
        g.drawVerticalLine(juce::roundToInt(guideX),r.getY(),r.getBottom());
        g.setColour(col.withAlpha(enabled?(focused?0.24f:0.045f):0.025f));
        g.fillEllipse(x-10,y-10,20,20);
        const auto disabled=juce::Colour(polished?0xff49545b:0xff6d6056);
        if(linked.band>=0){
            const auto partner=colour(linked.band);
            const auto leftColour=anchor==scalpel::bands::Anchor::right?col:partner;
            const auto rightColour=anchor==scalpel::bands::Anchor::right?partner:col;
            const auto bounds=juce::Rectangle<float>(x-6.5f,y-6.5f,13.f,13.f);
            {
                juce::Graphics::ScopedSaveState half(g);
                g.reduceClipRegion(juce::Rectangle<float>(x-7.f,y-7.f,7.f,14.f).toNearestInt());
                g.setColour(enabled?leftColour:disabled);g.fillEllipse(bounds);
            }
            {
                juce::Graphics::ScopedSaveState half(g);
                g.reduceClipRegion(juce::Rectangle<float>(x,y-7.f,7.f,14.f).toNearestInt());
                g.setColour(enabled?rightColour:disabled);g.fillEllipse(bounds);
            }
        }else{
            g.setColour(enabled?col:disabled);g.fillEllipse(x-6.5f,y-6.5f,13,13);
        }
        if(enabled||i==selected_||solo||focused){
            g.setColour(linked.band>=0?juce::Colours::white:
                (solo?juce::Colours::white:col.brighter(0.65f)));
            g.drawEllipse(x-9,y-9,18,18,i==selected_?2.0f:1.2f);
        }
        if(persistentLink.band>=0){
            g.setColour(juce::Colours::white.withAlpha(focused?.88f:.58f));
            g.fillEllipse(x-2.25f,y-2.25f,4.5f,4.5f);
        }
        // Precise ROOT selection details live in the contextual control shelf,
        // not in floating text that masks live spectrum or CARVE contours.
    }
    // Carve selection is housed in the dedicated mechanical CARVE shelf.
    const auto focusedColor=colour(selectedRoot).brighter(.25f);

    // Selected ROOT CARVE markers always render AFTER boundary handles and heat.
    // Active cuts use their actual host frequency and depth; inactive slots
    // exist only as OFF selectors in the top rail (no fabricated filter plot).
    for(int slot=0;slot<surgery::ui::kSlotsPerRoot;++slot){
        if(!selectedPresent)break;
        const auto id=[selectedRoot,slot](const char* suffix){
            return PRISMVSTAudioProcessor::carveId(selectedRoot,slot,suffix);
        };
        const bool enabled=pval(processor_.apvts,id("on"))>=0.5f;
        if(!enabled)continue; // Disabled CARVEs are genuinely invisible on the graph.
        const float x=freqToX(surgery::precision::boundedFrequency(
            pval(processor_.apvts,id("hz")),lowHz,highHz));
        const float depth=processor_.getCarveGainDb(selectedRoot,slot);
        const float y=responseY(depth);
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
        // Identification is available on hover and in the CARVE shelf;
        // the reference X-RAY lens has clean nodes, never black C1-C4 tabs.
    }
    // Real polycarbonate reflection sits above signal light and nodes, while
    // native labels/readouts remain crisp above the optical layer.
    scalpel::finish::analyzerReflection(g,full);
    scalpel::ui105::drawAnalyzerPanel(g,full,processor_.getPeakHoldDb(),
        processor_.getChannelPeakDb(0),processor_.getChannelPeakDb(1),polished);
    const auto domain=displayDomain_;
    const auto peak=peakReadouts_[(size_t)selectedRoot];
    const juce::String output=domain==1?"MID OUT":domain==2?"SIDE OUT":"STEREO OUT";
    const juce::String info=output+"  /  BAND "+juce::String(selectedRoot+1)+
        " PEAK "+(peak<=-140.f?juce::String("--"):juce::String(peak,1)+" dBFS");
    g.setColour(juce::Colours::white.withAlpha(.82f));
    g.setFont(juce::Font(juce::FontOptions("Segoe UI",11.0f,juce::Font::bold)));
    g.drawText(info,r.reduced(12.f).removeFromTop(15.f),juce::Justification::centredLeft);
    if(showResponse){
        const auto graphLabel=bandEnabled
            ? "BAND EQ  "+juce::String(gain>=0.0?"+":"")+juce::String(gain,1)+" dB"
            : juce::String("BAND OFF");
        g.setColour(scalpel::finish::ice.withAlpha(.9f));
        g.drawText(graphLabel,r.reduced(10.f).removeFromTop(15.f),juce::Justification::centredRight);
        if(mode_==0&&compActive){
            static constexpr std::array<const char*,8> ratioText{
                "AUTO","2:1","3:1","4:1","6:1","8:1","12:1","20:1"};
            const int ratioIndex=juce::jlimit(0,7,juce::roundToInt(pval(processor_.apvts,rid("comp_ratio"))));
            const auto dynamics="COMP "+juce::String(ratioText[(size_t)ratioIndex])+"  /  THR "+
                juce::String(threshold,1)+" dBFS  /  GR "+juce::String(-dynamicsReadout_[1],1)+" dB";
            g.setColour(scalpel::finish::accent.withAlpha(.82f));
            g.setFont(juce::Font(juce::FontOptions("Segoe UI",9.0f,juce::Font::bold)));
            auto status=r.reduced(10.f).removeFromTop(30.f);status.removeFromTop(15.f);
            g.drawText(dynamics,status,juce::Justification::centredRight);
        }
    }
    if(hover_&&r.contains(mouse_)){
        const int bar=prism::analysis::LabBands::indexForFrequency(xToFreq(mouse_.x));
        const float freq=xToFreq(mouse_.x);
        g.setColour(juce::Colours::white.withAlpha(0.26f));
        g.drawVerticalLine(juce::roundToInt(mouse_.x),r.getY(),r.getBottom());
        // BAND handles intentionally move in frequency only. A second,
        // pointer-height horizontal crosshair looked like a displaced BAND
        // response line; reserve that hover guide for ANALYZE mode.
        if(mode_==3)
            g.drawHorizontalLine(juce::roundToInt(mouse_.y),r.getX(),r.getRight());
        juce::String text=hzText(freq)+" Hz ~"+pitchText(freq)+" / "+
                juce::String(bands_[(size_t)bar],1)+" dBFS";
        if(showResponse){
            const auto p=(size_t)std::clamp((int)std::lround(std::log(freq/20.f)*2048.0/std::log(1000.0)),0,2048);
            const auto db=responseDb_[p];
            text+=" / EQ "+(db>=0?juce::String("+"):juce::String())+juce::String(db,1)+" dB";
        }
        const int hoveredCarve=hitCarveNode(mouse_);
        if(hoveredCarve>=0) {
            const int root=selected_/2;
            const auto id=[root,hoveredCarve](const char* suffix){
                return PRISMVSTAudioProcessor::carveId(root,hoveredCarve,suffix);
            };
            text="C"+juce::String(hoveredCarve+1)+" / "+
                 hzText(pval(processor_.apvts,id("hz")))+" Hz / Q "+
                 juce::String(pval(processor_.apvts,id("q")),2)+" / GAIN "+
                 juce::String(processor_.getCarveGainDb(root,hoveredCarve),1)+" dB";
        }
        const juce::Font hoverFont(juce::FontOptions("Segoe UI",11.0f,juce::Font::plain));
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(hoverFont,text,0,0);
        const float width=std::min(r.getWidth(),std::ceil(glyphs.getBoundingBox(0,-1,true).getWidth())+14.f);
        const float height=std::min(22.f,r.getHeight());
        const float x=mouse_.x+10.f+width<=r.getRight()?mouse_.x+10.f:mouse_.x-width-10.f;
        const float y=mouse_.y-height-9.f>=r.getY()?mouse_.y-height-9.f:mouse_.y+9.f;
        const juce::Rectangle<float> tip{std::clamp(x,r.getX(),r.getRight()-width),
            std::clamp(y,r.getY(),r.getBottom()-height),width,height};
        g.setColour(juce::Colour::fromRGB(6,11,18).withAlpha(0.97f));
        g.fillRoundedRectangle(tip,4.f);
        g.setColour(juce::Colour::fromRGB(236,243,247));
        g.setFont(hoverFont);
        g.drawFittedText(text,tip.toNearestInt().reduced(6,1),
                         juce::Justification::centredLeft,1);
    }
}
int RootsGraph::hitCarveTab(juce::Point<float> point) const {
    // C1–C4 buttons were moved off the graph into the physical CARVE shelf.
    juce::ignoreUnused(point);
    return -1;
}
int RootsGraph::hitCarveNode(juce::Point<float> point) const {
    const int root=selected_/2;
    const auto model=loadBandModel();
    const auto& band=model.bands[(size_t)root];
    if(!band.present)return -1;
    float nearest=15.0f;
    int target=-1;
    for(int slot=0;slot<surgery::ui::kSlotsPerRoot;++slot){
        const auto id=[root,slot](const char* suffix){
            return PRISMVSTAudioProcessor::carveId(root,slot,suffix);
        };
        if(pval(processor_.apvts,id("on"))<0.5f)continue; // Invisible means not hit-testable.
        const float x=freqToX(surgery::precision::boundedFrequency(
            pval(processor_.apvts,id("hz")),(float)band.left,(float)band.right));
        const auto plot=area();
        const float y=scalpel::dbaxis::responseY(processor_.getCarveGainDb(root,slot),plot.getY(),plot.getHeight());
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
    if(!e.mods.isPopupMenu()&&mode_==1){
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
            for(const auto* suffix:{"hz","gain_db"})
                if(auto* parameter=processor_.apvts.getParameter(
                       PRISMVSTAudioProcessor::carveId(root,slot,suffix)))
                    parameter->beginChangeGesture();
            return;
        }
    }
    // Normal clicks preserve the active band's handle priority. A deliberate
    // triple-click is the sole gesture that cycles to a neighboring band dot.
    const bool cycleCoincidentBand=!e.mods.isPopupMenu()&&e.getNumberOfClicks()>=3;
    const int hit=hitNode(e.position,cycleCoincidentBand);
    if(hit>=0)selectNode(hit);
    if(e.mods.isPopupMenu()){
        const int interior=hit>=0?hit/2:hitBandInterior(e.position);
        if(interior>=0){selectNode(interior*2);popup(nullptr,e.getScreenPosition());}
        return;
    }
    dragging_=hit;
    draggingInterior_=-1;
    snapTargetHz_=0.0;
    if(dragging_>=0){
        dragStartX_=e.position.x;
        dragStartHz_=param(dragging_,"hz");
        dragStartModel_=loadBandModel();
    }else{
        draggingInterior_=hitBandInterior(e.position);
        if(draggingInterior_>=0){
            selectNode(draggingInterior_*2);
            dragStartX_=e.position.x;
            dragStartModel_=loadBandModel();
        }
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
        if(!fine){
            const auto plot=area();
            const auto gain=scalpel::dbaxis::responseDb(e.position.y,plot.getY(),plot.getHeight());
            change(processor_,id("gain_db"),std::clamp(gain,-40.f,40.f));
            change(processor_,id("depth"),0.0f);
        }
        repaint();return;
    }
    if(draggingInterior_>=0){
        auto model=dragStartModel_;
        const double start=xToFreq(dragStartX_);
        const double now=xToFreq(e.position.x);
        model.moveGroup(draggingInterior_,std::log2(now/start));
        storeBandModel(model);
        repaint();return;
    }
    if(dragging_<0)return;
    auto model=loadBandModel();
    const int root=dragging_/2;
    const auto anchor=dragging_%2?scalpel::bands::Anchor::right:
                                  scalpel::bands::Anchor::left;
    const double pointerHz=xToFreq(freqToX(dragStartHz_)+e.position.x-dragStartX_);
    const double next=snappedFrequency(pointerHz,e.mods.isCtrlDown(),root,anchor);
    const bool wasFree=model.touchingNeighbor(root,anchor).band<0;
    if(model.moveSeam(root,anchor,next)&&wasFree&&!e.mods.isCtrlDown()
       &&model.link(root,anchor).band<0&&snapTargetHz_>0.0){
        bool connected=false;
        for(int other=0;other<7&&!connected;++other){
            if(other==root||!model.bands[(size_t)other].present
               ||model.bands[(size_t)other].domain!=model.bands[(size_t)root].domain)
                continue;
            const auto otherAnchor=anchor==scalpel::bands::Anchor::left?
                scalpel::bands::Anchor::right:scalpel::bands::Anchor::left;
            if(model.frequency(other,otherAnchor)==next)
                connected=model.connect(root,anchor,other,otherAnchor);
        }
        if(connected)snapTargetHz_=0.0;
    }
    storeBandModel(model);
    repaint();
}
void RootsGraph::mouseUp(const juce::MouseEvent&){
    if(draggingCarve_>=0){
        const int root=selected_/2,slot=draggingCarve_;
        for(const auto* name:{"hz","gain_db"})
            if(auto* p=processor_.apvts.getParameter(
                PRISMVSTAudioProcessor::carveId(root,slot,name)))
                p->endChangeGesture();
        draggingCarve_=-1;
        fineDragging_=false;
    }
    endBandGesture();
    if(dragging_>=0||draggingInterior_>=0){
        const auto current=loadBandModel();
        for(int root=0;root<scalpel::bands::count;++root){
            const auto& before=dragStartModel_.bands[(size_t)root];
            const auto& after=current.bands[(size_t)root];
            if(after.present&&(before.left!=after.left||before.right!=after.right))
                clampCarvesToRoot(root);
        }
    }
    dragging_=-1;draggingInterior_=-1;snapTargetHz_=0.0;
}
void RootsGraph::mouseMove(const juce::MouseEvent& e){
    mouse_=e.position;hover_=true;
    const bool movable=hitNode(e.position)>=0||hitBandInterior(e.position)>=0;
    setMouseCursor(juce::MouseCursor(movable?
        juce::MouseCursor::DraggingHandCursor:
        juce::MouseCursor::NormalCursor));
    repaint();
}
void RootsGraph::mouseExit(const juce::MouseEvent&){hover_=false;repaint();}
void RootsGraph::mouseDoubleClick(const juce::MouseEvent& e){
    if(e.mods.isPopupMenu())return;
    // Finish any gesture opened by the second mouse-down before resetting.
    const int tab=mode_==1?hitCarveTab(e.position):-1;
    const int carve=tab>=0?tab:(mode_==1?hitCarveNode(e.position):-1);
    const int boundary=carve<0?hitNode(e.position,false):-1;
    const bool coincidentBand=boundary>=0&&
        hitNode(e.position,true)!=boundary;
    if(draggingCarve_>=0||dragging_>=0||draggingInterior_>=0)mouseUp(e);
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
        // Do not reset an overlapping boundary on click two of an intentional
        // triple-click. Unique handles retain their double-click reset.
        if(!coincidentBand){
            auto model=loadBandModel();
            const auto anchor=boundary%2?scalpel::bands::Anchor::right:
                                           scalpel::bands::Anchor::left;
            if(model.moveAnchor(boundary/2,anchor,
                scalpel::bands::Model::factoryBoundaries[(size_t)boundary])){
                storeBandModel(model);clampCarvesToRoot(boundary/2);
            }
        }
        repaint();
        return;
    }
    if(area().contains(e.position)){
        auto model=loadBandModel();
        int slot=!model.bands[(size_t)(selected_/2)].present?selected_/2:-1;
        if(slot<0)for(int root=0;root<7;++root)
            if(!model.bands[(size_t)root].present){slot=root;break;}
        if(slot<0)return;
        const auto domain=model.bands[(size_t)slot].domain;
        const double hz=xToFreq(e.position.x);
        for(const auto& band:model.bands)
            if(band.present&&band.domain==domain&&hz>band.left&&hz<band.right)
                return;
        const int added=model.addAt(slot,hz,domain);
        if(added>=0){storeBandModel(model);selectNode(added*2);repaint();}
    }
}
void RootsGraph::mouseWheelMove(const juce::MouseEvent& e,
                                const juce::MouseWheelDetails& wheel){
    const int slot=mode_==1?hitCarveNode(e.position):-1;
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
void RootsGraph::popup(juce::Component* bandButton,juce::Point<int> screenPoint){
    const int root=selected_/2;
    auto model=loadBandModel();
    const auto changeDomain=[root](scalpel::bands::Model& layout, int domain){
        auto& band=layout.bands[(size_t)root];
        const auto target=static_cast<scalpel::bands::Domain>(domain);
        if(band.domain!=target){
            layout.unlink(root,scalpel::bands::Anchor::left);
            layout.unlink(root,scalpel::bands::Anchor::right);
            band.domain=target;
        }
        return layout.valid();
    };
    juce::PopupMenu menu;
    if(model.bands[(size_t)root].present){
        juce::PopupMenu routing;
        const char* domains[]{"STEREO","MID","SIDE"};
        for(int domain=0;domain<3;++domain){
            auto candidate=model;
            routing.addItem(10+domain,domains[domain],changeDomain(candidate,domain),
                (int)model.bands[(size_t)root].domain==domain);
        }
        menu.addSubMenu("BAND EQ DOMAIN",routing);
        menu.addSeparator();
        using scalpel::bands::Anchor;
        const bool leftLinked=model.link(root,Anchor::left).band>=0;
        const bool rightLinked=model.link(root,Anchor::right).band>=0;
        if(leftLinked)menu.addItem(4,"UNLINK LEFT MAGNET");
        if(rightLinked)menu.addItem(5,"UNLINK RIGHT MAGNET");
        if(leftLinked||rightLinked)menu.addSeparator();
        menu.addItem(3,"REMOVE BAND");
    }else{
        if(bandButton==nullptr)return;
        menu.addItem(6,"ADD BAND",model.restore(root));
    }
    const juce::Component::SafePointer<RootsGraph> safe(this);
    auto options=juce::PopupMenu::Options().withTargetComponent(
        bandButton!=nullptr?bandButton:this);
    if(bandButton==nullptr){
        const auto bounds=getScreenBounds();
        options=options.withTargetScreenArea({screenPoint.x,screenPoint.y,1,1})
            .withPreferredPopupDirection(screenPoint.y>bounds.getCentreY()
                ?juce::PopupMenu::Options::PopupDirection::upwards
                :juce::PopupMenu::Options::PopupDirection::downwards);
    }
    menu.showMenuAsync(options,[safe,root,changeDomain](int id){
            if(safe==nullptr||id<=0)return;
            auto& graph=*safe;
            auto layout=graph.loadBandModel();
            if(id>=10 && id<=12){
                if(!layout.bands[(size_t)root].present || !changeDomain(layout,id-10))return;
                const auto parameterId=PRISMVSTAudioProcessor::rootsRootControlId(root,"domain");
                if(auto* parameter=graph.processor_.apvts.getParameter(parameterId)){
                    parameter->beginChangeGesture();
                    graph.storeBandModel(layout);
                    parameter->endChangeGesture();
                }
                graph.repaint();
                return;
            }
            if(id==3)layout.remove(root);
            if(id==4||id==5)
                layout.unlink(root,id==4?scalpel::bands::Anchor::left:scalpel::bands::Anchor::right);
            if(id==6&&!layout.restore(root))return;
            graph.storeBandModel(layout);
            if(id==6)graph.selectNode(root*2);
            graph.repaint();
        });
}

RootsEditor::RootsEditor(PRISMVSTAudioProcessor& p)
: juce::AudioProcessorEditor(&p),processor_(p),graph_(p) {
    // The host-owned surround is a true black canvas.  The copper rack face
    // is drawn by paintRack() and must never bleed into the plugin border.
    setOpaque(true);
    setLookAndFeel(&lookAndFeel_);

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
        b.onContext=[this,root](){graph_.showRootMenu(root,rootButtons_[(size_t)root]);};
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
        label->setFont(juce::Font(juce::FontOptions("Segoe UI",12.0f,juce::Font::plain)));
        label->setJustificationType(juce::Justification::centredLeft);
    }
    title_.setText("SCALPEL EQ",juce::dontSendNotification);
    analyzerLegend_.setFont(juce::Font(juce::FontOptions("Segoe UI",12.0f,juce::Font::bold)));
    analyzerLegend_.setText("SPECTRAL COLOR   /   LEVEL",
                            juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions("Segoe UI",24.0f,juce::Font::bold)));
    selection_.setFont(juce::Font(juce::FontOptions("Segoe UI",12.0f,juce::Font::bold)));
    status_.setFont(juce::Font(juce::FontOptions("Segoe UI",11.5f,juce::Font::plain)));
    status_.setJustificationType(juce::Justification::centredRight);
    for(auto* s:{&gain_,static_cast<juce::Slider*>(&threshold_),&limitAmount_}){
        addAndMakeVisible(*s);rotary(*s,"",0.0);
        s->setColour(juce::Slider::rotarySliderFillColourId,juce::Colour::fromRGB(143,192,204));
        s->setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour::fromRGB(38,55,65));
        s->setColour(juce::Slider::textBoxTextColourId,juce::Colour::fromRGB(239,243,245));
        s->setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
        s->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    }
    // BAND MODE's two signed controls share an exact 270-degree scale:
    // minimum at bottom-left, zero at twelve o'clock, maximum at bottom-right.
    gain_.setComponentID("band-gain-dial");
    threshold_.setComponentID("band-threshold-dial");
    gain_.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,
                              juce::MathConstants<float>::pi*2.75f,true);
    threshold_.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,
                                   juce::MathConstants<float>::pi*2.75f,true);
    gainLabel_.setText("BAND GAIN (dB)",juce::dontSendNotification);
    gain_.setTooltip("Selected BAND frequency range gain: -60 to +60 dB");

    thresholdLabel_.setText("COMP THRESHOLD",juce::dontSendNotification);
    threshold_.setTooltip("-9 dB lowers the loud-signal threshold to -27 dBFS. +9 dB raises it to -9 dBFS and increases quiet-signal lift. Double-click for the -18 dBFS reference.");
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
    // The two labels are commands, not independent toggle latches. Drive the
    // shared APVTS state explicitly so BAND MODE cannot desynchronise ON/OFF.
    on_.setClickingTogglesState(false);
    on_.setTriggeredOnMouseDown(true);
    on_.onClick=[this]{setSelectedEnabled(false,true);};
    addAndMakeVisible(off_);
    off_.setClickingTogglesState(false);
    off_.setTriggeredOnMouseDown(true);
    off_.onClick=[this]{setSelectedEnabled(false,false);};
    static constexpr const char* labels[]{"BAND MODE","CARVE","ANALYZE"};
    for(int i=0;i<3;++i){
        auto& b=modeButtons_[(size_t)i];
        b.setButtonText(labels[i]);
        b.onClick=[this,i]{setMode(scalpel::reference::modePages[(size_t)i]);};
        addAndMakeVisible(b);
    }
    masterLabel_.setComponentID("master-caption");
    masterLabel_.setText("MASTER",juce::dontSendNotification);
    masterLabel_.setJustificationType(juce::Justification::centred);
    masterLabel_.setFont(juce::Font(juce::FontOptions("Segoe UI",13.f,juce::Font::bold)));
    masterLabel_.setColour(juce::Label::textColourId,juce::Colour(0xffcdd3da));
    addAndMakeVisible(masterLabel_);
    const char* domains[]{"STEREO","MID","SIDE"};
    for(int i=0;i<3;++i){
        auto& button=masterButtons_[(size_t)i];
        button.setButtonText(domains[i]);
        button.setComponentID("master-output-"+juce::String(i));
        button.setTooltip(i==0?"Master output: full stereo mix":
            i==1?"Master output: Mid (center) only":"Master output: Side (difference) only");
        button.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff222327));
        button.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff393b3f));
        button.setColour(juce::TextButton::textColourOffId,juce::Colour(0xffcdd3da));
        button.setColour(juce::TextButton::textColourOnId,juce::Colour(0xffecf0f3));
        button.onClick=[this,i]{
            if(auto* parameter=processor_.apvts.getParameter("master_output_domain")){
                parameter->beginChangeGesture();
                change(processor_,"master_output_domain",(float)i);
                parameter->endChangeGesture();
                timerCallback();
            }
        };
        addAndMakeVisible(button);
    }
    for(int i=0;i<4;++i){
        auto& b=carveSlotButtons_[(size_t)i];
        b.setButtonText("C"+juce::String(i+1));
        b.onClick=[this,i]{carveSlot_=i;bindCarve();graph_.setPresentation(mode_,carveSlot_);};
        addAndMakeVisible(b);
    }
    for(auto* s:{&carveFrequency_,&carveDepth_,static_cast<juce::Slider*>(&carveQ_)}){
        rotary(*s,"",0.0);addAndMakeVisible(*s);
    }
    carveDepth_.setDoubleClickReturnValue(true,0.0);
    carveQ_.setDoubleClickReturnValue(true,surgery::precision::defaultCarveQ);
    carveFrequency_.setMouseDragSensitivity(400);
    carveQ_.setMouseDragSensitivity(600);
    carveQ_.onValueChange=[this]{updateCarveTightness();};
    // The slider/host still hold raw Q; only editable UI text is tightness.
    carveQ_.textFromValueFunction=[](double q){return juce::String(surgery::calibration::qPosition(q)*100.0,1);};
    carveQ_.valueFromTextFunction=[](const juce::String& text){return surgery::calibration::qFromPosition(text.getDoubleValue()/100.0);};
    carveQ_.setNumDecimalPlacesToDisplay(1);
    carveFrequency_.setTooltip("Exact Hz value: type in the field. CTRL+drag a CARVE node for fine frequency scrubbing.");
    carveQ_.setTooltip("Tightness 0-100% (type a percentage). Raw DSP/automation Q remains 0.300-96.000 on a logarithmic curve. Clockwise narrows the filter. SHIFT+wheel = fine adjustment.");
    carveFrequencyLabel_.setText("FREQUENCY Hz",juce::dontSendNotification);
    carveDepthLabel_.setText("CARVE GAIN dB",juce::dontSendNotification);
    carveQLabel_.setText("Q / TIGHTNESS",juce::dontSendNotification);
    for(auto* b:{&carveOn_,&carveDynamic_}){
        b->setClickingTogglesState(true);
        addAndMakeVisible(*b);
    }
    carveOn_.setClickingTogglesState(false);
    carveOn_.setTriggeredOnMouseDown(true);
    carveOn_.onClick=[this]{setSelectedEnabled(true,true);};
    addAndMakeVisible(carveOff_);
    carveOff_.setClickingTogglesState(false);
    carveOff_.setTriggeredOnMouseDown(true);
    carveOff_.onClick=[this]{setSelectedEnabled(true,false);};
    mono_.setTooltip("Frequency-selective MONO on the selected BAND only");
    phase_.setTooltip("Analyzes coherent L/R phase at the center of this stereo band and rotates both channels toward alignment. Stereo bands only.");
    limit_.setTooltip("Enable fixed-depth LA-2A-inspired optical leveling for the selected band. The hidden automation amount remains compatible with saved sessions.");
    analyzerDepthLabel_.setText("HEAT MAP",juce::dontSendNotification);
    analyzerSlopeLabel_.setText("FREEZE",juce::dontSendNotification);
    analyzerMemoryLabel_.setText("VISUAL DECAY",juce::dontSendNotification);
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
    analyzerMemory_.setNumDecimalPlacesToDisplay(2);
    heatToggle_.setTooltip("Measured spectral heat map. Frequency sets colour; level sets intensity.");
    heatToggle_.setClickingTogglesState(true);
    addAndMakeVisible(heatToggle_);
    heatA_=std::make_unique<ButtonA>(processor_.apvts,"aura_color",heatToggle_);
    xrayButton_.setClickingTogglesState(true);
    xrayButton_.setTooltip("X-RAY visualizer. Display only; audio processing is unchanged.");
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
    jukeModeLabel_.setFont(juce::Font(juce::FontOptions("Segoe UI",11.f,juce::Font::plain)));jukeModeLabel_.setColour(juce::Label::textColourId,juce::Colour(0xffb8c5d1));
    reducedMotion_.setClickingTogglesState(true);addAndMakeVisible(reducedMotion_);
    reducedMotion_.setTooltip("Suppresses weather drift and lightning. Enabled by default. Full motion still limits local flashes to one per 0.75 seconds.");
    reducedMotionA_=std::make_unique<ButtonA>(processor_.apvts,"sonar_reduced_motion",reducedMotion_);
    analyzerLegend_.setText("SONAR 2.1 / VISUAL MODE",juce::dontSendNotification);
    compressor_.addItemList({ "OFF", "176E", "CE1B" },1);
    ratio_.addItemList({"AUTO","2:1","3:1","4:1","6:1","8:1","12:1","20:1"},1);
    ratio_.setTooltip("Selected band compressor ratio. AUTO uses 4:1 for 176E or 6:1 for CE1B.");
    detector_.addItemList({ "LINKED", "DUAL M/S", "MID ONLY", "SIDE ONLY" },1);
    for(auto* box:std::array<juce::ComboBox*,3>{&compressor_,&ratio_,&detector_})addAndMakeVisible(*box);
    compLabel_.setText("COMPRESSION",juce::dontSendNotification);
    ratioLabel_.setText("RATIO",juce::dontSendNotification);
    detectorLabel_.setText("DYNAMIC",juce::dontSendNotification);
    for(auto* label:{&compLabel_,&ratioLabel_,&detectorLabel_,&outputLabel_}){
        label->setFont(juce::Font(juce::FontOptions("Segoe UI",11.0f,juce::Font::plain)));
        label->setColour(juce::Label::textColourId,juce::Colour(0xffafc8cf));
        addAndMakeVisible(*label);
    }
    for(auto* button:{&bypass_,&compare_,&copy_,&save_,&load_,&peakHold_})addAndMakeVisible(*button);
    peakHold_.onClick=[this]{processor_.resetPeakHold();graph_.clearPeakReadouts();};
    peakHold_.setTooltip("Reset the held output peak and all seven held band peaks.");
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
    // A clean Sanyo/Game Boy control face: no inherited Surgery status strip,
    // output gain fader, bypass, A/B, copy or file buttons on the chassis.
    // The associated APVTS parameters/preset code survive for DAW compatibility.
    for(auto* button:{&bypass_,&compare_,&copy_,&save_,&load_})
        button->setVisible(false);
    peakHold_.setVisible(true);
    peakHold_.setButtonText("RESET");
    output_.setVisible(false);outputLabel_.setVisible(false);
    bindNode(0);setMode(0);
    // Host resize handles remain available without JUCE's generic corner grip.
    // Every user resize is snap-locked to the authored 16:9 rack face and
    // clamped to tested minimum/maximum sizes.
    setResizable(true,false);
    setResizeLimits(896,504,1792,1008);
    if(auto* constrainer=getConstrainer())constrainer->setFixedAspectRatio(16.0/9.0);
    setSize(1344,756);
    timerCallback();startTimerHz(60);
}
RootsEditor::~RootsEditor(){
    closing_=true;
    stopTimer();
    presetChooser_.reset();
    graph_.onSelectNode=nullptr;
    graph_.onSelectCarve=nullptr;
    graph_.releaseVisualResources();
    setLookAndFeel(nullptr);
}
void RootsEditor::choosePreset(bool saving){
    presetChooser_=std::make_unique<juce::FileChooser>(saving?"Save Scalpel EQ preset":"Load Scalpel EQ preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Scalpel EQ.surgerypreset"),"*.surgerypreset");
    const auto chooserFlags=saving?(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting):
        (juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles);
    presetChooser_->launchAsync(chooserFlags,[safe=juce::Component::SafePointer<RootsEditor>(this),saving](const juce::FileChooser& chooser){
        if(safe==nullptr||safe->closing_)return;
        auto file=chooser.getResult();if(file==juce::File{})return;
        const bool ok=saving?safe->processor_.savePreset(file.withFileExtension("surgerypreset")):safe->processor_.loadPreset(file);
        if(!ok)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Preset could not be opened",
            saving?"Could not write this preset. Choose a writable folder.":"Choose a valid Scalpel EQ preset file.");
        safe->timerCallback();
    });
}
void RootsEditor::bindNode(int n){
    node_=juce::jlimit(0,prism::roots::v2::kNodes-1,n);
    compressorA_.reset();ratioA_.reset();detectorA_.reset();
    gainA_.reset();thresholdA_.reset();limitAmountA_.reset();monoA_.reset();
    onA_.reset();soloA_.reset();phaseA_.reset();limitA_.reset();
    const auto rid=[this](const char* suffix){
        return PRISMVSTAudioProcessor::rootsRootControlId(node_/2,suffix);
    };
    compressorA_=std::make_unique<juce::ParameterAttachment>(
        *processor_.apvts.getParameter(rid("dynamics_mode")),[this](float mode){
            compressor_.setSelectedId(juce::jlimit(1,3,juce::roundToInt(mode)),juce::dontSendNotification);
        },nullptr);
    compressor_.onChange=[this]{
        if(compressorA_&&compressor_.getSelectedId()>0)
            compressorA_->setValueAsCompleteGesture((float)compressor_.getSelectedId());
    };
    compressorA_->sendInitialUpdate();
    ratioA_=std::make_unique<ComboA>(processor_.apvts,rid("comp_ratio"),ratio_);
    detectorA_=std::make_unique<ComboA>(processor_.apvts,rid("detector"),detector_);
    gainA_=std::make_unique<SliderA>(processor_.apvts,
       PRISMVSTAudioProcessor::rootsRootGainId(node_/2),gain_);

    thresholdA_=std::make_unique<SliderA>(processor_.apvts,rid("comp_offset"),threshold_);
    threshold_.textFromValueFunction=[](double db){
        return (db>0?juce::String("+"):juce::String())+juce::String(db,1);
    };
    threshold_.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue();};
    threshold_.setNumDecimalPlacesToDisplay(1);
    threshold_.setTextValueSuffix(" dB");
    threshold_.updateText();
    limitAmountA_=std::make_unique<SliderA>(processor_.apvts,rid("limit_amount"),limitAmount_);
    phaseA_=std::make_unique<ButtonA>(processor_.apvts,rid("phase"),phase_);
    limitA_=std::make_unique<ButtonA>(processor_.apvts,rid("limit_on"),limit_);
    monoA_=std::make_unique<ButtonA>(processor_.apvts,rid("mono"),mono_);
    // ON/OFF issue explicit parameter commands; a ButtonAttachment would
    // also write its toggle state on click and race that command.
    soloA_=std::make_unique<ButtonA>(processor_.apvts,
        PRISMVSTAudioProcessor::rootsRootSoloId(node_/2),solo_);
    selection_.setText("BAND "+juce::String(node_/2+1)+
        "  /  CARVE "+juce::String(carveSlot_+1),juce::dontSendNotification);
    bindCarve();
    updateRootButtons();
}
void RootsEditor::updateCarveTightness(){
    const float tight=surgery::precision::tightnessPercent((float)carveQ_.getValue());
    const auto text="Q / "+juce::String(tight,1)+"% TIGHT";
    if(carveQLabel_.getText()!=text)carveQLabel_.setText(text,juce::dontSendNotification);
}
void RootsEditor::bindCarve(){
    carveFrequencyA_.reset();carveDepthA_.reset();
    carveQA_.reset();
    carveOnA_.reset();carveDynamicA_.reset();
    const int root=node_/2,slot=carveSlot_;
    selection_.setText("BAND "+juce::String(root+1)+"  /  CARVE "+juce::String(slot+1),
                       juce::dontSendNotification);
    const float a=pval(processor_.apvts,PRISMVSTAudioProcessor::rootsNodeId(root*2,"hz"));
    const float b=pval(processor_.apvts,PRISMVSTAudioProcessor::rootsNodeId(root*2+1,"hz"));
    const double low=juce::jmax(20.0f,std::min(a,b));
    const double high=juce::jmax((float)low,std::max(a,b));
    carveFrequency_.setDoubleClickReturnValue(true,
        std::exp(std::log(low)+(slot+1.0)/5.0*std::log(high/low)));
    const auto id=[root,slot](const char* s){
        return PRISMVSTAudioProcessor::carveId(root,slot,s);
    };
    carveFrequencyA_=std::make_unique<SurgeryLogSliderAttachment>(
        *processor_.apvts.getParameter(id("hz")),carveFrequency_,low,high,.01);
    carveDepthA_=std::make_unique<ScalpelCarveGainAttachment>(
        *processor_.apvts.getParameter(id("gain_db")),*processor_.apvts.getParameter(id("depth")),carveDepth_);
    carveQA_=std::make_unique<SurgeryLogSliderAttachment>(
        *processor_.apvts.getParameter(id("q")),carveQ_,.3,96.0,.001);
    carveQ_.updateText();
    updateCarveTightness();
    carveDynamicA_=std::make_unique<ButtonA>(processor_.apvts,id("dynamic"),carveDynamic_);
    carveOn_.setButtonText("ON");
    updateCarveButtons();
    graph_.setPresentation(mode_,carveSlot_);
}
void RootsEditor::setMode(int next){
    mode_=next==1||next==3?next:0;
    lookAndFeel_.setFinishEnabled(scalpel::finish::enabledForMode(mode_));
    for(int i=0;i<3;++i){
        auto& b=modeButtons_[(size_t)i];
        b.setToggleState(scalpel::reference::modePages[(size_t)i]==mode_,juce::dontSendNotification);
        b.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff182a38));
        b.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff274960));
        b.setColour(juce::TextButton::textColourOffId,juce::Colour(0xffb7c9d8));
        b.setColour(juce::TextButton::textColourOnId,juce::Colour(0xffedf7ff));
    }
    const bool carve=mode_==1;
    for(auto* s:{&carveFrequency_,&carveDepth_,static_cast<juce::Slider*>(&carveQ_)})s->setVisible(carve);
    for(auto* l:{&carveFrequencyLabel_,&carveDepthLabel_,&carveQLabel_})l->setVisible(carve);
    for(auto& b:carveSlotButtons_)b.setVisible(carve);
    carveOn_.setVisible(carve);carveOff_.setVisible(carve);
    carveDynamic_.setVisible(false);
    // Legacy SONAR / JUKE settings stay automatable and preset-compatible,
    // but aren't injected into the redesigned 3-control ANALYZE shelf.
    analyzerLegend_.setVisible(false);
    weatherMode_.setVisible(false);jukeMode_.setVisible(false);jukeModeLabel_.setVisible(false);reducedMotion_.setVisible(false);
    for(auto* box:{&compressor_,&ratio_})box->setVisible(mode_==0);
    for(auto* label:{&compLabel_,&ratioLabel_})label->setVisible(mode_==0);
    // Retain the legacy detector parameter for automation/preset recall only.
    // Its former 75 px control collided with the MONO switch block.
    detector_.setVisible(false);detectorLabel_.setVisible(false);
    gain_.setVisible(mode_==0);gainLabel_.setVisible(mode_==0);
    threshold_.setVisible(mode_==0);
    thresholdLabel_.setVisible(mode_==0);
    // LIMIT is a true single-button control; its old amount rotary is hidden.
    limitAmount_.setVisible(false);
    phase_.setVisible(mode_==0);
    limit_.setVisible(mode_==0);
    on_.setVisible(mode_==0);
    off_.setVisible(mode_==0);
    solo_.setVisible(mode_==0);
    mono_.setVisible(mode_==0);
    const bool analyze=mode_==3;
    analyzerDepth_.setVisible(false);
    analyzerDepthLabel_.setVisible(analyze);
    analyzerSlope_.setVisible(false);
    analyzerSlopeLabel_.setVisible(analyze);
    analyzerMemory_.setVisible(analyze);
    analyzerMemoryLabel_.setVisible(analyze);
    heatToggle_.setVisible(analyze);
    freezeButton_.setVisible(analyze);
    selection_.setVisible(carve);
    status_.setVisible(false);
    detector_.setVisible(false);detectorLabel_.setVisible(false);
    graph_.setPresentation(mode_,carveSlot_);
    resized();
    timerCallback();
    repaint();
}
void RootsEditor::setSelectedEnabled(bool carve,bool enabled){
    const auto id=carve?PRISMVSTAudioProcessor::carveId(node_/2,carveSlot_,"on"):
                        PRISMVSTAudioProcessor::rootsRootControlId(node_/2,"on");
    if(auto* parameter=processor_.apvts.getParameter(id)){
        const float value=enabled?1.f:0.f;
        if(parameter->convertFrom0to1(parameter->getValue())!=value){
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    }
    // Respond on this press, without a timer tick or another analyzer/weather
    // update on the click path. Host automation is still reconciled by timer.
    auto& on=carve?carveOn_:on_;
    auto& off=carve?carveOff_:off_;
    const bool actual=pval(processor_.apvts,id)>=.5f;
    on.setToggleState(actual,juce::dontSendNotification);
    off.setToggleState(!actual,juce::dontSendNotification);
    if(carve)updateCarveButtons();
    on.repaint();off.repaint();graph_.repaint();
}
void RootsEditor::updateCarveButtons(){
    for(int i=0;i<4;++i){
        auto& button=carveSlotButtons_[(size_t)i];
        button.setToggleState(i==carveSlot_,juce::dontSendNotification);
        const auto id=PRISMVSTAudioProcessor::carveId(node_/2,i,"on");
        if(button.getProperties().set("carve-active",pval(processor_.apvts,id)>=.5f))
            button.repaint();
    }
}
void RootsEditor::updateRootButtons(){
    const double nowMs=juce::Time::getMillisecondCounterHiRes();
    const auto revision=processor_.getRootBandPeakRevision();
    const bool fresh=revision!=lastPeakRevision_;
    lastPeakRevision_=revision;
    std::array<float,7> warnings{};
    for(int root=0;root<7;++root){
        const bool selected=graph_.selectedNode()/2==root;
        auto& button=rootButtons_[(size_t)root];
        button.setToggleState(selected,juce::dontSendNotification);
        if(fresh&&processor_.getRootBandPeakDb(root)>=-0.01f)
            peakWarningUntilMs_[(size_t)root]=nowMs+750.0;
        const bool peak=nowMs<peakWarningUntilMs_[(size_t)root];
        const float pulse=.35f+.25f*(float)std::sin(nowMs*.0043);
        warnings[(size_t)root]=peak?pulse:0.f;
        button.setColour(juce::TextButton::buttonOnColourId,
            peak?juce::Colour(0xffdf1715).withAlpha(pulse):territory(root).withAlpha(.52f));
        button.setColour(juce::TextButton::buttonColourId,
            peak?juce::Colour(0xffde1714).withAlpha(pulse):juce::Colour(0xffdbc5ac));
    }
    graph_.setPeakWarnings(warnings);
}
void RootsEditor::timerCallback(){
    const bool b=processor_.comparisonSlot()==1;
    compare_.setButtonText(b?"A / B: B":"A / B: A");
    copy_.setButtonText(b?"COPY B > A":"COPY A > B");
    const float held=processor_.getPeakHoldDb();
    juce::ignoreUnused(held);
    peakHold_.setButtonText("RESET");
    peakHold_.setToggleState(false,juce::dontSendNotification);
    const auto labelColour=scalpel::finish::text;
    for(auto* label:{&title_,&selection_,&status_,&gainLabel_,&thresholdLabel_,
                    &carveFrequencyLabel_,&carveDepthLabel_,&carveQLabel_,
                    &analyzerDepthLabel_,&analyzerSlopeLabel_,&analyzerMemoryLabel_,&analyzerLegend_,
                    &compLabel_,&ratioLabel_,&detectorLabel_,&outputLabel_})
        label->setColour(juce::Label::textColourId,labelColour);
    // Threshold remains editable while OFF without activating compression.
    threshold_.setEnabled(true);
    phase_.setEnabled(pval(processor_.apvts,
        PRISMVSTAudioProcessor::rootsRootControlId(node_/2,"domain"))==0.f);
thresholdLabel_.setText("COMP THRESHOLD",
                            juce::dontSendNotification);
    updateRootButtons();
    updateCarveTightness();
    updateCarveButtons();
    if(carveFrequencyA_){
        const auto bounds=surgery::precision::rootBounds(
            pval(processor_.apvts,PRISMVSTAudioProcessor::rootsNodeId(node_/2*2,"hz")),
            pval(processor_.apvts,PRISMVSTAudioProcessor::rootsNodeId(node_/2*2+1,"hz")));
        carveFrequencyA_->setBounds(bounds.lo,bounds.hi);
    }
    carveOn_.setButtonText("ON");
    const bool bandOn=pval(processor_.apvts,
        PRISMVSTAudioProcessor::rootsRootControlId(node_/2,"on"))>=.5f;
    on_.setToggleState(bandOn,juce::dontSendNotification);
    off_.setToggleState(!bandOn,juce::dontSendNotification);
    const bool carveOn=pval(processor_.apvts,
        PRISMVSTAudioProcessor::carveId(node_/2,carveSlot_,"on"))>=.5f;
    carveOn_.setToggleState(carveOn,juce::dontSendNotification);
    carveOff_.setToggleState(!carveOn,juce::dontSendNotification);
    heatToggle_.setButtonText("HEAT MAP");
    graph_.setPresentation(mode_,carveSlot_);
    std::array<float,PRISMVSTAudioProcessor::labBars> bars{},peaks{};
    const int masterMode=processor_.getMasterDomain();
    for(int i=0;i<3;++i)
        masterButtons_[(size_t)i].setToggleState(i==masterMode,juce::dontSendNotification);
    processor_.setAnalyzerDomain(juce::roundToInt(pval(processor_.apvts,
        PRISMVSTAudioProcessor::rootsRootControlId(node_/2,"domain"))));
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
    // The old Surgery JUKE exterior is intentionally retired. X-RAY alone
    // consumes shared weather measurements and repaints its own graph bounds.
    const auto state=processor_.getRootsMeter();
    const juce::String status=
        "IN "+juce::String(processor_.getInputPeakDb(),1)+" dBFS  /  OUT "+
        juce::String(processor_.getPeakDb(),1)+" dBFS  /  CORR "+
        juce::String(state.correlation,2)+"  /  ROOT GR "+
        juce::String(state.largestGrDb,1)+" dB";
    if(mode_==3){
        const auto metrics=graph_.spectralMetrics();
        const juce::String line=metrics.signalPresent?
            "PEAK "+hzText(metrics.loudestHz)+" Hz  /  "+
            juce::String(metrics.loudestDb,1)+" dBFS  /  CENTROID "+
            hzText(metrics.approximateCentroidHz)+" Hz":
            "SIGNAL BELOW DISPLAY RANGE";
        status_.setText(line,juce::dontSendNotification);
    } else status_.setText(status,juce::dontSendNotification);
}
void RootsEditor::paint(juce::Graphics& g){
    // Clear the host/window surround before compositing the fitted rack.  This
    // keeps letterbox pixels and antialiased outer edges black at every size.
    g.fillAll(juce::Colours::black);
    // Cache in physical pixels. Crossing a DPI boundary or changing host UI
    // scale rebuilds the surface even when logical editor dimensions match.
    const float dpi=juce::jlimit(.5f,4.f,g.getInternalContext().getPhysicalPixelScaleFactor());
    const int pw=juce::roundToInt(getWidth()*dpi),ph=juce::roundToInt(getHeight()*dpi);
    if(rackBackground_.getWidth()!=pw||rackBackground_.getHeight()!=ph||backgroundMode_!=mode_||backgroundScale_!=dpi){
        rackBackground_=juce::Image(juce::Image::RGB,pw,ph,false);
        juce::Graphics background(rackBackground_);
        background.fillAll(juce::Colours::black);
        background.addTransform(juce::AffineTransform::scale(dpi));
        paintRack(background);backgroundMode_=mode_;backgroundScale_=dpi;
    }
    g.setOpacity(1.f);g.drawImage(rackBackground_,getLocalBounds().toFloat());
    // A visual separation from Surgery EQ is intentional: its external
    // cyan/purple JUKE rails are not part of Scalpel's 1980s hardware face.
}
void RootsEditor::resized(){rackBackground_={};backgroundScale_=0;layoutRack();repaint();}

void RootsEditor::paintRack(juce::Graphics& g){
    juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(surgery::rack::contentTransform(getWidth(),getHeight()));
    scalpel::ui105::drawRackFace(g,1120,630,mode_);
}

void RootsEditor::layoutRack(){
    // The approved 16:9 console is authored on a 1120 x 630 logical grid.
    // One uniform transform keeps assets, text and hit targets pixel-aligned.
    const auto transform=surgery::rack::contentTransform(getWidth(),getHeight());
    for(auto* child:getChildren()){
        if(dynamic_cast<juce::ResizableCornerComponent*>(child)!=nullptr)child->setTransform({});
        else child->setTransform(transform);
    }
    for(auto* label:{&gainLabel_,&thresholdLabel_,&carveFrequencyLabel_,&carveDepthLabel_,&carveQLabel_,
                     &analyzerDepthLabel_,&analyzerSlopeLabel_,&analyzerMemoryLabel_}){
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::Font(juce::FontOptions("Segoe UI",14.0f,juce::Font::bold)));
    }
    title_.setVisible(false);
    using namespace scalpel::reference;
    for(int root=0;root<7;++root){auto& button=rootButtons_[(size_t)root];button.setVisible(true);button.setBounds(band(root));}
    graph_.setBounds(analyzer().toNearestInt());
    for(int i=0;i<3;++i){
        modeButtons_[(size_t)i].setBounds(tab(modePages[(size_t)i]));
        masterButtons_[(size_t)i].setBounds(masterButton(i));
    }
    masterLabel_.setBounds(masterCaption());
    xrayButton_.setBounds(analyzerXray());
    peakHold_.setBounds(analyzerReset());

    on_.setBounds(switchBounds(false));off_.setBounds(switchBounds(true));
    gainLabel_.setBounds(bounds(344,595,180,28));gain_.setBounds(bounds(352,624,164,199));
    thresholdLabel_.setBounds(bounds(580,595,190,28));threshold_.setBounds(bounds(592,624,166,199));
    compLabel_.setBounds(bounds(843,601,270,28));compressor_.setBounds(bounds(837,636,296,55));
    ratioLabel_.setBounds(bounds(843,698,270,28));ratio_.setBounds(bounds(837,732,296,55));
    detectorLabel_.setBounds(0,0,1,1);detector_.setBounds(0,0,1,1);limitAmount_.setBounds(0,0,1,1);
    mono_.setBounds(bandAction(0));solo_.setBounds(bandAction(1));
    phase_.setBounds(bandAction(2));limit_.setBounds(bandAction(3));

    selection_.setBounds(bounds(94,589,250,28));
    carveOn_.setBounds(switchBounds(false));carveOff_.setBounds(switchBounds(true));
    carveDynamic_.setBounds(0,0,1,1);
    const std::array<juce::Slider*,3> cs{&carveFrequency_,&carveDepth_,static_cast<juce::Slider*>(&carveQ_)};
    const std::array<juce::Label*,3> cl{&carveFrequencyLabel_,&carveDepthLabel_,&carveQLabel_};
    for(int i=0;i<3;++i){
        cs[(size_t)i]->setBounds(carveDial(i));
        cl[(size_t)i]->setBounds(cs[(size_t)i]->getX()-20,bounds(0,589,1,28).getY(),cs[(size_t)i]->getWidth()+40,bounds(0,589,1,28).getHeight());
    }
    for(int i=0;i<4;++i)carveSlotButtons_[(size_t)i].setBounds(carveSlot(i));
    // ANALYZE headings use measured reference hierarchy and remain clear of
    // the knob canvas; do not alter typography on unrelated pages.
    for(auto* label:{&analyzerDepthLabel_,&analyzerMemoryLabel_,&analyzerSlopeLabel_})
        label->setFont(juce::Font(juce::FontOptions("Segoe UI",20.f,juce::Font::bold)));
    analyzerDepthLabel_.setBounds(analyzeCaption(0));
    analyzerMemoryLabel_.setBounds(analyzeCaption(1));
    analyzerSlopeLabel_.setBounds(analyzeCaption(2));
    heatToggle_.setBounds(analyzeButton(false));
    analyzerMemory_.setTextBoxStyle(juce::Slider::TextBoxBelow,false,decayReadoutWidth,decayReadoutHeight);
    analyzerMemory_.setBounds(decayDial());
    freezeButton_.setBounds(analyzeButton(true));

    jukeModeLabel_.setBounds(0,0,1,1);jukeMode_.setBounds(0,0,1,1);
    weatherMode_.setBounds(0,0,1,1);reducedMotion_.setBounds(0,0,1,1);
    analyzerDepth_.setBounds(0,0,1,1);analyzerSlope_.setBounds(0,0,1,1);
    analyzerLegend_.setBounds(0,0,1,1);status_.setBounds(0,0,1,1);
    bypass_.setBounds(0,0,1,1);compare_.setBounds(0,0,1,1);copy_.setBounds(0,0,1,1);
    save_.setBounds(0,0,1,1);load_.setBounds(0,0,1,1);
    outputLabel_.setBounds(0,0,1,1);output_.setBounds(0,0,1,1);
}





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
        0xFFFF5A42u,0xFFD5A076u,0xFF9BBA89u,0xFF759ACA,
        0xFFBB79CFu,0xFFCE8DA4u,0xFFD6474Eu
    };
    return juce::Colour(hues[(size_t)juce::jlimit(0,6,i)]);
}
void rotary(juce::Slider& s,juce::String suffix,double resetValue) {
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,22);
    s.setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xff2b2019));
    s.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xffe3cdb4));
    s.setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xffa58c70));
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
    // The new Scalpel lens fills the deck, with axes etched into its bezel.
    const auto b=getLocalBounds().toFloat();
    return {b.getX()+29.f,b.getY()+26.f,b.getWidth()-66.f,b.getHeight()-59.f};
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
    if(xrayActive_)mercuryElapsed_=elapsed;
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

    const juce::Colour top(0xff22201e),bottom(0xff090909);
    juce::ColourGradient bg(top,full.getX(),full.getY(),
                            bottom,full.getX(),full.getBottom(),false);
    bg.addColour(.43,juce::Colour(0xff100f0f));
    g.setGradientFill(bg);g.fillRoundedRectangle(full.reduced(1.0f),7.0f);
    g.setColour(juce::Colour(0xff070605));
    g.drawRoundedRectangle(full.reduced(1.0f),7.0f,2.0f);
    {
        g.setColour(juce::Colour(0xff6d5b4b));g.strokePath(surgery::rack::cut(full.reduced(1.5f),7),juce::PathStrokeType(1.15f));
        g.setColour(juce::Colour(0xff010101));g.strokePath(surgery::rack::cut(full.reduced(4),5),juce::PathStrokeType(2));
    }
    g.setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::bold)));
    g.setColour(juce::Colour(0xffb4aaa0));
    // No inherited cyan spectrum title, old SONAR weather image or C1-C4
    // badges: the graph is a continuous, smoked-glass Scalpel lens.
    // Frequency and calibrated dBFS dimensions.
    static constexpr std::array<float,11> freqs{
        20,40,80,160,315,630,1250,2500,5000,10000,20000
    };
    for(const float f:freqs){
        const float x=freqToX(f);
        g.setColour(juce::Colour(0xff8a796b).withAlpha(.20f));
        g.drawVerticalLine(juce::roundToInt(x),r.getY(),r.getBottom());
        g.setColour(juce::Colour(0xffc7b5a1));
        g.drawText(hzText(f),juce::roundToInt(x-19),juce::roundToInt(r.getBottom()+6),
                   38,17,juce::Justification::centred);
    }
    static constexpr std::array<int,11> dbTicks{0,-5,-10,-15,-20,-25,-30,-35,-40,-50,-60};
    for(const int db:dbTicks){
        const float y=r.getY()-r.getHeight()*db/60.0f;
        g.setColour(juce::Colour(0xffa89986).withAlpha(db==-30?.34f:.19f));
        g.drawHorizontalLine(juce::roundToInt(y),r.getX(),r.getRight());
        g.setColour(juce::Colour(0xffc7b8a8));
        g.drawText(juce::String(db),juce::roundToInt(r.getRight()+5),
                   juce::roundToInt(y-8),34,16,juce::Justification::centredLeft);
    }
    // ROOT handle height is constant; vertical axis is exclusively dBFS.
    // ROOT gain is a separate, single control in the bottom panel.
    g.setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::plain)));
    g.setColour(juce::Colour(0xffc5b7a9));
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
        const auto warning=peakWarnings_[(size_t)root];
        const auto accent=warning>0.f?juce::Colour(0xffd71916):
            (root==selected_/2?colour(root):juce::Colour(0xff9b7b68));
        g.setColour(accent.withAlpha(warning>0.f?warning*.33f:
            root==selected_/2?.095f:.017f));
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
    if(!xrayActive_){
        // Standard Scalpel graph is smoked graphite, never Surgery's bright
        // cyan/blue filled histogram. HEAT MAP adds restrained spectral warmth.
        juce::ColourGradient graphite(juce::Colour(0xff35302b).withAlpha(.64f),
            r.getX(),r.getY(),juce::Colour(0xff0b0a09).withAlpha(.9f),
            r.getX(),r.getBottom(),false);
        g.setGradientFill(graphite);g.fillPath(filledPath);
        if(heatOn){
            juce::Graphics::ScopedSaveState heat(g);
            g.reduceClipRegion(filledPath);
            juce::ColourGradient glint(juce::Colour(0xffb2816d).withAlpha(.12f),
                r.getX(),r.getY(),juce::Colour(0xff0c1111).withAlpha(.01f),
                r.getRight(),r.getBottom(),false);
            g.setGradientFill(glint);g.fillRect(r);
        }
    } else {
        // SCALPEL X-RAY: depth-layered black mercury, driven only by the
        // existing measured FFT and shared synesthesia/weather state.
        // The clipping path prevents any decorative peaks outside the data.
        juce::Graphics::ScopedSaveState mercury(g);
        g.reduceClipRegion(filledPath);
        const auto& m=weatherInterpretation_.state();
        const float energy=juce::jlimit(0.f,1.f,
            .30f*m.level+.35f*m.bass+.20f*m.flux+.15f*m.width);
        // A charcoal base remains legible even when the measured crest is
        // far below 0 dBFS. Previously the brightest stop lived above the
        // audio silhouette, making the real Windows X-RAY render look flat.
        juce::ColourGradient tar(juce::Colour(0xff191d1e).withAlpha(.96f),
            r.getCentreX(),r.getY(),juce::Colour(0xff020304).withAlpha(.99f),
            r.getCentreX(),r.getBottom(),false);
        tar.addColour(.20,juce::Colour(0xff0b0d10));
        tar.addColour(.43,juce::Colour(0xff343738));
        tar.addColour(.61,juce::Colour(0xff101719));
        tar.addColour(.78,juce::Colour(0xff2e3334));
        tar.addColour(.91,juce::Colour(0xff080a0b));
        g.setGradientFill(tar);g.fillRect(r);
        // Material sheen is a reflection field, not an independent waveform.
        // Its apparent ridges are clipped to the actual FFT silhouette.
        juce::ColourGradient oblique(juce::Colour(0xffb2c8c5).withAlpha(.075f),
            r.getX(),r.getBottom(),juce::Colour(0xffba916f).withAlpha(.10f),
            r.getRight(),r.getY(),false);
        oblique.addColour(.38,juce::Colour(0xff282026).withAlpha(.016f));
        oblique.addColour(.73,juce::Colour(0xffb9a5c2).withAlpha(.048f));
        g.setGradientFill(oblique);g.fillRect(r);
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
            for(int column=0;column<(int)source.size();++column){
                const float u=(column+.5f)/(float)source.size();
                const int bin=juce::jlimit(0,N-1,juce::roundToInt(u*(N-1)));
                const auto syn=surgery::synesthesia::read(m,u);
                const float local=juce::jlimit(0.f,1.f,(bands_[(size_t)bin]+72.f)/65.f);
                source[(size_t)column]={local,syn.r,syn.g,syn.b};
            }
            // GPU velocity/pressure/phase transport is restricted to this UI path.
            // The measured contour, parameter IDs and all audio code are unchanged.
            mercury3D_.render(source,m.bass,m.flux,m.level,mercuryElapsed_,
                              pval(processor_.apvts,"sonar_reduced_motion")>=.5f,
                              mercury3DImage_);
        }
        if(mercury3D_.hasFrame()){
            g.setOpacity(.98f);g.drawImage(mercury3DImage_,r);g.setOpacity(1.f);
        }else{
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
                    const float red=juce::jlimit(0.f,1.f,ambient+gloss*(.42f+iridescence*syn.r));
                    const float green=juce::jlimit(0.f,1.f,ambient+gloss*(.49f+iridescence*syn.g));
                    const float bCol=juce::jlimit(0.f,1.f,ambient*1.13f+gloss*(.53f+iridescence*syn.b));
                    pixels.setPixelColour(px,py,juce::Colour::fromFloatRGBA(red,green,bCol,1.f));
                }
            }
        }
        g.setOpacity(.94f);
        g.drawImage(mercuryMaterial_,r);
        g.setOpacity(1.f);
        } // CPU fallback when D3D11 compute is unavailable
        if(heatOn){
            // Heat visualization is a restrained thin-film reflection, not
            // an orange/brown overlay obscuring the pitch-black material.
            juce::ColourGradient heat(juce::Colour(0xff84756d).withAlpha(.085f),
                r.getX(),r.getY(),juce::Colour(0xff517983).withAlpha(.045f),
                r.getRight(),r.getBottom(),false);
            g.setGradientFill(heat);g.fillRect(r);
        }

        // Layered capillary crests follow the real FFT surface and settle
        // when the music stops; unlike rigid colored lines they bend and
        // uncoil with the damped liquid surface.
        if(!mercury3D_.hasFrame()){
        for(int layer=0;layer<3;++layer){
            juce::Path ribbon;
            bool open=false;
            for(int i=0;i<192;++i){
                const float u=(i+.5f)/192.f;
                const int bin=juce::jlimit(0,N-1,juce::roundToInt(u*(N-1)));
                const float db=surgery::displayDb(bands_[(size_t)bin],
                    prism::analysis::LabBands::centreFrequency(bin),slope,rangeDb);
                const float h=juce::jlimit(0.f,1.f,(bands_[(size_t)bin]+60.f)/56.f);
                const float top=r.getY()-db*r.getHeight()/rangeDb;
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
        // Distinct meniscus bubbles only where the measured audio is strong.
        // No permanently animated decorative droplets during silence.
        const float motion=mercuryPhase_*1.65f;
        for(int i=9;i<N-7;i+=13){
            const float t=(i+.5f)/(float)N;
            const float x=r.getX()+t*r.getWidth();
            const float actual=bands_[(size_t)i];
            if(actual < -46.f)continue;
            const float db=surgery::displayDb(actual,
                prism::analysis::LabBands::centreFrequency(i),slope,rangeDb);
            const float top=r.getY()-db*r.getHeight()/rangeDb;
            const float strength=juce::jlimit(0.f,1.f,(actual+46.f)/42.f);
            const float sway=.5f+.5f*std::sin(i*2.431f+motion*.11f);
            const float radius=(1.1f+strength*4.9f)*(.49f+.51f*sway);
            const float depth=(.18f+.58f*std::abs(std::sin(i*.71f+motion*.14f)))*
                juce::jmax(0.f,r.getBottom()-top);
            const float y=top+depth;
            if(y+radius>=r.getBottom()||y-radius<=top)continue;
            const auto syn=surgery::synesthesia::read(m,t);
            const auto tint=juce::Colour::fromFloatRGBA(syn.r,syn.g,syn.b,1.f);
            g.setColour(juce::Colour(0xff020304).withAlpha(.93f));
            g.fillEllipse(x-radius,y-radius,radius*2.1f,radius*1.8f);
            g.setColour(tint.withAlpha(.24f+.21f*strength));
            g.drawEllipse(x-radius,y-radius,radius*2.f,radius*1.7f,.85f);
            g.setColour(juce::Colour(0xfff0ede5).withAlpha(.38f+.26f*strength));
            g.fillEllipse(x-radius*.5f,y-radius*.65f,radius*.56f,radius*.24f);
        }
        } // The 3D volume already contains its own internal folds and bubbles.
    }
    // Two restrained, stationary glow layers follow ONLY the measured path.
    // Existing attack/release supplies persistence; no clock-driven shimmer,
    // independent objects or fabricated motion is introduced during silence.
    const float luminous=surgery::spectrum::brightness(*std::max_element(bands_.begin(),bands_.end()));
    if(luminous>0){
        juce::ColourGradient foil(juce::Colour(0xffd3ae8c).withAlpha(.10f*luminous),r.getX(),r.getY(),
                                  juce::Colour(0xffa08d85).withAlpha(.12f*luminous),r.getRight(),r.getY(),false);
        g.setGradientFill(foil);g.strokePath(spectrumPath,juce::PathStrokeType(4.2f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xffd9c6ae).withAlpha(.13f*luminous));
        g.strokePath(spectrumPath,juce::PathStrokeType(2.4f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
    // A single fine readout follows the spectrum edge, never a 2D waterfall.
    g.setColour(juce::Colour(xrayActive_?0xffd9ddd6:0xffc1b2a3));
    g.strokePath(spectrumPath,juce::PathStrokeType(weatherStyle_==7?1.7f:1.3f,
        juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    // A restrained peak-hold readout; the audio measurements are unchanged.
    g.setColour(juce::Colour(0xffdfd5c7).withAlpha(0.24f));
    g.strokePath(peakPath,juce::PathStrokeType(0.8f));
    if(heatOn&&!xrayActive_){
        // Narrow isolated tones have too little filled area to express the
        // heat-map toggle by tinting mercury alone (a real 4 kHz recovery
        // regression). Add tiny glows ONLY at measured spectral local peaks.
        // Nothing appears for silence or unmeasured frequencies.
        juce::Path heatBubbles;
        for(int i=1;i<N-1;++i){
            const float db=bands_[(size_t)i];
            if(db<=-85.f||db<bands_[(size_t)i-1]||db<bands_[(size_t)i+1])continue;
            const float f=prism::analysis::LabBands::centreFrequency(i);
            const float y=r.getY()-surgery::displayDb(db,f,slope,rangeDb)*r.getHeight()/rangeDb;
            if(y>=r.getBottom()-5.f)continue;
            const float x=freqToX(f);
            const float radius=7.f+3.f*juce::jlimit(0.f,1.f,(db+60.f)/60.f);
            heatBubbles.addEllipse(x-radius,y-radius,radius*2.f,radius*2.f);
        }
        g.setColour(juce::Colour(0xffdb795a).withAlpha(.34f));
        g.fillPath(heatBubbles);
        g.setColour(juce::Colour(0xffe9b887).withAlpha(.43f));
        g.strokePath(heatBubbles,juce::PathStrokeType(.85f));
    }
    g.restoreState();

    // Predict the EXACT static RBJ peak-filter response used in the CARVE DSP.
    // It shows the maximum possible response for dynamic nodes, not measured GR.
    const int selectedRoot=selected_/2;
    const double sr=processor_.getSampleRate()>8000.0?
                    processor_.getSampleRate():48000.0;
    // 0 dB of CARVE response and the frequency-only BAND handle rail are
    // the same horizontal reference. The previous +23 px offset fabricated
    // an apparently misplaced EQ line in BAND MODE.
    const float baseY=r.getCentreY();
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
        const auto tint=juce::Colour(0xffe79577).brighter(slot==slot_?.28f:0.f);
        overlayGraphics.setColour(tint.withAlpha(slot==slot_?0.20f:0.08f));
        overlayGraphics.fillPath(footprint);
        overlayGraphics.setColour(tint.withAlpha(slot==slot_?0.96f:0.58f));
        overlayGraphics.strokePath(contour,juce::PathStrokeType(slot==slot_?2.4f:1.35f));
    }
        responseOverlayKey_=overlayKey;responseOverlayValid_=true;
    }
    g.setOpacity(1.f);g.drawImage(responseOverlay_,getLocalBounds().toFloat());
    // No engineering telemetry or legacy SONAR prose over the primary lens.
    // Detailed numbers are available through the parameter shelf and hover.
    const float selectedLo=freqToX(param(selectedRoot*2,"hz"));
    const float selectedHi=freqToX(param(selectedRoot*2+1,"hz"));
    {
        juce::Graphics::ScopedSaveState selectedGlass(g);
        g.reduceClipRegion(r.toNearestInt());
        const float lo=std::min(selectedLo,selectedHi), hi=std::max(selectedLo,selectedHi);
        const float warning=peakWarnings_[(size_t)selectedRoot];
        const auto tint=warning>0.f?juce::Colour(0xffed2d22):colour(selectedRoot);
        juce::ColourGradient selection(tint.withAlpha(warning>0.f?.22f:.095f),lo,r.getY(),
            juce::Colour(0xff181718).withAlpha(.015f),hi,r.getY(),false);
        g.setGradientFill(selection);g.fillRect(lo,r.getY(),hi-lo,r.getHeight());
        g.setColour(tint.withAlpha(warning>0.f?.95f:.60f));
        g.drawVerticalLine(juce::roundToInt(freqToX(param(selected_,"hz"))),r.getY(),r.getBottom());
    }
    // Both boundary handles always live on one horizontal rail.
    g.setColour(juce::Colour(0xffa38d79).withAlpha(0.25f));
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
        // Vertical guides represent frequency boundaries, not gain movements.
        const bool focused=i/2==selectedRoot;
        const auto col=focused&&peakWarnings_[(size_t)(i/2)]>0.f?
            juce::Colour(0xffff4f38):
            colour(i/2).interpolatedWith(juce::Colour(0xffc1a997),.15f);
        g.setColour(col.withAlpha(enabled?(focused?0.68f:0.18f):0.045f));
        g.drawVerticalLine(juce::roundToInt(x),r.getY(),r.getBottom());
        g.setColour(col.withAlpha(enabled?(focused?0.24f:0.045f):0.025f));
        g.fillEllipse(x-10,y-10,20,20);
        g.setColour(enabled?col:juce::Colour(0xff6d6056));
        g.fillEllipse(x-5.5f,y-5.5f,11,11);
        if(i==selected_||solo||focused){
            g.setColour(solo?juce::Colours::white:col.brighter(0.65f));
            g.drawEllipse(x-9,y-9,18,18,i==selected_?2.0f:1.2f);
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
        // Identification is available on hover and in the CARVE shelf;
        // the reference X-RAY lens has clean nodes, never black C1-C4 tabs.
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
    // C1–C4 buttons were moved off the graph into the physical CARVE shelf.
    juce::ignoreUnused(point);
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
    menu.addSectionHeader("SCALPEL / BAND "+juce::String(root+1));
    menu.addItem(1,"BAND ENABLED",true,current("on")>=0.5f);
    const bool solo=pval(processor_.apvts,
        PRISMVSTAudioProcessor::rootsRootSoloId(root))>=0.5f;
    menu.addItem(2,"SOLO BAND / OFF",true,solo);
    menu.addSeparator();
    menu.addSectionHeader("BAND GAIN + CARVE / EQ DOMAIN");
    menu.addItem(10,"STEREO",true,juce::roundToInt(current("domain"))==0);
    menu.addItem(11,"MID",true,juce::roundToInt(current("domain"))==1);
    menu.addItem(12,"SIDE",true,juce::roundToInt(current("domain"))==2);
    menu.addSeparator();
    menu.addSectionHeader("DYNAMIC / COMPRESSOR CHANNEL");
    static const char* dn[]{"STEREO LINK","DUAL MID/SIDE","MID ONLY","SIDE ONLY"};
    for(int i=0;i<4;++i)
        menu.addItem(30+i,dn[i],true,juce::roundToInt(current("detector"))==i);
    menu.addSeparator();
    menu.addItem(15,"MONO SELECTED BAND",true,current("mono")>=0.5f);
    menu.addSeparator();
    juce::PopupMenu palette;
    static constexpr const char* names[]{"CYAN","AMBER","JADE","VIOLET","ROSE","BLUE","LIME","WHITE"};
    for(int i=0;i<8;++i)palette.addItem(80+i,names[i]);
    palette.addItem(99,"CUSTOM HEX...");
    menu.addSubMenu("BAND ACCENT",palette);
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
                    "BAND ACCENT","Enter a 6-digit RGB hexadecimal value",
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
    gainLabel_.setText("BAND GAIN (dB)",juce::dontSendNotification);
    gain_.setTooltip("Selected BAND frequency range gain: -60 to +60 dB");

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
    addAndMakeVisible(off_);
    off_.onClick=[this]{change(processor_,PRISMVSTAudioProcessor::rootsRootControlId(node_/2,"on"),0.f);timerCallback();};
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
    addAndMakeVisible(carveOff_);
    carveOff_.onClick=[this]{change(processor_,PRISMVSTAudioProcessor::carveId(node_/2,carveSlot_,"on"),0.f);timerCallback();};
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
    mono_.setTooltip("Frequency-selective MONO on the selected BAND only");
    phase_.setTooltip("Automatic per-band coherence guard: gently reduces anti-correlated SIDE power, with smoothed engagement.");
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
    // A clean Sanyo/Game Boy control face: no inherited Surgery status strip,
    // output gain fader, bypass, A/B, copy or file buttons on the chassis.
    // The associated APVTS parameters/preset code survive for DAW compatibility.
    for(auto* button:{&bypass_,&compare_,&copy_,&save_,&load_,&peakHold_})
        button->setVisible(false);
    output_.setVisible(false);outputLabel_.setVisible(false);
    bindNode(0);setMode(2);
    setResizable(true,true);setResizeLimits(840,470,1800,1200);setSize(1344,756);
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
    selection_.setText("BAND "+juce::String(node_/2+1)+
        "  /  CARVE "+juce::String(carveSlot_+1),juce::dontSendNotification);
    bindCarve();
    updateRootButtons();
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
    carveOn_.setButtonText("ON");
    for(int i=0;i<4;++i){
        carveSlotButtons_[(size_t)i].setToggleState(i==slot,juce::dontSendNotification);
        carveSlotButtons_[(size_t)i].setColour(juce::TextButton::buttonOnColourId,
            juce::Colour(0xffc6a182));
    }
    graph_.setPresentation(mode_,carveSlot_);
}
void RootsEditor::setMode(int next){
    mode_=juce::jlimit(0,3,next);
    for(int i=0;i<4;++i){
        auto& b=modeButtons_[(size_t)i];
        b.setToggleState(i==mode_,juce::dontSendNotification);
        b.setColour(juce::TextButton::buttonColourId,juce::Colour(0xffe4cdb4));
        b.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffbba084));
        b.setColour(juce::TextButton::textColourOffId,juce::Colour(0xff241812));
        b.setColour(juce::TextButton::textColourOnId,juce::Colour(0xff241812));
    }
    const bool carve=mode_==1;
    for(auto* s:{&carveFrequency_,&carveDepth_,&carveQ_})s->setVisible(carve);
    for(auto* l:{&carveFrequencyLabel_,&carveDepthLabel_,&carveQLabel_})l->setVisible(carve);
    for(auto& b:carveSlotButtons_)b.setVisible(carve);
    carveOn_.setVisible(carve);carveOff_.setVisible(carve);
    // COLOR DYNAMIC is the sole visible transient-contrast control. Keep the
    // historical CARVE dynamic parameter for preset compatibility only.
    carveDynamic_.setVisible(false);
    // Legacy SONAR / JUKE settings stay automatable and preset-compatible,
    // but aren't injected into the redesigned 3-control ANALYZE shelf.
    analyzerLegend_.setVisible(false);
    weatherMode_.setVisible(false);jukeMode_.setVisible(false);jukeModeLabel_.setVisible(false);reducedMotion_.setVisible(false);
    for(auto* box:{&compressor_,&domain_,&detector_})box->setVisible(mode_==0);
    for(auto* label:{&compLabel_,&domainLabel_,&detectorLabel_})label->setVisible(mode_==0);
    gain_.setVisible(mode_==0);gainLabel_.setVisible(mode_==0);
    threshold_.setVisible(mode_==0);
    thresholdLabel_.setVisible(mode_==0);
    // LIMIT is a true single-button control; its old amount rotary is hidden.
    limitAmount_.setVisible(false);
    phase_.setVisible(mode_==0);
    limit_.setVisible(mode_==0);
    for(int i=0;i<5;++i){
        onyxSliders_[(size_t)i].setVisible(mode_==2);
        onyxLabels_[(size_t)i].setVisible(mode_==2);
    }
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
    carveOn_.setButtonText("ON");
    const bool bandOn=pval(processor_.apvts,
        PRISMVSTAudioProcessor::rootsRootControlId(node_/2,"on"))>=.5f;
    off_.setToggleState(!bandOn,juce::dontSendNotification);
    carveOff_.setToggleState(pval(processor_.apvts,
        PRISMVSTAudioProcessor::carveId(node_/2,carveSlot_,"on"))<.5f,
        juce::dontSendNotification);
    heatToggle_.setButtonText("HEAT MAP");
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
    // A visual separation from Surgery EQ is intentional: its external
    // cyan/purple JUKE rails are not part of Scalpel's 1980s hardware face.
}
void RootsEditor::resized(){rackBackground_={};backgroundScale_=0;layoutRack();repaint();}

void RootsEditor::paintRack(juce::Graphics& g){
    const int width=getWidth(),height=getHeight();
    g.fillAll(juce::Colour(0xff382b22));
    const auto outer=getLocalBounds().toFloat().reduced(1.f);
    juce::ColourGradient plastic(juce::Colour(0xfff7e3ca),0,0,
                                 juce::Colour(0xffc7a486),0,(float)height,false);
    plastic.addColour(.27,juce::Colour(0xffe6cdb0));
    plastic.addColour(.72,juce::Colour(0xffd9bda2));
    g.setGradientFill(plastic);g.fillRoundedRectangle(outer,12.f);
    g.setColour(juce::Colour(0xfffff0d9).withAlpha(.86f));
    g.drawRoundedRectangle(outer.reduced(2.f),10.f,1.4f);
    g.setColour(juce::Colour(0xff5b4534).withAlpha(.78f));
    g.drawRoundedRectangle(outer.reduced(5.f),8.f,.8f);
    // Subpixel molded-powder grain, cached in the editor background.
    juce::Random grain(50219);
    for(int i=0;i<680;++i){
        const float x=grain.nextFloat()*width,y=grain.nextFloat()*height;
        g.setColour(juce::Colour(0xff705743).withAlpha(.014f+grain.nextFloat()*.018f));
        g.fillEllipse(x,y,.65f,.65f);
    }
    // Recessed black Phillips fasteners, reference-equivalent and static.
    auto screw=[&](float cx,float cy){
        g.setColour(juce::Colour(0xff705542));g.fillEllipse(cx-13,cy-13,26,26);
        g.setColour(juce::Colour(0xff07090a));g.fillEllipse(cx-11.5f,cy-11.5f,23,23);
        juce::ColourGradient steel(juce::Colour(0xff4c4e4d),cx-7,cy-9,
                                   juce::Colour(0xff090b0c),cx+9,cy+8,false);
        g.setGradientFill(steel);g.fillEllipse(cx-9.2f,cy-9.2f,18.4f,18.4f);
        g.setColour(juce::Colour(0xff010202));
        g.drawLine(cx-5.3f,cy,cx+5.3f,cy,3.5f);
        g.drawLine(cx,cy-5.3f,cx,cy+5.3f,3.5f);
        g.setColour(juce::Colour(0xffaaaaa4).withAlpha(.24f));
        juce::Path rimArc;rimArc.addCentredArc(cx,cy,8.2f,8.2f,0.f,3.5f,5.0f,true);
        g.strokePath(rimArc,juce::PathStrokeType(1.f));
    };
    screw(19,19);screw((float)width-19,19);
    screw(19,(float)height-19);screw((float)width-19,(float)height-19);
    juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(surgery::rack::contentTransform(width,height));
    auto deck=[&](juce::Rectangle<float> r,bool dark){
        g.setColour(juce::Colour(0xff4c3529).withAlpha(.82f));
        g.fillRoundedRectangle(r.expanded(1.5f),5.f);
        juce::ColourGradient face(dark?juce::Colour(0xff141311):juce::Colour(0xffedddc7),
            r.getX(),r.getY(),dark?juce::Colour(0xff050504):juce::Colour(0xffd5b596),
            r.getX(),r.getBottom(),false);
        g.setGradientFill(face);g.fillRoundedRectangle(r,4.f);
        g.setColour(juce::Colour(dark?0xff987b61:0xffffefda).withAlpha(.8f));
        g.drawRoundedRectangle(r.reduced(.8f),3.f,1.f);
    };
    // Wide, flush, interlocking rails match the approved Scalpel render.
    deck({37,48,1046,46},false);
    deck({18,91,1084,352},true);
    deck({32,444,1056,34},false);
    deck({32,476,1056,143},false);
    // Inset dark joints dividing the mechanical control shelf only.
    if(mode_==0){
        for(float x:{183.f,358.f,802.f}){
            g.setColour(juce::Colour(0xff735944).withAlpha(.6f));
            g.drawLine(x,477,x,600,.9f);
        }
    } else if(mode_==1){
        for(float x:{280.f,867.f}){
            g.setColour(juce::Colour(0xff735944).withAlpha(.6f));
            g.drawLine(x,477,x,600,.9f);
        }
    } else if(mode_==3){
        for(float x:{384.f,736.f}){
            g.setColour(juce::Colour(0xff735944).withAlpha(.6f));
            g.drawLine(x,477,x,600,.9f);
        }
    }
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
    title_.setBounds(54,8,395,38);
    for(int root=0;root<7;++root)
        rootButtons_[(size_t)root].setBounds(42+root*149,51,146,37);
    graph_.setBounds(20,94,1080,347);
    for(int i=0;i<4;++i)modeButtons_[(size_t)i].setBounds(35+i*263,447,260,28);
    xrayButton_.setBounds(945,112,118,40);

    // BAND MODE / the precise two-knob, two-menu, four-button deck.
    on_.setBounds(53,505,120,36);off_.setBounds(53,549,120,36);
    gainLabel_.setBounds(195,484,140,20);gain_.setBounds(200,499,130,91);
    thresholdLabel_.setBounds(352,484,150,20);threshold_.setBounds(360,499,135,91);
    compLabel_.setBounds(521,482,204,19);compressor_.setBounds(522,503,204,31);
    domainLabel_.setBounds(521,535,204,19);domain_.setBounds(522,556,204,32);
    detectorLabel_.setBounds(728,482,75,19);detector_.setBounds(728,502,75,30);
    // The fourth switch block now meets the compression selectors directly.
    // No stray LIMIT amount/third dial is painted beside MONO.
    limitAmount_.setBounds(0,0,1,1);
    mono_.setBounds(788,504,133,39);solo_.setBounds(930,504,133,39);
    phase_.setBounds(788,550,133,39);limit_.setBounds(930,550,133,39);

    // CARVE / real C1-C4 selector panel, two-state switches and three dials.
    selection_.setBounds(56,482,230,20);
    carveOn_.setBounds(71,519,188,39);
    carveOff_.setBounds(71,565,188,39);
    carveDynamic_.setBounds(0,0,1,1);
    const std::array<juce::Slider*,3> cs{&carveFrequency_,&carveDepth_,&carveQ_};
    const std::array<juce::Label*,3> cl{&carveFrequencyLabel_,&carveDepthLabel_,&carveQLabel_};
    for(int i=0;i<3;++i){
        cl[(size_t)i]->setBounds(299+i*194,482,182,20);
        cs[(size_t)i]->setBounds(307+i*194,497,164,94);
    }
    for(int i=0;i<4;++i)
        carveSlotButtons_[(size_t)i].setBounds(880+(i%2)*96,504+(i/2)*49,90,39);

    // COLOR / five exact user-provided ivory dial assets, no tick marks.
    for(int i=0;i<5;++i){
        onyxLabels_[(size_t)i].setBounds(42+i*210,483,196,20);
        onyxSliders_[(size_t)i].setBounds(67+i*210,499,146,94);
    }

    // ANALYZE / centered sculpted flame, snowflake and visual-decay dial.
    analyzerDepthLabel_.setBounds(106,482,220,22);
    analyzerSlopeLabel_.setBounds(447,482,220,22);
    analyzerMemoryLabel_.setBounds(785,482,220,22);
    heatToggle_.setBounds(174,510,85,85);freezeButton_.setBounds(515,510,85,85);
    analyzerMemory_.setBounds(828,501,135,93);
    // Preserved automation settings have no legacy front-panel decoration.
    jukeModeLabel_.setBounds(12,0,10,10);jukeMode_.setBounds(12,0,10,10);
    weatherMode_.setBounds(0,0,10,10);reducedMotion_.setBounds(24,0,10,10);
    analyzerDepth_.setBounds(0,0,10,10);analyzerSlope_.setBounds(0,0,10,10);
    analyzerLegend_.setBounds(0,0,10,10);status_.setBounds(0,0,10,10);

    // Compact secondary utility strip below the visual reference shelf.
    bypass_.setBounds(47,605,75,20);compare_.setBounds(128,605,75,20);
    copy_.setBounds(209,605,92,20);save_.setBounds(307,605,65,20);
    load_.setBounds(378,605,65,20);peakHold_.setBounds(449,605,119,20);
    outputLabel_.setBounds(788,604,60,21);output_.setBounds(850,604,219,21);
}

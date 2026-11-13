#pragma once
#include <JuceHeader.h>
#include "../Source/ScalpelReferenceLayout.h"
#include <iostream>
#include <fstream>
#if JUCE_WINDOWS && JUCE_PLUGINHOST_VST3
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Loads the built VST3 in a real JUCE host. Captures native window pixels,
// including its embedded HWND; Component snapshots alone miss hosted HWNDs.
inline bool captureScalpelModule(const juce::String& modulePath){
    const auto pump=[](int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);};
    juce::AudioPluginFormatManager formats;formats.addFormat(new juce::VST3PluginFormat());
    juce::OwnedArray<juce::PluginDescription> types;
    formats.getFormat(0)->findAllTypesForFile(types,modulePath);
    if(types.isEmpty()){
        const auto bundle=juce::File(modulePath).getParentDirectory().getParentDirectory().getParentDirectory();
        formats.getFormat(0)->findAllTypesForFile(types,bundle.getFullPathName());
    }
    if(types.isEmpty()){std::cerr<<"MODULE CAPTURE: VST3 scan failed\n";return false;}
    juce::String error;auto plugin=formats.createPluginInstance(*types[0],48000,512,error);
    if(!plugin){std::cerr<<"MODULE CAPTURE: "<<error<<'\n';return false;}
    plugin->prepareToPlay(48000,512);
    const auto parameter=[&](const juce::String& name)->juce::AudioProcessorParameter*{
        for(auto* p:plugin->getParameters())if(p->getName(128)==name)return p;
        return nullptr;
    };
    const auto setText=[&](const juce::String& name,const juce::String& value){
        auto* p=parameter(name);if(!p){std::cerr<<"MODULE CAPTURE: missing "<<name<<'\n';return false;}
        p->beginChangeGesture();p->setValueNotifyingHost(p->getValueForText(value));p->endChangeGesture();return true;
    };
    if(!setText("CARVE 3/4 Frequency","215.87")||!setText("CARVE 3/4 Cut Depth","0.00")||!setText("CARVE 3/4 Q","0.300"))return false;
    auto* enabled=parameter("CARVE 3/4 Enabled");auto* xray=parameter("X-RAY Visualizer");
    if(!enabled||!xray)return false;
    enabled->setValueNotifyingHost(1.f);xray->setValueNotifyingHost(1.f);
    if(std::abs(parameter("CARVE 3/4 Frequency")->getCurrentValueAsText().getFloatValue()-215.87f)>.02f||
       std::abs(parameter("CARVE 3/4 Q")->getCurrentValueAsText().getFloatValue()-.3f)>.001f)return false;
    // CI-owned virtual display only. A full-size native capture must not be
    // truncated by an undersized server desktop. Restore its mode on exit.
    struct DisplayMode {
        DEVMODEW original{};bool changed=false;
        DisplayMode(){
            original.dmSize=sizeof(original);
            if(!EnumDisplaySettingsW(nullptr,ENUM_CURRENT_SETTINGS,&original))return;
            if(original.dmPelsWidth>=1424&&original.dmPelsHeight>=836)return;
            DEVMODEW next=original;next.dmPelsWidth=1920;next.dmPelsHeight=1080;
            next.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT;
            changed=ChangeDisplaySettingsW(&next,0)==DISP_CHANGE_SUCCESSFUL;
            if(!changed){
                for(DWORD i=0;i<256;++i){
                    DEVMODEW candidate{};candidate.dmSize=sizeof(candidate);
                    if(!EnumDisplaySettingsW(nullptr,i,&candidate))break;
                    if(candidate.dmPelsWidth<1424||candidate.dmPelsHeight<836)continue;
                    if(ChangeDisplaySettingsW(&candidate,0)==DISP_CHANGE_SUCCESSFUL){changed=true;break;}
                }
            }
        }
        ~DisplayMode(){if(changed)ChangeDisplaySettingsW(&original,0);}
    } display;
    std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorIfNeeded());
    if(!editor){std::cerr<<"MODULE CAPTURE: VST3 has no editor\n";return false;}
    editor->setSize(1344,756);editor->addToDesktop(0);editor->setTopLeftPosition(30,30);editor->setVisible(true);pump(300);
    auto* peer=editor->getPeer();if(!peer)return false;
    const auto window=(HWND)peer->getNativeHandle();SetForegroundWindow(window);pump(120);
    const auto click=[&](double x,double y){
        RECT rect{};GetClientRect(window,&rect);POINT origin{0,0};ClientToScreen(window,&origin);
        SetCursorPos(origin.x+(int)std::lround(x*(rect.right-rect.left)/1120.),
                     origin.y+(int)std::lround(y*(rect.bottom-rect.top)/630.));
        INPUT input[2]{};for(auto& i:input)i.type=INPUT_MOUSE;
        input[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;input[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        if(SendInput(2,input,sizeof(INPUT))!=2)return false;
        pump(100);return true;
    };
    const auto capture=[&](const juce::String& name){
        RECT r{};GetClientRect(window,&r);POINT origin{0,0};ClientToScreen(window,&origin);
        const int width=r.right-r.left,height=r.bottom-r.top;
        if(width!=1344||height!=756){std::cerr<<"MODULE CAPTURE: native client size "<<width<<'x'<<height<<'\n';return false;}
        SetCursorPos(origin.x+110,origin.y+45);pump(40);
        HDC screen=GetDC(nullptr),memory=CreateCompatibleDC(screen);void* bytes=nullptr;
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bytes,nullptr,0);
        if(!bitmap||!memory||!bytes){if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);ReleaseDC(nullptr,screen);return false;}
        const auto old=SelectObject(memory,bitmap);
        const bool copied=BitBlt(memory,0,0,width,height,screen,origin.x,origin.y,SRCCOPY|CAPTUREBLT)!=0;
        GdiFlush();
        auto image=juce::SoftwareImageType{}.convert(juce::Image(juce::Image::RGB,width,height,true));
        if(copied){
            juce::Image::BitmapData pixels(image,juce::Image::BitmapData::writeOnly);
            const auto* bgra=static_cast<const unsigned char*>(bytes);
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                const auto* c=bgra+4*(y*width+x);pixels.setPixelColour(x,y,juce::Colour(c[2],c[1],c[0]));
            }
        }
        SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(nullptr,screen);
        if(!copied)return false;
        // A native child that did not paint produces a blank capture.
        int lit=0;for(int y=0;y<height;y+=4)for(int x=0;x<width;x+=4)
            if(image.getPixelAt(x,y).getPerceivedBrightness()>.18f)++lit;
        // A single pixel at 1200,118 intersects the larger BAND7 glyphs.
        // Require a substantial painted ivory key surface, independent of text.
        int paintedKey=0;
        const auto key=scalpel::reference::band(6).toFloat().transformedBy(juce::AffineTransform::scale(1.2f)).toNearestInt().reduced(6);
        for(int y=key.getY();y<key.getBottom();++y)for(int x=key.getX();x<key.getRight();++x){
            const auto c=image.getPixelAt(x,y);
            if(c.getRed()>140&&c.getGreen()>120&&c.getBlue()>100&&c.getRed()>c.getBlue()+8)++paintedKey;
        }
        if(lit<1500||paintedKey<1600){
            juce::FileOutputStream failure(juce::File::getCurrentWorkingDirectory().getChildFile("surgery-ui-VST3-capture-failure.png"));
            juce::PNGImageFormat().writeImageToStream(image,failure);
            std::cerr<<"MODULE CAPTURE: blank/clipped native window; lit="<<lit<<" ivory key="<<paintedKey<<'\n';return false;
        }
        juce::FileOutputStream out(juce::File::getCurrentWorkingDirectory().getChildFile(name));out.setPosition(0);out.truncate();
        return juce::PNGImageFormat().writeImageToStream(image,out);
    };
    const auto hit=[&](juce::Rectangle<int> box){return click(box.getCentreX(),box.getCentreY());};
    using namespace scalpel::reference;
    bool ok=hit(band(2))&&hit(tab(1))&&hit(carveSlot(3))&&capture("surgery-ui-VST3-CARVE-1344x756.png");
    ok=ok&&hit(switchBounds(false,true))&&capture("surgery-ui-VST3-CARVE-ON.png");
    ok=ok&&hit(switchBounds(true,true))&&capture("surgery-ui-VST3-CARVE-OFF.png");
    ok=ok&&hit(tab(2))&&capture("surgery-ui-VST3-COLOR-1344x756.png");
    ok=ok&&hit(tab(0))&&capture("surgery-ui-VST3-BAND-1344x756.png");
    ok=ok&&hit(switchBounds(false))&&capture("surgery-ui-VST3-BAND-ON.png");
    ok=ok&&hit(switchBounds(true))&&capture("surgery-ui-VST3-BAND-OFF.png");
    // Reference BAND fixture: native client pixels, BAND1/ON, digital silence.
    ok=ok&&hit(band(0))&&hit(switchBounds(false))&&capture("surgery-ui-VST3-IVORY-REFERENCE-BAND.png");
    // An additional known input proves that FFT and meters remain audio-driven.
    juce::Random noise(1071645);juce::AudioBuffer<float> block(2,512);juce::MidiBuffer midi;
    float bass=0;
    for(int n=0;n<80;++n){
        for(int i=0;i<512;++i){bass+=.025f*((noise.nextFloat()*2-1)-bass);
            const float sample=.65f*bass+.035f*std::sin((n*512+i)*juce::MathConstants<float>::twoPi*110.f/48000.f);
            block.setSample(0,i,sample);block.setSample(1,i,sample*.85f);}
        plugin->processBlock(block,midi);if(n%4==3)pump(12);
    }
    ok=ok&&capture("surgery-ui-VST3-IVORY-BAND-AUDIO.png");
    ok=ok&&hit(tab(3))&&capture("surgery-ui-VST3-ANALYZE-1344x756.png");
    // Actual compiled pointer/readout calibration. These are live host values,
    // not artwork or replacement waveforms. Restore the capture fixture below.
    ok=ok&&hit(band(2))&&hit(tab(0));
    for(int p=0;p<=2;++p){
        ok=ok&&setText("ROOT 3 Compression Threshold",juce::String(-30*p));pump(80);
        ok=ok&&capture("surgery-ui-VST3-CALIBRATION-THRESHOLD-"+juce::String(p*50)+".png");
    }
    ok=ok&&setText("ROOT 3 Compression Threshold","0")&&hit(tab(1))&&hit(carveSlot(3));
    for(int p=0;p<=2;++p){
        const double q=.3*std::pow(96./.3,p*.5);
        ok=ok&&setText("CARVE 3/4 Q",juce::String(q,6));pump(80);
        ok=ok&&capture("surgery-ui-VST3-CALIBRATION-TIGHTNESS-"+juce::String(p*50)+".png");
    }
    ok=ok&&setText("CARVE 3/4 Q","0.300");
    std::ofstream evidence("surgery-vst3-capture.csv");
    evidence<<"version,"<<types[0]->version<<"\n";
    evidence<<"module,"<<modulePath<<"\neditor,1344x756\nfrequency,"<<parameter("CARVE 3/4 Frequency")->getCurrentValueAsText()<<"\nq,"<<parameter("CARVE 3/4 Q")->getCurrentValueAsText()<<"\n";
    editor->setVisible(false);editor->removeFromDesktop();editor.reset();plugin->releaseResources();
    if(ok)std::cout<<"MODULE CAPTURE: built VST3 loaded in JUCE host; native CARVE/COLOR/BAND/ANALYZE captured at 1344x756; fixture preserved\n";
    return ok;
}
#endif



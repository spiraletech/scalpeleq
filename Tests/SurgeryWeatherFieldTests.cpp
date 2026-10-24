#include <JuceHeader.h>
#include "../Source/SurgeryWeatherField.h"
#include "../Source/SurgeryVisualModes.h"
#include <iostream>
#include <fstream>
void startAllocationProbe() noexcept;
std::size_t finishAllocationProbe() noexcept;
bool runWeatherFieldTests(){
    using namespace surgery::weather;
    auto fail=[](const char* text){std::cerr<<"WEATHER FIELD FAIL: "<<text<<'\n';return false;};
    for(float c:{0.f,.003f,.04f,.2f,.5f,1.f})if(std::abs(srgb(linear(c))-c)>.00001)return fail("linear-light roundtrip");
    Snapshot input;input.analysisPower=.04f;input.level=.85f;input.bass=.8f;input.centroid=.32f;input.width=.65f;input.flux=.1f;input.density=.45f;input.rootIntensity.fill(.8f);input.boundaries={20,60,60,120,120,250,250,500,500,2000,2000,6000,6000,20000};
    std::array<float,3> density{};int index=0;
    for(int fps:{30,60,120}){
        Interpretation meaning;Field field;
        for(int frame=0;frame<fps*2;++frame){input.seconds=frame/(double)fps;meaning.accept(input);field.update(meaning,1./fps,.18,true);}
        float mass=0;int colour=0;for(const auto& cell:field.cells()){mass+=cell.density;if(cell.vx!=0||cell.vy!=0||cell.x!=0||cell.y!=0||cell.illumination!=0)return fail("reduced motion");}
        for(const auto& pixel:field.pixels())colour+=pixel.r>20||pixel.g>25||pixel.b>32?1:0;
        density[(size_t)index++]=mass/Field::count;if(colour<1000)return fail("field not visibly responsive");
        Snapshot silence;
        for(int frame=0;frame<fps*15;++frame){meaning.accept(silence);field.update(meaning,1./fps,.18,true);}
        for(const auto& cell:field.cells())if(cell.density>.00002||cell.illumination!=0)return fail("silence does not settle");
        if(field.update(meaning,1./fps,.18,true))return fail("settled idle redraw");
    }
    if(std::abs(density[0]-density[2])>.01)return fail("frame-rate dependent density");
    Interpretation meaning;Field field;
    float previousFlash=0;double lastFlash=-100;int flashes=0;
    for(int frame=0;frame<600;++frame){
        input.event=frame+1;input.eventSeconds=input.seconds=frame*.01;input.eventStrength=1;input.eventPosition=.4f;
        meaning.accept(input);field.update(meaning,.01,.18,false);
        const auto flash=meaning.state().flash;
        if(flash>previousFlash+.002f){if(frame*.01-lastFlash<.749)return fail("visual flash frequency");lastFlash=frame*.01;++flashes;}
        previousFlash=flash;
        for(const auto& cell:field.cells())if(!std::isfinite(cell.density)||!std::isfinite(cell.vx)||!std::isfinite(cell.x)||cell.density<0||cell.density>1||cell.illumination>.10001||std::abs(cell.x)>.08001||std::abs(cell.y)>.08001)return fail("unbounded physics");
        if(frame%10==0)meaning.quiet(); // automation cannot reset flash refractory time
    }
    if(flashes<2||flashes>9)return fail("transient flash counts");
    meaning.accept(input);field.update(meaning,.016,.18,true);if(meaning.state().flash!=0)return fail("reduced motion does not suppress lightning");
    auto a=std::make_unique<Field>(),b=std::make_unique<Field>();Interpretation ia,ib;input.event=0;
    for(int i=0;i<60;++i){ia.accept(input);ib.accept(input);a->update(ia,.016,.4,false);b->update(ib,.016,.4,false);}
    const auto& pa=a->pixels();const auto& pb=b->pixels();for(size_t i=0;i<pa.size();++i)if(pa[i].r!=pb[i].r||pa[i].g!=pb[i].g||pa[i].b!=pb[i].b)return fail("nondeterministic spatial field");
    Interpretation stalled;Field stalledField;stalled.accept(input);stalledField.update(stalled,.016,.18,false);
    stalledField.update(stalled,30.,.18,false);if(stalled.state().level>.00001)return fail("stalled UI ignores elapsed silence");
    for(int i=0;i<1000;++i)stalledField.update(stalled,.1,10,false);
    if(stalled.state().level!=0||stalled.state().flash!=0)return fail("silent subnormal interpretation tail");
    startAllocationProbe();
    for(int i=0;i<100;++i){ia.accept(input);a->update(ia,1./60.,.18,false);}
    const auto allocations=finishAllocationProbe();if(allocations!=0)return fail("field update allocates");
    auto shared=meaning.state();shared.level=.8f;shared.bass=.6f;shared.centroid=.55f;shared.width=.65f;shared.flux=.7f;shared.crest=.6f;shared.musicEnergy={.8f,.6f,.7f,.5f};
    std::array<std::uint64_t,8> hashes{};
    for(int mode=0;mode<8;++mode){std::uint64_t h=1469598103934665603ull;
        for(int y=0;y<64;++y)for(int x=0;x<128;++x){const auto c=surgery::visual::style(mode,shared,.7f,x/127.f,y/63.f,field.pixels()[(size_t)(y*128+x)]);h^=(c.r<<16)|(c.g<<8)|c.b;h*=1099511628211ull;}
        hashes[(size_t)mode]=h;if(mode>1&&h!=hashes[1])return fail("retired styles must alias Standard+");
    }
    std::ofstream report("surgery-weather-field.csv");report<<"field_bytes,interpretation_bytes,measurement_bytes,queue_bytes,cells,flash_limit_hz,density_30fps,density_60fps,density_120fps\n"<<sizeof(Field)<<','<<sizeof(Interpretation)<<','<<sizeof(Measurement)<<','<<sizeof(SnapshotQueue<Snapshot>)<<','<<Field::count<<','<<1/.75<<','<<density[0]<<','<<density[1]<<','<<density[2]<<'\n';
    std::cout<<"WEATHER FIELD: deterministic approved-palette field, Standard+ legacy aliases, 30/60/120 Hz physics, bounded field, silence/idle, reduced motion and hard flash rate limit passed; fixed field bytes "<<sizeof(Field)<<'\n';
    return true;
}

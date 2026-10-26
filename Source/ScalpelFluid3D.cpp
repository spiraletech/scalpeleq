#include "ScalpelFluid3D.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#endif

namespace scalpel::xray {

#if defined(_WIN32)
namespace {
using Microsoft::WRL::ComPtr;
constexpr unsigned GX = FluidVolume3D::gridWidth;
constexpr unsigned GY = FluidVolume3D::gridHeight;
constexpr unsigned GZ = FluidVolume3D::gridDepth;
constexpr unsigned IW = FluidVolume3D::imageWidth;
constexpr unsigned IH = FluidVolume3D::imageHeight;

// Every pass operates on a real 3D grid. The velocity is semi-Lagrangian
// advected and projected to approximately divergence-free flow by solving
// a pressure Poisson equation. Two transported phase fractions provide a
// preliminary gas/liquid/heavy-fluid model; this is not yet a validated VOF
// interface-reconstruction solver.
constexpr const char* fluidHlsl = R"HLSL(
#define GX 64
#define GY 40
#define GZ 32
#define IW ((uint)control.z)
#define IH ((uint)control.w)
cbuffer Parameters : register(b0) {
    float4 dynamics;   // dt, bass, flux, overall level
    float4 control;    // elapsed phase, reduced-motion flag, unused, unused
    float4 spectral[64]; // local energy and transported optical source RGB
};
Texture3D<float4> velocityIn : register(t0);
Texture3D<float4> phasesIn : register(t1);
Texture3D<float> pressureIn : register(t2);
Texture3D<float> divergenceIn : register(t3);
SamplerState linearClamp : register(s0);
RWTexture3D<float4> velocityOut : register(u0);
RWTexture3D<float4> phasesOut : register(u1);
RWTexture3D<float> scalarOut : register(u2);
RWTexture2D<float4> imageOut : register(u3);

bool within(int3 p) {
    return all(p >= int3(0,0,0)) && all(p < int3(GX,GY,GZ));
}
int3 bounded(int3 p) {
    return clamp(p,int3(0,0,0),int3(GX-1,GY-1,GZ-1));
}
float3 vel(int3 p) {
    return within(p) ? velocityIn.Load(int4(p,0)).xyz : float3(0,0,0);
}
float pressure(int3 p) {
    return pressureIn.Load(int4(bounded(p),0));
}
float4 phase(int3 p) {
    return phasesIn.Load(int4(bounded(p),0));
}
float3 uvw(float3 p) {
    return saturate(p / float3(GX,GY,GZ));
}
[numthreads(8,8,4)]
void AdvectVelocity(uint3 id : SV_DispatchThreadID) {
    if(any(id>=uint3(GX,GY,GZ))) return;
    int3 p=int3(id);
    if(any(id==uint3(0,0,0)) || id.x==GX-1 || id.y==GY-1 || id.z==GZ-1) {
        velocityOut[id]=float4(0,0,0,0);
        return;
    }
    float dt=dynamics.x;
    float3 current=vel(p);
    float3 back=(float3(id)+0.5)-current*dt;
    float3 advected=velocityIn.SampleLevel(linearClamp,uvw(back),0).xyz;
    float4 layers=phase(p);
    float strength=spectral[id.x].x;
    float heavy=layers.x,light=layers.y;
    float mixViscosity=lerp(0.82,2.6,saturate(heavy));
    float3 force=float3(0,0,0);
    float signedWave=sin(control.x*0.71+id.x*0.27+id.z*0.21);
    force.x+=0.35*strength*signedWave+0.11*(light-heavy);
    force.y+=0.90*dynamics.y*strength*(0.3+0.7*saturate((float)GY-id.y)/GY)
           +0.45*dynamics.z*strength*(0.5+0.5*signedWave)
           +0.62*light-0.35*heavy;
    force.z+=0.32*strength*cos(control.x*0.46+id.y*0.18+id.x*0.12);
    // Finite-difference phase-gradient surface-tension approximation.
    float3 grad=float3(
        phase(p+int3(1,0,0)).x-phase(p-int3(1,0,0)).x,
        phase(p+int3(0,1,0)).x-phase(p-int3(0,1,0)).x,
        phase(p+int3(0,0,1)).x-phase(p-int3(0,0,1)).x)*0.5;
    float lap=phase(p+int3(1,0,0)).x+phase(p-int3(1,0,0)).x
             +phase(p+int3(0,1,0)).x+phase(p-int3(0,1,0)).x
             +phase(p+int3(0,0,1)).x+phase(p-int3(0,0,1)).x-6*heavy;
    force+=-0.18*lap*grad;
    // Local circulation: advected curl provides restrained 3D turbulence.
    float3 vXP=vel(p+int3(1,0,0)),vXM=vel(p-int3(1,0,0));
    float3 vYP=vel(p+int3(0,1,0)),vYM=vel(p-int3(0,1,0));
    float3 vZP=vel(p+int3(0,0,1)),vZM=vel(p-int3(0,0,1));
    float3 curl=0.5*float3(vYP.z-vYM.z-vZP.y+vZM.y,
                           vZP.x-vZM.x-vXP.z+vXM.z,
                           vXP.y-vXM.y-vYP.x+vYM.x);
    force+=0.095*curl*strength;
    float motion=control.y>0.5?0.40:1.0;
    float3 next=advected*exp(-mixViscosity*dt)+force*(dt*motion);
    velocityOut[id]=float4(clamp(next,-2.5,2.5),0);
}
[numthreads(8,8,4)]
void AdvectPhases(uint3 id : SV_DispatchThreadID) {
    if(any(id>=uint3(GX,GY,GZ))) return;
    float dt=dynamics.x;
    float3 velocity=vel(int3(id));
    float3 origin=(float3(id)+0.5)-velocity*dt;
    float4 q=phasesIn.SampleLevel(linearClamp,uvw(origin),0);
    float4 sound=spectral[id.x];
    // Material must be visible promptly after a cold X-RAY opening.
    // Interleaved phases are transported, but not yet a validated VOF solver.
    float floorWeight=exp(-float(id.y)*0.061);
    float depthPattern=0.65+0.35*sin(float(id.z)*0.27+control.x*0.30);
    float adjacent=spectral[max(0,(int)id.x-1)].x+spectral[min(63,(int)id.x+1)].x;
    float strength=max(sound.x,adjacent*0.24)*floorWeight*depthPattern;
    float source=strength*(0.037+0.035*dynamics.z+0.026*dynamics.w);
    float heavy=saturate(q.x*0.998+source*0.91);
    float light=saturate(q.y*0.993+source*(0.58+0.16*sound.z));
    float total=heavy+light;
    if(total>0.96) {heavy*=0.96/total;light*=0.96/total;}
    float pigment=saturate(q.z*0.994+source*(0.70+0.12*sound.y+0.12*sound.z));
    float bubbles=saturate(q.w*0.978+source*(0.08+0.55*dynamics.z+0.14*dynamics.y));
    if(id.y>=GY-2) bubbles*=0.8;
    phasesOut[id]=float4(heavy,light,pigment,bubbles);
}
[numthreads(8,8,4)]
void Divergence(uint3 id : SV_DispatchThreadID) {
    if(any(id>=uint3(GX,GY,GZ))) return;
    int3 p=int3(id);
    float d=0.5*(vel(p+int3(1,0,0)).x-vel(p-int3(1,0,0)).x
                +vel(p+int3(0,1,0)).y-vel(p-int3(0,1,0)).y
                +vel(p+int3(0,0,1)).z-vel(p-int3(0,0,1)).z);
    scalarOut[id]=d;
}
[numthreads(8,8,4)]
void PressureJacobi(uint3 id : SV_DispatchThreadID) {
    if(any(id>=uint3(GX,GY,GZ))) return;
    int3 p=int3(id);
    float sum=pressure(p+int3(1,0,0))+pressure(p-int3(1,0,0))
             +pressure(p+int3(0,1,0))+pressure(p-int3(0,1,0))
             +pressure(p+int3(0,0,1))+pressure(p-int3(0,0,1));
    float d=divergenceIn.Load(int4(p,0));
    scalarOut[id]=(sum-d)/6.0;
}
[numthreads(8,8,4)]
void ProjectVelocity(uint3 id : SV_DispatchThreadID) {
    if(any(id>=uint3(GX,GY,GZ))) return;
    if(any(id==uint3(0,0,0)) || id.x==GX-1 || id.y==GY-1 || id.z==GZ-1) {
        velocityOut[id]=float4(0,0,0,0);
        return;
    }
    int3 p=int3(id);
    float3 grad=0.5*float3(pressure(p+int3(1,0,0))-pressure(p-int3(1,0,0)),
                           pressure(p+int3(0,1,0))-pressure(p-int3(0,1,0)),
                           pressure(p+int3(0,0,1))-pressure(p-int3(0,0,1)));
    velocityOut[id]=float4(clamp(vel(p)-grad,-2.5,2.5),0);
}
[numthreads(8,8,1)]
void RenderVolume(uint3 id : SV_DispatchThreadID) {
    if(id.x>=IW||id.y>=IH)return;
    float2 uv=(float2(id.xy)+0.5)/float2(IW,IH);
    float x=uv.x;
    float y=1-uv.y;
    float3 acc=float3(0.0018,0.0025,0.004);
    float transparency=1.0;
    float4 note=spectral[min(63,(uint)(x*64))];
    // Derived exclusively from measured bin energy; silence stays dark.
    float drive=saturate(note.x*1.20);
    [loop]
    for(int z=GZ-1;z>=0;--z){
        float depth=(z+0.5)/GZ;
        float3 pos=float3(x,y,depth);
        float4 f=phasesIn.SampleLevel(linearClamp,pos,0);
        float density=saturate(1.8*f.x+1.6*f.y+0.6*f.w);
        if(density<0.001)continue;
        float3 dx=float3(1.0/GX,0,0);
        float3 dy=float3(0,1.0/GY,0);
        float3 dz=float3(0,0,1.0/GZ);
        float nx=phasesIn.SampleLevel(linearClamp,saturate(pos+dx),0).x-
                 phasesIn.SampleLevel(linearClamp,saturate(pos-dx),0).x;
        float ny=phasesIn.SampleLevel(linearClamp,saturate(pos+dy),0).x-
                 phasesIn.SampleLevel(linearClamp,saturate(pos-dy),0).x;
        float nz=phasesIn.SampleLevel(linearClamp,saturate(pos+dz),0).x-
                 phasesIn.SampleLevel(linearClamp,saturate(pos-dz),0).x;
        float3 normal=normalize(float3(-nx*4,-ny*4,0.50-nz*4));
        float facing=saturate(dot(normal,normalize(float3(-0.42,0.64,0.71))));
        float highlight=pow(facing,11.0);
        // Iridescence lives inside the transported material, not on the FFT line.
        float twist=sin(13*x+10*y+6*depth+f.z*8+control.x*0.25);
        float oil=0.5+0.5*twist;
        float3 violet=float3(note.z,note.w,note.y);
        float3 tideColour=lerp(note.yzw,violet,0.20+0.30*oil);
        float shadow=0.58+0.42*saturate(normal.z*0.5+0.5);
        float interference=0.5+0.5*sin(20.0*f.z+12.0*depth+9.0*y+control.x*0.35);
        float layer=saturate(f.y*1.5+f.z*0.9);
        float3 base=float3(0.012,0.018,0.023)*(0.5+shadow);
        float3 colour=base+
            tideColour*layer*(0.40+0.47*interference)*drive+
            float3(0.76,0.89,0.96)*highlight*(0.29+1.45*f.x)*drive;
        colour+=tideColour*f.w*(0.18+0.26*oil)*drive;
        float extinction=1-exp(-density*0.68);
        acc+=transparency*extinction*colour;
        transparency*=1-extinction;
        if(transparency<0.018)break;
    }
    // JUCE's image pixels are display encoded. Linear radiance written
    // directly to UNORM previously appeared too dark to show the oil film.
    acc+=transparency*float3(0.002,0.004,0.006);
    // A continuous reflective surface sits in front of the transported
    // volume. The volume supplies refraction/turbulence, not an opaque fog
    // covering the graph. Surface height follows smoothed measured energy;
    // this gives an immediate material response after a cold editor opening.
    float column=x*63.0;
    int ci=(int)column;
    float e=0;
    [unroll] for(int j=-2;j<=2;++j){
        float w=3-abs(j);
        float energy=lerp(spectral[clamp(ci+j,0,63)].x,
                          spectral[clamp(ci+j+1,0,63)].x,frac(column));
        e+=w*saturate((65*energy-12)/60);
    }
    e/=9;
    float t=control.x*(control.y>.5?.12:.30);
    float height=e*(.76+.065*sin(31*x+t)+.025*sin(77*x-t));
    if(height>.003 && y<height){
        float v=saturate((height-y)/max(.003,height));
        float4 transported=phasesIn.SampleLevel(linearClamp,float3(x,1-uv.y,.58),0);
        float fold=sin(19*v+sin(37*x+t)*2+transported.z*2);
        float nx=.28*cos(37*x+t)+.14*sin(81*x-t);
        float ny=.68*cos(19*v+sin(37*x+t)*2+transported.z*2);
        float3 n=normalize(float3(nx,ny,1));
        float facing=saturate(dot(n,normalize(float3(-.3,.56,.77))));
        float spec=pow(facing,48);
        float rim=exp(-v*height*IH/2.2);
        float ribbon=pow(saturate(.5+.5*fold),18);
        float3 film=lerp(note.yzw,float3(note.w,note.y,note.z),.35+.25*sin(27*v+17*x));
        float3 black=float3(.001,.002,.003)+float3(.003,.004,.006)*saturate(n.z);
        acc=black+(float3(.72,.85,.94)*spec+film*ribbon*.38+rim*float3(.53,.65,.72))*saturate(e*1.7);
        acc+=film*transported.w*.035;
    }else{
        acc*=.16; // L02 faint aurora remains behind the reflective body.
    }
    // Sparse reflective beads above the meniscus, with no independent energy
    // or free-running flashes. Position is deterministic and Freeze holds t.
    float cell=floor(x*31);
    float beadX=(cell+.5+.16*sin(cell*2.37))/31;
    float beadEnergy=saturate((65*spectral[clamp((int)(beadX*63),0,63)].x-12)/60);
    float beadY=beadEnergy*(.80+.055*sin(cell*1.9+t));
    float radius=.009*(.4+beadEnergy);
    float2 delta=float2((x-beadX)*IW/IH,y-beadY);
    float distance=dot(delta,delta)/(radius*radius);
    if(beadEnergy>.20 && distance<1 && sin(cell*3.2)>.1){
        float3 n=normalize(float3(delta/radius,sqrt(1-distance)));
        float shine=pow(saturate(dot(n,normalize(float3(-.45,.6,.65)))),30);
        float edge=pow(saturate(distance),10);
        acc=float3(.004,.007,.01)+shine*float3(.85,.94,1)+edge*note.yzw*.24;
    }
    float3 mapped=acc/(1.0+acc);
    imageOut[id.xy]=float4(pow(saturate(mapped*1.45),1.0/2.2),1);
}
)HLSL";

struct VolumeTexture {
    ComPtr<ID3D11Texture3D> texture;
    ComPtr<ID3D11ShaderResourceView> read;
    ComPtr<ID3D11UnorderedAccessView> write;
};
struct ScalarTexture {
    ComPtr<ID3D11Texture3D> texture;
    ComPtr<ID3D11ShaderResourceView> read;
    ComPtr<ID3D11UnorderedAccessView> write;
};
struct alignas(16) GPUParameters {
    float dynamics[4]{};
    float control[4]{};
    float spectral[64][4]{};
};
} // namespace

struct FluidVolume3D::Impl {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11ComputeShader> shader[6];
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11Buffer> parameters;
    VolumeTexture velocity[2], phases[2];
    ScalarTexture pressure[2], divergence;
    ComPtr<ID3D11Texture2D> output;
    ComPtr<ID3D11UnorderedAccessView> outputWrite;
    ComPtr<ID3D11Texture2D> staging[3];
    unsigned frame=0;
    unsigned imageW=IW,imageH=IH;
    bool attempted=false, ready=false, imageReady=false;
    const char* status="GPU PENDING";
    double accumulator=0.0;
    float elapsedPhase=0.f;

    ~Impl(){
        // Release all bindings while our context/device still exist. No GPU
        // fence or deferred callback may outlive the editor/DLL on host removal.
        if(context){context->ClearState();context->Flush();}
    }
    bool resizeOutput(unsigned w,unsigned h){
        if(w==imageW&&h==imageH)return true;
        D3D11_TEXTURE2D_DESC d{};
        d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.Usage=D3D11_USAGE_DEFAULT;
        d.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
        ComPtr<ID3D11Texture2D> newOutput,newStaging[3];
        ComPtr<ID3D11UnorderedAccessView> newWrite;
        if(FAILED(device->CreateTexture2D(&d,nullptr,&newOutput))||
           FAILED(device->CreateUnorderedAccessView(newOutput.Get(),nullptr,&newWrite)))return false;
        d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        for(auto& texture:newStaging)if(FAILED(device->CreateTexture2D(&d,nullptr,&texture)))return false;
        output=std::move(newOutput);outputWrite=std::move(newWrite);
        for(int i=0;i<3;++i)staging[i]=std::move(newStaging[i]);
        imageW=w;imageH=h;frame=0;imageReady=false;return true;
    }
    bool makeVolume(VolumeTexture& target) {
        D3D11_TEXTURE3D_DESC d{};
        d.Width=GX;d.Height=GY;d.Depth=GZ;d.MipLevels=1;
        d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
        d.Usage=D3D11_USAGE_DEFAULT;
        d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        return SUCCEEDED(device->CreateTexture3D(&d,nullptr,&target.texture)) &&
               SUCCEEDED(device->CreateShaderResourceView(target.texture.Get(),nullptr,&target.read)) &&
               SUCCEEDED(device->CreateUnorderedAccessView(target.texture.Get(),nullptr,&target.write));
    }
    bool makeScalar(ScalarTexture& target) {
        D3D11_TEXTURE3D_DESC d{};
        d.Width=GX;d.Height=GY;d.Depth=GZ;d.MipLevels=1;
        d.Format=DXGI_FORMAT_R32_FLOAT;
        d.Usage=D3D11_USAGE_DEFAULT;
        d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        return SUCCEEDED(device->CreateTexture3D(&d,nullptr,&target.texture)) &&
               SUCCEEDED(device->CreateShaderResourceView(target.texture.Get(),nullptr,&target.read)) &&
               SUCCEEDED(device->CreateUnorderedAccessView(target.texture.Get(),nullptr,&target.write));
    }
    bool initialize() {
        if(attempted)return ready;
        attempted=true;
        // Headless CI must validate HLSL without exercising virtual/software
        // GPUs during GUI screenshots; real GTX hardware still uses D3D11.
        if(const char* disabled=std::getenv("SCALPEL_GPU_DISABLE"))
            if(disabled[0]=='1'){status="GPU DISABLED";return false;}
        D3D_FEATURE_LEVEL feature{};
        const D3D_FEATURE_LEVEL requested[]{D3D_FEATURE_LEVEL_11_0};
        if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,requested,1,D3D11_SDK_VERSION,
            &device,&feature,&context)) || feature<D3D_FEATURE_LEVEL_11_0){
            status="GPU UNAVAILABLE";return false;
        }
        static constexpr const char* entries[]{
            "AdvectVelocity","AdvectPhases","Divergence",
            "PressureJacobi","ProjectVelocity","RenderVolume"};
        for(int i=0;i<6;++i) {
            ComPtr<ID3DBlob> bytecode,errors;
            if(FAILED(D3DCompile(fluidHlsl,std::strlen(fluidHlsl),nullptr,nullptr,nullptr,
                entries[i],"cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,
                0,&bytecode,&errors))){
                status="SHADER ERROR";
                if(errors)std::fprintf(stderr,"Scalpel X-RAY GPU shader: %s\n",
                    (const char*)errors->GetBufferPointer());
                return false;
            }
            if(FAILED(device->CreateComputeShader(bytecode->GetBufferPointer(),
                bytecode->GetBufferSize(),nullptr,&shader[i]))){
                status="GPU SHADER UNSUPPORTED";return false;
            }
        }
        // Any subsequent failure is a D3D resource/format limit rather
        // than a silent return to the old CPU visualizer.
        status="GPU RESOURCE ERROR";
        D3D11_SAMPLER_DESC s{};
        s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
        s.MaxLOD=D3D11_FLOAT32_MAX;
        if(FAILED(device->CreateSamplerState(&s,&sampler)))return false;
        D3D11_BUFFER_DESC b{};
        b.ByteWidth=sizeof(GPUParameters);
        b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(FAILED(device->CreateBuffer(&b,nullptr,&parameters)))return false;
        for(auto& v:velocity)if(!makeVolume(v))return false;
        for(auto& v:phases)if(!makeVolume(v))return false;
        for(auto& v:pressure)if(!makeScalar(v))return false;
        if(!makeScalar(divergence))return false;
        const float clear[]{0,0,0,0};
        for(auto& v:velocity)context->ClearUnorderedAccessViewFloat(v.write.Get(),clear);
        for(auto& v:phases)context->ClearUnorderedAccessViewFloat(v.write.Get(),clear);
        for(auto& v:pressure)context->ClearUnorderedAccessViewFloat(v.write.Get(),clear);
        context->ClearUnorderedAccessViewFloat(divergence.write.Get(),clear);
        D3D11_TEXTURE2D_DESC image{};
        image.Width=IW;image.Height=IH;image.MipLevels=1;image.ArraySize=1;
        image.SampleDesc.Count=1;image.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        image.Usage=D3D11_USAGE_DEFAULT;image.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
        if(FAILED(device->CreateTexture2D(&image,nullptr,&output)) ||
           FAILED(device->CreateUnorderedAccessView(output.Get(),nullptr,&outputWrite)))return false;
        image.Usage=D3D11_USAGE_STAGING;image.BindFlags=0;
        image.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        for(auto& p:staging)
            if(FAILED(device->CreateTexture2D(&image,nullptr,&p)))return false;
        ready=true;status="GPU WARMING";return true;
    }
    void dispatch(int kernel, ID3D11ShaderResourceView* velocityRead,
                  ID3D11ShaderResourceView* phaseRead,
                  ID3D11ShaderResourceView* pressureRead,
                  ID3D11ShaderResourceView* divergenceRead,
                  ID3D11UnorderedAccessView* velocityWrite,
                  ID3D11UnorderedAccessView* phaseWrite,
                  ID3D11UnorderedAccessView* scalarWrite,
                  ID3D11UnorderedAccessView* imageWrite) {
        ID3D11ShaderResourceView* inputs[]{velocityRead,phaseRead,pressureRead,divergenceRead};
        ID3D11UnorderedAccessView* outputs[]{velocityWrite,phaseWrite,scalarWrite,imageWrite};
        context->CSSetShader(shader[kernel].Get(),nullptr,0);
        context->CSSetShaderResources(0,4,inputs);
        context->CSSetUnorderedAccessViews(0,4,outputs,nullptr);
        context->Dispatch(kernel==5?(imageW+7)/8:(GX+7)/8,
                          kernel==5?(imageH+7)/8:(GY+7)/8,
                          kernel==5?1:(GZ+3)/4);
        ID3D11UnorderedAccessView* emptyOutputs[4]{};
        ID3D11ShaderResourceView* emptyInputs[4]{};
        context->CSSetUnorderedAccessViews(0,4,emptyOutputs,nullptr);
        context->CSSetShaderResources(0,4,emptyInputs);
    }
    void fluidStep() {
        dispatch(0,velocity[0].read.Get(),phases[0].read.Get(),nullptr,nullptr,
                 velocity[1].write.Get(),nullptr,nullptr,nullptr);
        dispatch(1,velocity[1].read.Get(),phases[0].read.Get(),nullptr,nullptr,
                 nullptr,phases[1].write.Get(),nullptr,nullptr);
        dispatch(2,velocity[1].read.Get(),nullptr,nullptr,nullptr,
                 nullptr,nullptr,divergence.write.Get(),nullptr);
        int source=0,destination=1;
        for(int i=0;i<24;++i) {
            dispatch(3,nullptr,nullptr,pressure[source].read.Get(),divergence.read.Get(),
                     nullptr,nullptr,pressure[destination].write.Get(),nullptr);
            std::swap(source,destination);
        }
        dispatch(4,velocity[1].read.Get(),nullptr,pressure[source].read.Get(),nullptr,
                 velocity[0].write.Get(),nullptr,nullptr,nullptr);
        std::swap(phases[0],phases[1]);
    }
    bool render(const std::array<FluidColumn,64>& columns,float bass,float flux,float level,
                double elapsed,bool reduced,juce::Image& target,int physicalWidth,int physicalHeight) {
        if(!initialize())return false;
        if(!resizeOutput((unsigned)std::clamp(physicalWidth,64,3840),(unsigned)std::clamp(physicalHeight,32,2160))){status="GPU SIZE ERROR";return false;}
        if(device->GetDeviceRemovedReason()!=S_OK) {
            status="GPU DEVICE LOST";ready=false;imageReady=false;return false;
        }
        elapsed=std::clamp(elapsed,0.0,1.0/15.0);
        accumulator=(std::min)(accumulator+elapsed,1.0/15.0);
        elapsedPhase=std::fmod(elapsedPhase+(float)elapsed,4096.f);
        GPUParameters frameData{};
        frameData.dynamics[0]=1.f/30.f;
        frameData.dynamics[1]=std::clamp(bass,0.f,1.f);
        frameData.dynamics[2]=std::clamp(flux,0.f,1.f);
        frameData.dynamics[3]=std::clamp(level,0.f,1.f);
        frameData.control[0]=elapsedPhase;
        frameData.control[1]=reduced?1.f:0.f;
        frameData.control[2]=(float)imageW;frameData.control[3]=(float)imageH;
        for(unsigned i=0;i<GX;++i){
            frameData.spectral[i][0]=std::clamp(columns[i].energy,0.f,1.f);
            frameData.spectral[i][1]=std::clamp(columns[i].red,0.f,1.f);
            frameData.spectral[i][2]=std::clamp(columns[i].green,0.f,1.f);
            frameData.spectral[i][3]=std::clamp(columns[i].blue,0.f,1.f);
        }
        D3D11_MAPPED_SUBRESOURCE data{};
        if(FAILED(context->Map(parameters.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&data))){
            status="GPU UPLOAD ERROR";return false;
        }
        std::memcpy(data.pData,&frameData,sizeof(frameData));
        context->Unmap(parameters.Get(),0);
        ID3D11Buffer* constantBuffer=parameters.Get();
        ID3D11SamplerState* linearSampler=sampler.Get();
        context->CSSetConstantBuffers(0,1,&constantBuffer);
        context->CSSetSamplers(0,1,&linearSampler);
        // Integrate at fixed 30 Hz; ray-march at the editor's target 60 Hz.
        if(accumulator>=1.0/30.0) {
            fluidStep();
            accumulator-=1.0/30.0;
        }
        dispatch(5,nullptr,phases[0].read.Get(),nullptr,nullptr,
                 nullptr,nullptr,nullptr,outputWrite.Get());
        const unsigned written=frame%3;
        context->CopyResource(staging[written].Get(),output.Get());
        // Offscreen compute has no swap-chain Present. DO_NOT_WAIT Map does
        // not submit buffered commands; without Flush the GUI can remain at
        // GPU WARMING forever even on supported hardware. This submits work
        // without waiting for the GPU or involving the audio thread.
        context->Flush();
        ++frame;
        if(frame<2)return false;
        const unsigned readyIndex=(frame+1)%3;
        D3D11_MAPPED_SUBRESOURCE pixels{};
        const HRESULT mapped=context->Map(staging[readyIndex].Get(),0,
                                          D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&pixels);
        if(mapped==DXGI_ERROR_WAS_STILL_DRAWING)return false;
        if(FAILED(mapped)){status="GPU READBACK ERROR";return false;}
        if(target.getWidth()!=(int)imageW||target.getHeight()!=(int)imageH)
            target=juce::Image(juce::Image::RGB,(int)imageW,(int)imageH,true);
        {
            juce::Image::BitmapData destination(target,juce::Image::BitmapData::writeOnly);
            for(unsigned y=0;y<imageH;++y){
                auto* source=(const std::uint8_t*)pixels.pData+(std::size_t)y*pixels.RowPitch;
                for(unsigned x=0;x<imageW;++x){
                    const auto* rgba=source+x*4;
                    destination.setPixelColour((int)x,(int)y,juce::Colour(rgba[0],rgba[1],rgba[2]));
                }
            }
        }
        context->Unmap(staging[readyIndex].Get(),0);
        imageReady=true;
        status="GPU LIVE";
        return true;
    }
};
#else
struct FluidVolume3D::Impl {
    bool imageReady=false;
    const char* status="GPU UNSUPPORTED";
    bool render(const std::array<FluidColumn,64>&,float,float,float,
                double,bool,juce::Image&,int,int) {return false;}
};
#endif

bool FluidVolume3D::validateShaderSource() noexcept {
#if defined(_WIN32)
    static constexpr const char* entries[]{
        "AdvectVelocity","AdvectPhases","Divergence",
        "PressureJacobi","ProjectVelocity","RenderVolume"};
    for(const char* entry:entries){
        ComPtr<ID3DBlob> binary,errors;
        const HRESULT compiled=D3DCompile(fluidHlsl,std::strlen(fluidHlsl),
            nullptr,nullptr,nullptr,entry,"cs_5_0",
            D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,
            0,&binary,&errors);
        if(FAILED(compiled)){
            if(errors){
                std::fprintf(stderr,"X-RAY HLSL %s: %s\n",entry,
                    (const char*)errors->GetBufferPointer());
            }
            return false;
        }
    }
#endif
    return true;
}
FluidVolume3D::FluidVolume3D()=default;
FluidVolume3D::~FluidVolume3D()=default;
bool FluidVolume3D::render(const std::array<FluidColumn,gridWidth>& columns,
                           float bass,float flux,float level,double elapsed,
                           bool reduced,juce::Image& target,int physicalWidth,int physicalHeight) {
    if(!impl_)impl_=std::make_unique<Impl>();
    return impl_->render(columns,bass,flux,level,elapsed,reduced,target,physicalWidth,physicalHeight);
}
bool FluidVolume3D::hasFrame() const noexcept {
    return impl_&&impl_->imageReady;
}
const char* FluidVolume3D::statusText() const noexcept {
    return impl_?impl_->status:"GPU PENDING";
}
void FluidVolume3D::reset() noexcept {
    impl_.reset();
}

} // namespace scalpel::xray

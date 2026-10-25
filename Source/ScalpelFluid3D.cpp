#include "ScalpelFluid3D.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)
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
#define IW 640
#define IH 224
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
    float floorWeight=exp(-float(id.y)*0.125);
    float depthPattern=0.5+0.5*sin(float(id.z)*0.35+control.x*0.30);
    float strength=sound.x*floorWeight*(0.45+0.55*depthPattern);
    float source=strength*(0.007+0.027*dynamics.z+0.010*dynamics.w);
    // Phase 1 is slow heavy liquid, phase 2 is lighter coloured liquid.
    float heavy=saturate(q.x*0.998+source*0.67);
    float light=saturate(q.y*0.991+source*(0.38+0.22*sound.z));
    float total=heavy+light;
    if(total>0.96) {heavy*=0.96/total;light*=0.96/total;}
    // Chromatic pigment and gas pockets are independently transported.
    float pigment=saturate(q.z*0.992+source*(0.4*sound.y+0.34*sound.z+0.26*sound.w));
    float bubbles=saturate(q.w*0.963+source*dynamics.z*(0.2+0.8*dynamics.w));
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
    float3 acc=float3(0.005,0.007,0.010);
    float transparency=1.0;
    float4 note=spectral[min(63,(uint)(x*64))];
    [loop]
    for(int z=GZ-1;z>=0;--z){
        float depth=(z+0.5)/GZ;
        float3 pos=float3(x,y,depth);
        float4 f=phasesIn.SampleLevel(linearClamp,pos,0);
        float density=saturate(1.5*f.x+1.2*f.y+0.7*f.w);
        if(density<0.002)continue;
        float3 dx=float3(1.0/GX,0,0);
        float3 dy=float3(0,1.0/GY,0);
        float3 dz=float3(0,0,1.0/GZ);
        float nx=phasesIn.SampleLevel(linearClamp,saturate(pos+dx),0).x-
                 phasesIn.SampleLevel(linearClamp,saturate(pos-dx),0).x;
        float ny=phasesIn.SampleLevel(linearClamp,saturate(pos+dy),0).x-
                 phasesIn.SampleLevel(linearClamp,saturate(pos-dy),0).x;
        float nz=phasesIn.SampleLevel(linearClamp,saturate(pos+dz),0).x-
                 phasesIn.SampleLevel(linearClamp,saturate(pos-dz),0).x;
        float3 normal=normalize(float3(-nx*3,-ny*3,0.7-nz*3));
        float highlight=pow(saturate(dot(normal,normalize(float3(-0.4,0.6,0.8)))),20.0);
        float twist=sin(13*x+10*y+6*depth+f.z*8+control.x*0.10);
        float oil=0.5+0.5*twist;
        float3 base=float3(0.009,0.012,0.015);
        float3 spectralLight=note.yzw*(0.10+0.42*oil);
        float3 colour=base+spectralLight*f.y*(0.24+f.z*1.15)
                       +float3(0.76,0.85,0.91)*highlight*(0.10+0.75*f.x);
        colour+=float3(0.15,0.27,0.36)*f.w*0.12;
        float extinction=1-exp(-density*0.22);
        acc+=transparency*extinction*colour;
        transparency*=1-extinction;
        if(transparency<0.025)break;
    }
    // Black metallic substrate, with chromatic energy only where liquid exists.
    float shade=saturate(0.9-abs(y-0.45)*0.3);
    acc+=transparency*float3(0.006,0.009,0.011)*shade;
    imageOut[id.xy]=float4(saturate(acc),1);
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
    bool attempted=false, ready=false, imageReady=false;
    double accumulator=0.0;
    float elapsedPhase=0.f;

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
        D3D_FEATURE_LEVEL feature{};
        const D3D_FEATURE_LEVEL requested[]{D3D_FEATURE_LEVEL_11_0};
        if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,requested,1,D3D11_SDK_VERSION,
            &device,&feature,&context)) || feature<D3D_FEATURE_LEVEL_11_0)return false;
        static constexpr const char* entries[]{
            "AdvectVelocity","AdvectPhases","Divergence",
            "PressureJacobi","ProjectVelocity","RenderVolume"};
        for(int i=0;i<6;++i) {
            ComPtr<ID3DBlob> bytecode,errors;
            if(FAILED(D3DCompile(fluidHlsl,std::strlen(fluidHlsl),nullptr,nullptr,nullptr,
                entries[i],"cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,
                0,&bytecode,&errors)))return false;
            if(FAILED(device->CreateComputeShader(bytecode->GetBufferPointer(),
                bytecode->GetBufferSize(),nullptr,&shader[i])))return false;
        }
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
        ready=true;return true;
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
        context->Dispatch(kernel==5?(IW+7)/8:(GX+7)/8,
                          kernel==5?(IH+7)/8:(GY+7)/8,
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
                double elapsed,bool reduced,juce::Image& target) {
        if(!initialize())return false;
        if(device->GetDeviceRemovedReason()!=S_OK) {ready=false;return false;}
        elapsed=std::clamp(elapsed,0.0,1.0/15.0);
        accumulator=std::min(accumulator+elapsed,1.0/15.0);
        elapsedPhase=std::fmod(elapsedPhase+(float)elapsed,4096.f);
        GPUParameters frameData{};
        frameData.dynamics[0]=1.f/30.f;
        frameData.dynamics[1]=std::clamp(bass,0.f,1.f);
        frameData.dynamics[2]=std::clamp(flux,0.f,1.f);
        frameData.dynamics[3]=std::clamp(level,0.f,1.f);
        frameData.control[0]=elapsedPhase;
        frameData.control[1]=reduced?1.f:0.f;
        for(unsigned i=0;i<GX;++i){
            frameData.spectral[i][0]=std::clamp(columns[i].energy,0.f,1.f);
            frameData.spectral[i][1]=std::clamp(columns[i].red,0.f,1.f);
            frameData.spectral[i][2]=std::clamp(columns[i].green,0.f,1.f);
            frameData.spectral[i][3]=std::clamp(columns[i].blue,0.f,1.f);
        }
        D3D11_MAPPED_SUBRESOURCE data{};
        if(FAILED(context->Map(parameters.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&data)))return false;
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
        ++frame;
        if(frame<2)return false;
        const unsigned readyIndex=(frame+1)%3;
        D3D11_MAPPED_SUBRESOURCE pixels{};
        const HRESULT mapped=context->Map(staging[readyIndex].Get(),0,
                                          D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&pixels);
        if(mapped==DXGI_ERROR_WAS_STILL_DRAWING)return false;
        if(FAILED(mapped))return false;
        if(target.getWidth()!=(int)IW||target.getHeight()!=(int)IH)
            target=juce::Image(juce::Image::RGB,(int)IW,(int)IH,true);
        {
            juce::Image::BitmapData destination(target,juce::Image::BitmapData::writeOnly);
            for(unsigned y=0;y<IH;++y){
                auto* source=(const std::uint8_t*)pixels.pData+(std::size_t)y*pixels.RowPitch;
                for(unsigned x=0;x<IW;++x){
                    const auto* rgba=source+x*4;
                    destination.setPixelColour((int)x,(int)y,juce::Colour(rgba[0],rgba[1],rgba[2]));
                }
            }
        }
        context->Unmap(staging[readyIndex].Get(),0);
        imageReady=true;
        return true;
    }
};
#else
struct FluidVolume3D::Impl {
    bool imageReady=false;
    bool render(const std::array<FluidColumn,64>&,float,float,float,
                double,bool,juce::Image&) {return false;}
};
#endif

FluidVolume3D::FluidVolume3D()=default;
FluidVolume3D::~FluidVolume3D()=default;
bool FluidVolume3D::render(const std::array<FluidColumn,gridWidth>& columns,
                           float bass,float flux,float level,double elapsed,
                           bool reduced,juce::Image& target) {
    if(!impl_)impl_=std::make_unique<Impl>();
    return impl_->render(columns,bass,flux,level,elapsed,reduced,target);
}
bool FluidVolume3D::hasFrame() const noexcept {
    return impl_&&impl_->imageReady;
}
void FluidVolume3D::reset() noexcept {
    impl_.reset();
}

} // namespace scalpel::xray

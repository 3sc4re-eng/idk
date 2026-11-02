#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x06020000
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#include "model_preview.h"
#include "mesh_assets.h"
#include <compressapi.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace game_model{
namespace{
struct P{float x,y,z;};
struct Vertex{P position,normal;float u,v;};
struct Part{uint32_t first,count;int32_t texture;float color[4];};
struct Bone{P position;int parent,group;std::string name;};
struct Model{
    ID3D11Buffer* vertices=nullptr;ID3D11Buffer* indices=nullptr;
    std::vector<ID3D11ShaderResourceView*> textures;std::vector<P> positions;
    std::vector<Part> parts;std::vector<Bone> bones;P minimum{},maximum{};
    bool attempted=false;std::string error;unsigned triangles=0;
};
template<class T>void release(T*& object){if(object){object->Release();object=nullptr;}}
static ID3D11Device* device=nullptr;static ID3D11DeviceContext* context=nullptr;
static ID3D11VertexShader* vs=nullptr;static ID3D11PixelShader* ps=nullptr;
static ID3D11InputLayout* layout=nullptr;static ID3D11Buffer* constants=nullptr;static ID3D11Buffer* material=nullptr;
static ID3D11SamplerState* sampler=nullptr;static ID3D11RasterizerState* raster=nullptr;static ID3D11DepthStencilState* depth_state=nullptr;
static ID3D11BlendState* blend=nullptr;static ID3D11ShaderResourceView* white=nullptr;
static ID3D11Texture2D* target=nullptr;static ID3D11RenderTargetView* target_view=nullptr;static ID3D11ShaderResourceView* target_image=nullptr;
static ID3D11Texture2D* depth=nullptr;static ID3D11DepthStencilView* depth_view=nullptr;
static int target_width=0,target_height=0;static std::array<Model,2> models;static Frame frame;static std::string pipeline_error;static int cached_index=-1,cached_width=0,cached_height=0;static View cached_view;
struct Camera{float pivot[4];float rotate[4];float project[4];};
static void check(HRESULT result,const char* what){if(FAILED(result))throw std::runtime_error(std::string(what)+" ("+std::to_string((unsigned)result)+")");}
struct Reader{
    const unsigned char* cursor;const unsigned char* end;
    void read(void* output,size_t size){if(size_t(end-cursor)<size)throw std::runtime_error("Truncated model asset");std::memcpy(output,cursor,size);cursor+=size;}
    template<class T>T get(){T value;read(&value,sizeof(value));return value;}
};
static std::vector<unsigned char> unpack(const char* text,size_t length,size_t expected){
    std::vector<unsigned char> packed;packed.reserve(length*3/4);uint32_t word=0;int bits=-8;
    for(size_t i=0;i<length;i++){
        unsigned char c=text[i];int value=c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:c>='0'&&c<='9'?c-'0'+52:c=='+'?62:c=='/'?63:-1;
        if(c=='=')break;if(value<0)continue;word=(word<<6)|(unsigned)value;bits+=6;if(bits>=0){packed.push_back((unsigned char)(word>>bits));bits-=8;}
    }
    DECOMPRESSOR_HANDLE decoder=nullptr;if(!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&decoder))throw std::runtime_error("Cannot create model decoder");
    std::vector<unsigned char> result(expected);SIZE_T written=0;BOOL ok=Decompress(decoder,packed.data(),packed.size(),result.data(),result.size(),&written);CloseDecompressor(decoder);
    if(!ok||written!=expected)throw std::runtime_error("Cannot decompress model asset");return result;
}
static ID3D11ShaderResourceView* texture(const unsigned char* data,size_t length){
    IWICImagingFactory* factory=nullptr;IWICStream* stream=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* source=nullptr;IWICFormatConverter* converter=nullptr;
    ID3D11Texture2D* image=nullptr;ID3D11ShaderResourceView* view=nullptr;
    try{
        check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Texture factory");
        check(factory->CreateStream(&stream),"Texture stream");check(stream->InitializeFromMemory((BYTE*)data,(DWORD)length),"Texture bytes");
        check(factory->CreateDecoderFromStream(stream,nullptr,WICDecodeMetadataCacheOnLoad,&decoder),"PNG decoder");check(decoder->GetFrame(0,&source),"PNG frame");
        check(factory->CreateFormatConverter(&converter),"Texture converter");check(converter->Initialize(source,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"Texture format");
        UINT width=0,height=0;check(converter->GetSize(&width,&height),"Texture dimensions");if(!width||!height||width>8192||height>8192)throw std::runtime_error("Invalid texture dimensions");
        std::vector<BYTE> pixels(size_t(width)*height*4);check(converter->CopyPixels(nullptr,width*4,(UINT)pixels.size(),pixels.data()),"Texture pixels");
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.ArraySize=desc.MipLevels=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial{pixels.data(),width*4,0};check(device->CreateTexture2D(&desc,&initial,&image),"Texture upload");check(device->CreateShaderResourceView(image,nullptr,&view),"Texture view");
    }catch(...){release(image);release(converter);release(source);release(decoder);release(stream);release(factory);throw;}
    release(image);release(converter);release(source);release(decoder);release(stream);release(factory);return view;
}
static void load(Model& model,int index){
    if(model.attempted)return;model.attempted=true;
    try{
        auto bytes=index==0?unpack(warden_model_base64,sizeof(warden_model_base64)-1,warden_unpacked_size):unpack(sas_model_base64,sizeof(sas_model_base64)-1,sas_unpacked_size);
        Reader reader{bytes.data(),bytes.data()+bytes.size()};uint32_t magic=reader.get<uint32_t>(),version=reader.get<uint32_t>();
        if(magic!=0x324d4341||version!=2)throw std::runtime_error("Unsupported model format");
        uint32_t vertex_count=reader.get<uint32_t>(),index_count=reader.get<uint32_t>(),part_count=reader.get<uint32_t>(),bone_count=reader.get<uint32_t>(),texture_count=reader.get<uint32_t>();reader.get<uint32_t>();

        if(!vertex_count||vertex_count>2000000||!index_count||index_count%3||part_count>1024||bone_count>1024||texture_count>256)throw std::runtime_error("Invalid model counts");
        reader.read(&model.minimum,sizeof(P));reader.read(&model.maximum,sizeof(P));
        std::vector<Vertex> vertices(vertex_count);reader.read(vertices.data(),vertices.size()*sizeof(Vertex));
        std::vector<uint32_t> indices(index_count);reader.read(indices.data(),indices.size()*sizeof(uint32_t));for(auto value:indices)if(value>=vertex_count)throw std::runtime_error("Invalid triangle index");
        model.parts.resize(part_count);reader.read(model.parts.data(),model.parts.size()*sizeof(Part));
        for(auto part:model.parts)if(part.first>index_count||part.count>index_count-part.first||part.texture>=int(texture_count))throw std::runtime_error("Invalid material range");
        for(unsigned i=0;i<texture_count;i++){uint32_t size=reader.get<uint32_t>();if(size>size_t(reader.end-reader.cursor))throw std::runtime_error("Invalid texture range");model.textures.push_back(texture(reader.cursor,size));reader.cursor+=size;}
        for(unsigned i=0;i<bone_count;i++){
            Bone bone;reader.read(&bone.position,sizeof(P));bone.parent=reader.get<int32_t>();bone.group=reader.get<int32_t>();uint32_t size=reader.get<uint32_t>();
            if(size>256||size>size_t(reader.end-reader.cursor)||bone.parent>=int(bone_count)||bone.group<0||bone.group>6)throw std::runtime_error("Invalid joint");
            bone.name.assign((const char*)reader.cursor,size);reader.cursor+=size;model.bones.push_back(std::move(bone));
        }
        if(reader.cursor!=reader.end)throw std::runtime_error("Unexpected model data");
        for(auto& vertex:vertices)model.positions.push_back(vertex.position);

        D3D11_BUFFER_DESC desc{};desc.ByteWidth=(UINT)(vertices.size()*sizeof(Vertex));desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA initial{vertices.data(),0,0};check(device->CreateBuffer(&desc,&initial,&model.vertices),"Mesh vertex upload");
        desc.ByteWidth=(UINT)(indices.size()*sizeof(uint32_t));desc.BindFlags=D3D11_BIND_INDEX_BUFFER;initial.pSysMem=indices.data();check(device->CreateBuffer(&desc,&initial,&model.indices),"Mesh index upload");model.triangles=index_count/3;
    }catch(const std::exception& error){model.error=error.what();}
}
static void resize(int width,int height){
    if(target&&width==target_width&&height==target_height)return;
    release(target_image);release(target_view);release(target);release(depth_view);release(depth);
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.ArraySize=desc.MipLevels=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    check(device->CreateTexture2D(&desc,nullptr,&target),"Preview target");check(device->CreateRenderTargetView(target,nullptr,&target_view),"Preview target view");check(device->CreateShaderResourceView(target,nullptr,&target_image),"Preview image");
    desc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;check(device->CreateTexture2D(&desc,nullptr,&depth),"Preview depth");check(device->CreateDepthStencilView(depth,nullptr,&depth_view),"Preview depth view");
    target_width=width;target_height=height;
}
static P rotate(P p,const Camera& camera){
    p.x-=camera.pivot[0];p.y-=camera.pivot[1];p.z-=camera.pivot[2];
    float x=p.x*camera.rotate[0]+p.z*camera.rotate[1],z=-p.x*camera.rotate[1]+p.z*camera.rotate[0];
    return {x,p.y*camera.rotate[2]-z*camera.rotate[3],p.y*camera.rotate[3]+z*camera.rotate[2]};
}
static ImVec2 project(P p,const Camera& camera,int width,int height){p=rotate(p,camera);return {(p.x*camera.project[0]/camera.pivot[3]/camera.project[1]*.5f+.5f)*width,(.5f-p.y*camera.project[0]/camera.pivot[3]*.5f)*height};}
}
bool initialize(ID3D11Device* new_device,ID3D11DeviceContext* new_context){
    device=new_device;context=new_context;pipeline_error.clear();
    try{
        const char* shader=R"HLSL(
cbuffer Camera:register(b0){float4 pivot;float4 angles;float4 projection;};
cbuffer Material:register(b1){float4 baseColor;};
struct Input{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;};
struct Output{float4 p:SV_POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;};
float3 turn(float3 p){float x=p.x*angles.x+p.z*angles.y;float z=-p.x*angles.y+p.z*angles.x;return float3(x,p.y*angles.z-z*angles.w,p.y*angles.w+z*angles.z);}
Output vertexMain(Input input){Output o;float3 p=turn(input.p-pivot.xyz);o.p=float4(p.x*projection.x/pivot.w/projection.y,p.y*projection.x/pivot.w,.5-p.z/projection.z,1);o.n=turn(input.n);o.uv=input.uv;return o;}
Texture2D image:register(t0);SamplerState imageSampler:register(s0);
float4 pixelMain(Output input):SV_TARGET{float4 c=image.Sample(imageSampler,input.uv)*baseColor;float light=.38+.62*saturate(dot(normalize(input.n),normalize(float3(.35,.55,.8))));float3 lit=c.rgb*light;float rim=pow(1-saturate(abs(normalize(input.n).z)),3)*.12;return float4(float3(.12,.13,.15)+lit*.82+rim,1);}
)HLSL";
        ID3DBlob *vertex=nullptr,*pixel=nullptr,*errors=nullptr;
        HRESULT result=D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"vertexMain","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vertex,&errors);
        if(FAILED(result)){std::string message=errors?std::string((char*)errors->GetBufferPointer(),errors->GetBufferSize()):"Vertex shader failed";release(errors);throw std::runtime_error(message);}release(errors);
        result=D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"pixelMain","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&pixel,&errors);
        if(FAILED(result)){std::string message=errors?std::string((char*)errors->GetBufferPointer(),errors->GetBufferSize()):"Pixel shader failed";release(errors);release(vertex);throw std::runtime_error(message);}release(errors);
        check(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs),"Preview vertex shader");check(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps),"Preview pixel shader");
        D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
        check(device->CreateInputLayout(elements,3,vertex->GetBufferPointer(),vertex->GetBufferSize(),&layout),"Preview input layout");release(vertex);release(pixel);
        D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=sizeof(Camera);buffer.Usage=D3D11_USAGE_DEFAULT;buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;check(device->CreateBuffer(&buffer,nullptr,&constants),"Preview camera buffer");buffer.ByteWidth=16;check(device->CreateBuffer(&buffer,nullptr,&material),"Preview material buffer");
        D3D11_SAMPLER_DESC sample{};sample.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sample.AddressU=sample.AddressV=sample.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sample.MaxLOD=D3D11_FLOAT32_MAX;check(device->CreateSamplerState(&sample,&sampler),"Preview sampler");
        D3D11_RASTERIZER_DESC raster_desc{};raster_desc.FillMode=D3D11_FILL_SOLID;raster_desc.CullMode=D3D11_CULL_NONE;raster_desc.DepthClipEnable=TRUE;check(device->CreateRasterizerState(&raster_desc,&raster),"Preview rasterizer");
        D3D11_DEPTH_STENCIL_DESC depth_desc{};depth_desc.DepthEnable=TRUE;depth_desc.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;depth_desc.DepthFunc=D3D11_COMPARISON_LESS;check(device->CreateDepthStencilState(&depth_desc,&depth_state),"Preview depth state");
        D3D11_BLEND_DESC blend_desc{};blend_desc.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;check(device->CreateBlendState(&blend_desc,&blend),"Preview blend state");
        uint32_t white_pixel=0xffffffff;D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=desc.ArraySize=desc.MipLevels=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.Usage=D3D11_USAGE_IMMUTABLE;
        ID3D11Texture2D* white_image=nullptr;D3D11_SUBRESOURCE_DATA initial{&white_pixel,4,0};check(device->CreateTexture2D(&desc,&initial,&white_image),"Default material");check(device->CreateShaderResourceView(white_image,nullptr,&white),"Default material view");release(white_image);return true;
    }catch(const std::exception& error){pipeline_error=error.what();return false;}
}
const Frame& render(int index,int width,int height,const View& view){
    index=index==1?1:0;width=std::clamp(width,64,1600);height=std::clamp(height,64,1600);
    if(frame.texture&&frame.error.empty()&&index==cached_index&&width==cached_width&&height==cached_height&&view.yaw==cached_view.yaw&&view.pitch==cached_view.pitch&&view.zoom==cached_view.zoom)return frame;
    frame={};cached_index=index;cached_width=width;cached_height=height;cached_view=view;
    frame.label=index==0?"Warden":"SAS";
    if(!pipeline_error.empty()||!device||!vs){frame.error=pipeline_error.empty()?"Model renderer unavailable":pipeline_error;return frame;}
    Model& model=models[index];load(model,index);if(!model.error.empty()){frame.error=model.error;return frame;}
    try{resize(width,height);}catch(const std::exception& error){frame.error=error.what();return frame;}
    float model_height=model.maximum.y-model.minimum.y;
    Camera camera{{(model.minimum.x+model.maximum.x)/2,(model.minimum.y+model.maximum.y)/2,(model.minimum.z+model.maximum.z)/2,model_height*.60f},
        {std::cos(view.yaw),std::sin(view.yaw),std::cos(view.pitch),std::sin(view.pitch)},{view.zoom,(float)width/height,model_height*4,0}};
    ID3D11RenderTargetView* old_target=nullptr;ID3D11DepthStencilView* old_depth=nullptr;context->OMGetRenderTargets(1,&old_target,&old_depth);
    UINT old_count=16;D3D11_VIEWPORT old_viewports[16];context->RSGetViewports(&old_count,old_viewports);
    context->OMSetRenderTargets(1,&target_view,depth_view);float clear[4]={.045f,.045f,.062f,1};context->ClearRenderTargetView(target_view,clear);context->ClearDepthStencilView(depth_view,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
    D3D11_VIEWPORT viewport{0,0,(float)width,(float)height,0,1};context->RSSetViewports(1,&viewport);context->RSSetState(raster);
    context->OMSetBlendState(blend,nullptr,0xffffffff);context->OMSetDepthStencilState(depth_state,0);
    UINT stride=sizeof(Vertex),offset=0;context->IASetVertexBuffers(0,1,&model.vertices,&stride,&offset);context->IASetIndexBuffer(model.indices,DXGI_FORMAT_R32_UINT,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetInputLayout(layout);
    context->VSSetShader(vs,nullptr,0);context->PSSetShader(ps,nullptr,0);context->GSSetShader(nullptr,nullptr,0);
    context->UpdateSubresource(constants,0,nullptr,&camera,0,0);context->VSSetConstantBuffers(0,1,&constants);context->PSSetConstantBuffers(0,1,&constants);context->PSSetConstantBuffers(1,1,&material);context->PSSetSamplers(0,1,&sampler);
    for(auto part:model.parts){ID3D11ShaderResourceView* image=part.texture<0?white:model.textures[part.texture];context->UpdateSubresource(material,0,nullptr,part.color,0,0);context->PSSetShaderResources(0,1,&image);context->DrawIndexed(part.count,part.first,0);}
    ID3D11ShaderResourceView* none=nullptr;context->PSSetShaderResources(0,1,&none);
    context->OMSetRenderTargets(1,&old_target,old_depth);context->RSSetViewports(old_count,old_viewports);release(old_target);release(old_depth);
    frame.texture=(ImTextureID)target_image;frame.triangles=model.triangles;
    frame.minimum={(float)width,(float)height};frame.maximum={0,0};
    for(auto p:model.positions){auto s=project(p,camera,width,height);frame.minimum.x=std::min(frame.minimum.x,s.x);frame.minimum.y=std::min(frame.minimum.y,s.y);frame.maximum.x=std::max(frame.maximum.x,s.x);frame.maximum.y=std::max(frame.maximum.y,s.y);}
    for(auto& bone:model.bones)frame.joints.push_back({project(bone.position,camera,width,height),bone.parent,bone.group,bone.name});
    return frame;
}
void shutdown(){
    frame={};cached_index=-1;
    for(auto& model:models){release(model.vertices);release(model.indices);for(auto*& image:model.textures)release(image);model=Model{};}
    release(target_image);release(target_view);release(target);release(depth_view);release(depth);target_width=target_height=0;
    release(vs);release(ps);release(layout);release(constants);release(material);release(sampler);release(raster);release(depth_state);release(blend);release(white);device=nullptr;context=nullptr;
}
}

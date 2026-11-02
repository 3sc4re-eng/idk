#include <model_preview.h>
#include <wincodec.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
static void ok(HRESULT hr){if(FAILED(hr)){std::printf("HRESULT %08x\n",(unsigned)hr);std::abort();}}
static void save(ID3D11Device* device,ID3D11DeviceContext* context,ImTextureID image,const wchar_t* path){
    ID3D11Resource* resource=nullptr;((ID3D11ShaderResourceView*)image)->GetResource(&resource);ID3D11Texture2D* source=nullptr;ok(resource->QueryInterface(IID_PPV_ARGS(&source)));resource->Release();
    D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);desc.BindFlags=desc.MiscFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* copy=nullptr;ok(device->CreateTexture2D(&desc,nullptr,&copy));context->CopyResource(copy,source);D3D11_MAPPED_SUBRESOURCE mapped{};ok(context->Map(copy,0,D3D11_MAP_READ,0,&mapped));
    std::vector<BYTE> pixels(desc.Width*desc.Height*4);unsigned visible=0;
    for(UINT y=0;y<desc.Height;y++){
        auto* row=(BYTE*)mapped.pData+y*mapped.RowPitch;std::memcpy(pixels.data()+y*desc.Width*4,row,desc.Width*4);
        for(UINT x=0;x<desc.Width;x++)if(row[x*4]>20||row[x*4+1]>20||row[x*4+2]>24)visible++;
    }
    assert(visible>5000);context->Unmap(copy,0);copy->Release();source->Release();
    IWICImagingFactory* factory=nullptr;ok(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    IWICBitmap* bitmap=nullptr;ok(factory->CreateBitmapFromMemory(desc.Width,desc.Height,GUID_WICPixelFormat32bppRGBA,desc.Width*4,(UINT)pixels.size(),pixels.data(),&bitmap));
    IWICStream* stream=nullptr;ok(factory->CreateStream(&stream));ok(stream->InitializeFromFilename(path,GENERIC_WRITE));
    IWICBitmapEncoder* encoder=nullptr;ok(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));ok(encoder->Initialize(stream,WICBitmapEncoderNoCache));
    IWICBitmapFrameEncode* frame=nullptr;IPropertyBag2* properties=nullptr;ok(encoder->CreateNewFrame(&frame,&properties));ok(frame->Initialize(properties));ok(frame->SetSize(desc.Width,desc.Height));
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;ok(frame->SetPixelFormat(&format));IWICFormatConverter* converter=nullptr;ok(factory->CreateFormatConverter(&converter));
    ok(converter->Initialize(bitmap,format,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));ok(frame->WriteSource(converter,nullptr));ok(frame->Commit());ok(encoder->Commit());
    converter->Release();frame->Release();if(properties)properties->Release();encoder->Release();stream->Release();bitmap->Release();factory->Release();std::printf("Rendered %u non-background pixels\n",visible);
}
int main(){
    ok(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;D3D_FEATURE_LEVEL feature;
    ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&feature,&context));assert(game_model::initialize(device,context));
    for(int model=0;model<2;model++){
        const auto& frame=game_model::render(model,480,640,{});
        if(!frame.error.empty()){std::printf("Model error: %s\n",frame.error.c_str());return 1;}
        assert(frame.texture&&frame.triangles>10000&&frame.joints.size()>=20);assert(frame.minimum.y>=0&&frame.maximum.y<=640);
        for(auto joint:frame.joints){assert(joint.group>=0&&joint.group<7&&joint.parent<(int)frame.joints.size());}
        save(device,context,frame.texture,model==0?L"warden-model.png":L"sas-model.png");std::printf("%s: %u triangles, %zu actual joints\n",frame.label.c_str(),frame.triangles,frame.joints.size());
        auto before=frame.joints.front().screen;game_model::View turned;turned.yaw=.8f;const auto& rotated=game_model::render(model,480,640,turned);auto after=rotated.joints.front().screen;
        assert(std::fabs(before.x-after.x)+std::fabs(before.y-after.y)>.01f);
    }
    game_model::shutdown();context->Release();device->Release();CoUninitialize();std::puts("PASS: actual mesh decode, material rendering, skeleton projection, rotation and both game models");return 0;
}

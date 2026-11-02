#pragma once
#include "imgui.h"
#include "ability_art.h"
#include <d3d11.h>
#include <wincodec.h>
#include <map>
#include <vector>
namespace ability_icons{
inline std::map<unsigned,ID3D11ShaderResourceView*> textures;
inline void shutdown(){for(auto& p:textures)if(p.second)p.second->Release();textures.clear();}
inline ImTextureID get(unsigned token){auto it=textures.find(token);return it==textures.end()?nullptr:(ImTextureID)it->second;}
inline void initialize(ID3D11Device* device){
    shutdown();IWICImagingFactory* factory=nullptr;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return;
    for(auto& image:ability_art::images){
        IWICStream* stream=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;IWICFormatConverter* convert=nullptr;ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;
        HRESULT hr=factory->CreateStream(&stream);if(SUCCEEDED(hr))hr=stream->InitializeFromMemory((BYTE*)ability_art::pixels+image.offset,image.size);
        if(SUCCEEDED(hr))hr=factory->CreateDecoderFromStream(stream,nullptr,WICDecodeMetadataCacheOnLoad,&decoder);if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
        if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&convert);if(SUCCEEDED(hr))hr=convert->Initialize(frame,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
        UINT w=0,h=0;if(SUCCEEDED(hr))hr=convert->GetSize(&w,&h);
        if(SUCCEEDED(hr)&&w&&h&&w<=1024&&h<=1024){std::vector<BYTE> pixels(w*h*4);hr=convert->CopyPixels(nullptr,w*4,(UINT)pixels.size(),pixels.data());if(SUCCEEDED(hr)){D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA input{pixels.data(),w*4,0};hr=device->CreateTexture2D(&desc,&input,&texture);if(SUCCEEDED(hr))device->CreateShaderResourceView(texture,nullptr,&view);}}
        if(view)textures[image.token]=view;if(texture)texture->Release();if(convert)convert->Release();if(frame)frame->Release();if(decoder)decoder->Release();if(stream)stream->Release();
    }factory->Release();
}
}

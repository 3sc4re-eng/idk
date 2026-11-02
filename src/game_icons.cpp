#include "game_icons.h"
#include "game_art.h"
#include <wincodec.h>
#include <vector>
namespace game_icons{
static ID3D11ShaderResourceView* images[5]{};static unsigned widths[5]{},heights[5]{};static bool uninitialize=false;
void shutdown(){for(auto& image:images){if(image)image->Release();image=nullptr;}if(uninitialize)CoUninitialize();uninitialize=false;}
void initialize(ID3D11Device* device){
    shutdown();uninitialize=SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
    for(int i=0;i<5;i++){
        IWICImagingFactory* factory=nullptr;IWICStream* stream=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;IWICFormatConverter* converter=nullptr;ID3D11Texture2D* texture=nullptr;
        HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
        if(SUCCEEDED(hr))hr=factory->CreateStream(&stream);if(SUCCEEDED(hr))hr=stream->InitializeFromMemory((BYTE*)game_art::images[i].data,(DWORD)game_art::images[i].size);
        if(SUCCEEDED(hr))hr=factory->CreateDecoderFromStream(stream,nullptr,WICDecodeMetadataCacheOnLoad,&decoder);if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
        if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&converter);if(SUCCEEDED(hr))hr=converter->Initialize(frame,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
        UINT width=0,height=0;if(SUCCEEDED(hr))hr=converter->GetSize(&width,&height);
        if(SUCCEEDED(hr)&&width&&height&&width<=2048&&height<=2048){widths[i]=width;heights[i]=height;std::vector<unsigned char> pixels(width*height*4);hr=converter->CopyPixels(nullptr,width*4,(UINT)pixels.size(),pixels.data());
            if(SUCCEEDED(hr)){for(size_t pixel=0;pixel<pixels.size();pixel+=4){for(int channel=0;channel<3;channel++)pixels[pixel+channel]=(unsigned char)(32+(unsigned)pixels[pixel+channel]*223/255);pixels[pixel+3]=255;}D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{pixels.data(),width*4,0};hr=device->CreateTexture2D(&desc,&data,&texture);if(SUCCEEDED(hr))device->CreateShaderResourceView(texture,nullptr,&images[i]);}
        }
        if(texture)texture->Release();if(converter)converter->Release();if(frame)frame->Release();if(decoder)decoder->Release();if(stream)stream->Release();if(factory)factory->Release();
    }
}
ImTextureID get(int index){return index>=0&&index<5?(ImTextureID)images[index]:nullptr;}
void dimensions(int index,unsigned& width,unsigned& height){width=index>=0&&index<5?widths[index]:0;height=index>=0&&index<5?heights[index]:0;}
}

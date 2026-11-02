#pragma once
#include "imgui.h"
#include <d3d11.h>
#include <string>
#include <vector>
namespace game_model{
struct View{float yaw=-.22f,pitch=0,zoom=1;};
struct Joint{ImVec2 screen;int parent=-1,group=-1;std::string name;};
struct Frame{ImTextureID texture=nullptr;std::vector<Joint> joints;ImVec2 minimum{},maximum{};std::string error,label;unsigned triangles=0;};
bool initialize(ID3D11Device*,ID3D11DeviceContext*);
const Frame& render(int model,int width,int height,const View&);
void shutdown();
}

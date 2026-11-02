#pragma once
#include "imgui.h"
#include <d3d11.h>
namespace game_icons{void initialize(ID3D11Device* device);void shutdown();ImTextureID get(int index);
void dimensions(int index,unsigned& width,unsigned& height);}

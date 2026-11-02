#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

// Version-specific integration with the ImGui already shipped by ACE. No
// proprietary UI code/assets. Never skip Begin/ItemAdd/widget submissions.
namespace cooperative_capture {
namespace detail {
enum class WindowOwnership {outside,inside,invalid};
template<class Window>constexpr WindowOwnership menu_ownership(const Window* window,const Window* root){
    if(!root)return WindowOwnership::invalid;
    // ParentWindowInBeginStack is set on the first Begin each frame and includes
    // tooltips/popups, whose RootWindow alone need not be the main menu root.
    for(unsigned depth=0;window&&depth<4096;++depth){if(window==root)return WindowOwnership::inside;window=window->ParentWindowInBeginStack;}
    return window?WindowOwnership::invalid:WindowOwnership::outside;
}
}
class MenuDrawData {
public:
    ImDrawData data;
    bool without_menu(ImDrawData* source){
        static_assert(IMGUI_VERSION_NUM==19040,"Revalidate menu draw ownership for a changed ImGui version");
        auto* context=ImGui::GetCurrentContext();if(!context)return false;
        auto* root=ImGui::FindWindowByName("##AceSoftware");
        if(!source||!source->Valid||!root||root->LastFrameActive!=context->FrameCount||root->BeginCount<=0||
           context->Windows.Size>4096||source->CmdListsCount<0||source->CmdListsCount>8192||source->CmdListsCount!=source->CmdLists.Size)return false;
        try {
            std::vector<const ImDrawList*> excluded;excluded.reserve(static_cast<std::size_t>(context->Windows.Size));
            for(auto* window:context->Windows){
                if(window->LastFrameActive!=context->FrameCount||window->BeginCount<=0)continue;
                const auto ownership=detail::menu_ownership(window,root);
                if(ownership==detail::WindowOwnership::invalid)return false;
                if(ownership==detail::WindowOwnership::inside)excluded.push_back(window->DrawList);
            }
            if(std::find(excluded.begin(),excluded.end(),root->DrawList)==excluded.end())return false;
            data.Clear();data.DisplayPos=source->DisplayPos;data.DisplaySize=source->DisplaySize;
            data.FramebufferScale=source->FramebufferScale;data.OwnerViewport=source->OwnerViewport;
            data.CmdLists.reserve(source->CmdListsCount);
            std::int64_t vertices=0,indices=0;bool root_removed=false;
            for(auto* list:source->CmdLists){
                if(!list)return false;
                if(std::find(excluded.begin(),excluded.end(),list)!=excluded.end()){if(list==root->DrawList)root_removed=true;continue;}
                // Preserve original order/commands/buffers/callbacks. Only this
                // pointer list and its totals differ; original draw data is intact.
                vertices+=list->VtxBuffer.Size;indices+=list->IdxBuffer.Size;
                if(vertices>std::numeric_limits<int>::max()||indices>std::numeric_limits<int>::max())return false;
                data.CmdLists.push_back(list);
            }
            if(!root_removed)return false;
            data.TotalVtxCount=static_cast<int>(vertices);data.TotalIdxCount=static_cast<int>(indices);
            data.CmdListsCount=data.CmdLists.Size;data.Valid=true;return true;
        }catch(...){return false;}
    }
};
}

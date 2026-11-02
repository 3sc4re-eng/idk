#include <set>
#define _USE_MATH_DEFINES
#define main model_render_fixture_entry
#include "render_model_check.cpp"
#undef main
#include "fixture/overlay_widgets_fixture.h"
Snapshots snap;std::mutex snap_mtx;
int main(){
    ok(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;D3D_FEATURE_LEVEL level;
    ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));assert(game_model::initialize(device,context));
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={1000,680};io.DeltaTime=.016f;io.IniFilename=nullptr;aceui::fonts();aceui::style();assert(ImGui_ImplDX11_Init(device,context));
    cfg.register_all();overlay::edit_game="Deadlock";overlay::ui_device=device;
    cfg.find("aim.smooth")->f=13;cfg.dirty=true;overlay::switch_profile_game("Counter-Strike 2");assert(overlay::profile_path("rifles")=="configs/cs2_rifles.json");assert(cfg.F("aim.smooth")==33);cfg.find("aim.smooth")->f=19;overlay::switch_profile_game("Deadlock");assert(cfg.F("aim.smooth")==13&&cfg.dirty);overlay::switch_profile_game("Counter-Strike 2");assert(cfg.F("aim.smooth")==19);overlay::switch_profile_game("Deadlock");

    assert(std::string(overlay::key_name(VK_OEM_COMMA))=="Comma");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=1000;desc.Height=680;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D* target=nullptr;ID3D11RenderTargetView* view=nullptr;ID3D11ShaderResourceView* image=nullptr;ok(device->CreateTexture2D(&desc,nullptr,&target));ok(device->CreateRenderTargetView(target,nullptr,&view));ok(device->CreateShaderResourceView(target,nullptr,&image));
    overlay::ModelCanvas model;int mode=0;ImGuiID head_priority=0;
    auto frame=[&](){
        context->OMSetRenderTargets(1,&view,nullptr);float clear[]={.047f,.047f,.063f,1};context->ClearRenderTargetView(view,clear);
        ImGui_ImplDX11_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowPos({12,12});ImGui::SetNextWindowSize({976,656});
        ImGui::Begin("Actual product UI fixtures",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
        ImGui::SetCursorPos({20,20});aceui::panel("Controls",{440,610});
        overlay::section(mode?"RENDERING":"AIMING");
        if(mode){overlay::items({"esp.box","esp.health","esp.tracers","esp.head_circle","esp.name","esp.souls","esp.distance","esp.ultimate","esp.abilities"});}
        else{overlay::items({"aim.smooth","aim.curve","aim.skip_fast"});overlay::draw_pair("Reaction delay","aim.act_delay_min","aim.act_delay_max",1000);}
        ImGui::EndChild();ImGui::SameLine(0,16);aceui::panel("Real model",{470,610});
        if(mode==2)overlay::draw_esp_builder();else if(mode==1)model=overlay::model_canvas(430,false);else{ImGui::PushID(0);head_priority=ImGui::GetID("##priority");ImGui::PopID();overlay::aim_points();overlay::draw_aim_figure();}
        ImGui::EndChild();ImGui::End();ImGui::Render();context->OMSetRenderTargets(1,&view,nullptr);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    };
    frame();frame();assert(head_priority!=0);
    const char* choices[]={"Medium","Low","Ignore","High"};const int values[]={1,0,-1,2};
    for(int i=0;i<4;i++){
        GImGui->NavNextActivateId=head_priority;GImGui->NavNextActivateFlags=ImGuiActivateFlags_None;frame();
        auto* popup=ImGui::FindWindowByName("##Combo_00");assert(popup&&popup->Active);
        GImGui->NavNextActivateId=popup->GetID(choices[i]);GImGui->NavNextActivateFlags=ImGuiActivateFlags_None;frame();assert(cfg.I("aim.bone_head")==values[i]);
    }
    save(device,context,(ImTextureID)image,L"ui-aim-settings.png");
    mode=1;cfg.find("esp.preview_joints")->b=false;frame();frame();assert(model.frame.texture&&model.frame.joints.size()>=20);int bare=ImGui::GetDrawData()->TotalVtxCount;
    cfg.find("esp.preview_joints")->b=true;frame();assert(ImGui::GetDrawData()->TotalVtxCount==bare); // Legacy saved true must not restore the removed debug joints.
    ImVec2 point{model.origin.x+model.width*.5f,model.origin.y+model.height*.5f};float before=overlay::model_views[0].yaw;io.AddMousePosEvent(point.x,point.y);frame();io.AddMouseButtonEvent(1,true);frame();io.AddMousePosEvent(point.x+40,point.y);frame();assert(std::fabs(overlay::model_views[0].yaw-before)>.1f);io.AddMouseButtonEvent(1,false);frame();
    overlay::model_views[0]={};io.AddMousePosEvent(-100,-100);mode=2;cfg.find("esp.box_style")->i=2;cfg.find("esp.health_style")->i=2;cfg.find("esp.abilities")->b=true;cfg.find("esp.ultimate_position")->i=2;cfg.find("esp.abilities_position")->i=1;frame();frame();save(device,context,(ImTextureID)image,L"ui-esp-builder.png");
    game_model::shutdown();ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();image->Release();view->Release();target->Release();context->Release();device->Release();CoUninitialize();
    std::puts("PASS: priority dropdown activation persists real settings, FOV/deadzone diagram renders, legacy preview-joints flag adds no drawing and the model rotates, and five-element ESP builder renders through D3D11");
}

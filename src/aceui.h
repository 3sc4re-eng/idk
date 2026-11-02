#pragma once
#include "imgui.h"
#include <cmath>
#include <algorithm>
namespace aceui {
inline ImFont *body=nullptr, *small=nullptr, *heading=nullptr, *brand=nullptr;
inline ImVec4 accent{0.55f,0.48f,0.98f,1};
inline ImU32 purple=IM_COL32(141,123,250,255);
inline ImU32 muted=IM_COL32(129,132,151,255);
inline ImU32 white=IM_COL32(235,235,245,255);
inline void fonts(){
    auto& io=ImGui::GetIO();ImFontConfig font_config;font_config.OversampleH=3;font_config.OversampleV=2;font_config.PixelSnapH=false;
    body=io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf",14,&font_config);
    if(!body) body=io.Fonts->AddFontDefault();
    small=io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf",12,&font_config);
    heading=io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf",18,&font_config);
    brand=io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf",20,&font_config);
    if(!small) small=body; if(!heading) heading=body; if(!brand) brand=body;
    io.FontDefault=body;
}
inline void style(){
    auto& s=ImGui::GetStyle(); ImGui::StyleColorsDark();
    s.WindowPadding={0,0}; s.FramePadding={7,4}; s.ItemSpacing={7,3};
    s.ItemInnerSpacing={7,3}; s.WindowRounding=4; s.ChildRounding=3;
    s.FrameRounding=2; s.PopupRounding=4; s.GrabRounding=2; s.ScrollbarRounding=3;
    s.WindowBorderSize=1; s.ChildBorderSize=1; s.FrameBorderSize=0;
    s.ScrollbarSize=8; s.GrabMinSize=12;
    auto* c=s.Colors;
    c[ImGuiCol_Text]={.92f,.92f,.96f,1}; c[ImGuiCol_TextDisabled]={.51f,.52f,.60f,1};
    c[ImGuiCol_WindowBg]={.055f,.063f,.078f,1}; c[ImGuiCol_ChildBg]={.075f,.085f,.105f,1};
    c[ImGuiCol_PopupBg]={.09f,.09f,.12f,1}; c[ImGuiCol_Border]={.13f,.13f,.17f,1};
    c[ImGuiCol_FrameBg]={.105f,.12f,.145f,1}; c[ImGuiCol_FrameBgHovered]={.15f,.14f,.21f,1};
    c[ImGuiCol_FrameBgActive]={.19f,.17f,.28f,1};
    c[ImGuiCol_Button]={.14f,.12f,.23f,1}; c[ImGuiCol_ButtonHovered]={.23f,.19f,.37f,1};
    c[ImGuiCol_ButtonActive]={.32f,.26f,.51f,1};
    c[ImGuiCol_CheckMark]=accent; c[ImGuiCol_SliderGrab]=accent;
    c[ImGuiCol_SliderGrabActive]={.72f,.65f,1,1};
    c[ImGuiCol_Header]={.18f,.15f,.28f,1}; c[ImGuiCol_HeaderHovered]={.24f,.20f,.37f,1};
    c[ImGuiCol_HeaderActive]=c[ImGuiCol_HeaderHovered];
    c[ImGuiCol_Separator]={.14f,.14f,.18f,1}; c[ImGuiCol_TextSelectedBg]={.45f,.35f,.8f,.4f};
    c[ImGuiCol_ScrollbarBg]={0,0,0,0}; c[ImGuiCol_ScrollbarGrab]={.24f,.23f,.32f,1};
}
inline void theme(ImVec4 a,ImVec4 bg,ImVec4 panel,ImVec4 text){
    accent=a;purple=ImGui::ColorConvertFloat4ToU32(a);white=ImGui::ColorConvertFloat4ToU32(text);
    auto* c=ImGui::GetStyle().Colors;
    const auto mix=[](ImVec4 base,ImVec4 tint,float amount){return ImVec4{base.x+(tint.x-base.x)*amount,base.y+(tint.y-base.y)*amount,base.z+(tint.z-base.z)*amount,1};};
    c[ImGuiCol_WindowBg]=bg;c[ImGuiCol_ChildBg]=panel;c[ImGuiCol_Text]=text;
    // Popup and control surfaces follow the same chosen panel/text pair.
    c[ImGuiCol_PopupBg]={panel.x,panel.y,panel.z,1};
    c[ImGuiCol_TextDisabled]=mix(panel,text,.60f);muted=ImGui::ColorConvertFloat4ToU32(c[ImGuiCol_TextDisabled]);
    c[ImGuiCol_Border]={text.x,text.y,text.z,.10f};c[ImGuiCol_Separator]=c[ImGuiCol_Border];
    c[ImGuiCol_FrameBg]=mix(panel,text,.04f);
    c[ImGuiCol_FrameBgHovered]=mix(panel,a,.15f);
    c[ImGuiCol_FrameBgActive]=mix(panel,a,.23f);
    c[ImGuiCol_CheckMark]=a;c[ImGuiCol_SliderGrab]=a;c[ImGuiCol_SliderGrabActive]=a;
    c[ImGuiCol_Button]=mix(panel,a,.10f);
    c[ImGuiCol_ButtonHovered]=mix(panel,a,.24f);c[ImGuiCol_ButtonActive]=mix(panel,a,.32f);
    c[ImGuiCol_TextSelectedBg]=mix(panel,a,.30f);
    c[ImGuiCol_Header]=c[ImGuiCol_Button];c[ImGuiCol_HeaderHovered]=c[ImGuiCol_ButtonHovered];c[ImGuiCol_HeaderActive]=c[ImGuiCol_ButtonActive];
}
inline void text(ImDrawList* d,ImFont* f,ImVec2 p,ImU32 c,const char* t){d->AddText(f,f->FontSize*ImGui::GetIO().FontGlobalScale,p,c,t);}
inline void logo(ImDrawList* d,ImVec2 p,float size){
    d->AddRectFilled(p,{p.x+size,p.y+size},IM_COL32(37,30,61,255),size*.24f);
    const float x=p.x+size*.5f,y=p.y+size*.48f,r=size*.23f;
    d->AddCircleFilled({x-r*.46f,y+r*.18f},r*.53f,purple,24);
    d->AddCircleFilled({x+r*.46f,y+r*.18f},r*.53f,purple,24);
    d->AddTriangleFilled({x,y-r},{x-r*.93f,y+r*.1f},{x+r*.93f,y+r*.1f},purple);
    d->AddTriangleFilled({x,y+r*.3f},{x-r*.35f,y+r*1.13f},{x+r*.35f,y+r*1.13f},purple);
}
inline void caption(const char* t){ImGui::PushFont(small);ImGui::TextDisabled("%s",t);ImGui::PopFont();}
inline void section(const char* t){
    ImGui::Dummy({0,4}); ImGui::PushFont(small);ImGui::TextColored(accent,"%s",t);ImGui::PopFont();
    ImGui::Separator(); ImGui::Dummy({0,2});
}
inline void panel(const char* id,ImVec2 size){
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{14,12});
    ImGui::BeginChild(id,size,true,ImGuiWindowFlags_AlwaysVerticalScrollbar); ImGui::PopStyleVar();
}
inline bool toggle(const char* id,bool& value){
    const ImVec2 p=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton(id,{28,20});if(clicked)value=!value;
    auto* d=ImGui::GetWindowDrawList();
    d->AddRectFilled({p.x,p.y+3},{p.x+28,p.y+17},ImGui::GetColorU32(value?purple:IM_COL32(66,67,72,255)),7);
    d->AddCircleFilled({p.x+(value?21.f:7.f),p.y+10},5,ImGui::GetColorU32(IM_COL32(244,242,255,255)),20);
    if(ImGui::IsItemHovered())d->AddRect({p.x,p.y+3},{p.x+28,p.y+17},ImGui::GetColorU32(IM_COL32(192,180,255,150)),7);
    return clicked;
}
inline void status(const char* label,bool ok){
    auto p=ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddCircleFilled({p.x+4,p.y+8},3,ok?IM_COL32(101,211,166,255):IM_COL32(224,169,105,255));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+15);ImGui::TextDisabled("%s",label);
}
inline bool nav(int id,const char* title,const char* /*sub*/,bool selected,float width){
    ImGui::PushID(id);const auto p=ImGui::GetCursorScreenPos();const float height=std::max(30.f,ImGui::GetFrameHeight()+4.f);
    const bool clicked=ImGui::InvisibleButton("nav",{width,height});auto* d=ImGui::GetWindowDrawList();const bool hovered=ImGui::IsItemHovered();
    if(selected||hovered)d->AddRectFilled(p,{p.x+width,p.y+height},ImGui::GetColorU32(selected?ImGuiCol_HeaderActive:ImGuiCol_FrameBgHovered),2);
    if(selected)d->AddRectFilled({p.x,p.y+7},{p.x+3,p.y+height-7},purple,1);
    const ImU32 ink=selected?purple:muted;const ImVec2 c{p.x+17,p.y+height*.5f};
    if(id==0||id==1){d->AddCircle(c,5,ink,20,1.3f);d->AddLine({c.x-8,c.y},{c.x-3,c.y},ink);d->AddLine({c.x+3,c.y},{c.x+8,c.y},ink);d->AddLine({c.x,c.y-8},{c.x,c.y-3},ink);d->AddLine({c.x,c.y+3},{c.x,c.y+8},ink);}
    else if(id==2){d->AddRect({c.x-7,c.y-5},{c.x+7,c.y+5},ink,3,0,1.3f);d->AddCircle(c,2,ink,12,1.3f);}
    else if(id==4){d->AddCircle(c,7,ink,20,1.2f);d->AddCircle(c,3,ink,16);d->AddLine(c,{c.x+5,c.y-5},ink,1.3f);}
    else if(id==9){d->AddLine({c.x-6,c.y+6},{c.x+6,c.y-6},ink,1.7f);d->AddLine({c.x+1,c.y-6},{c.x+6,c.y-6},ink);d->AddLine({c.x+6,c.y-1},{c.x+6,c.y-6},ink);}
    else if(id==5){d->AddRect({c.x-6,c.y-7},{c.x+6,c.y+7},ink,2,0,1.3f);d->AddLine({c.x-3,c.y-2},{c.x+3,c.y-2},ink);d->AddLine({c.x-3,c.y+2},{c.x+3,c.y+2},ink);}
    else{for(int row=-1;row<=1;++row){const float y=c.y+row*5;d->AddLine({c.x-7,y},{c.x+7,y},ink,1.2f);d->AddCircleFilled({c.x+(row==0?3.f:-3.f),y},2,ink,12);}}
    const float font_height=body->FontSize*ImGui::GetIO().FontGlobalScale;
    d->PushClipRect({p.x+32,p.y},{p.x+width-7,p.y+height},true);text(d,body,{p.x+32,p.y+(height-font_height)*.5f},selected?white:ImGui::GetColorU32(ImGuiCol_TextDisabled),title);d->PopClipRect();
    if(hovered&&ImGui::CalcTextSize(title).x>width-39)ImGui::SetTooltip("%s",title);
    ImGui::PopID();return clicked;
}
}

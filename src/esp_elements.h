#pragma once
#include "esp_layout.h"
#include "esp_text.h"
#include "ability_tracking.h"
#include "ability_icons.h"
#include <array>
#include <algorithm>
#include <cstdio>
#include <string>
namespace esp_elements{
enum Id{Name,Souls,Distance,Ultimate,Abilities,Count};
inline constexpr int default_sides[]={0,3,1,0,1};
inline constexpr const char* features[]={"name","souls","distance","ultimate","abilities"};
inline constexpr const char* titles[]={"Name","Souls","Distance","Ultimate ready","Abilities"};
inline constexpr const char* enabled_keys[]={"esp.name","esp.souls","esp.distance","esp.ultimate","esp.abilities"};
inline constexpr const char* position_keys[]={"esp.name_position","esp.souls_position","esp.distance_position","esp.ultimate_position","esp.abilities_position"};
struct Block{bool enabled=false;int side=0;ImVec2 position{},size{};};
using Blocks=std::array<Block,Count>;
inline Blocks place(const std::array<bool,Count>& enabled,const std::array<ImVec2,Count>& sizes,float x1,float y1,float x2,float y2){
    Blocks blocks{};float lanes[4]={};
    for(int i=0;i<Count;i++){auto& b=blocks[i];b.enabled=enabled[i];b.side=std::clamp(cfg.I(position_keys[i]),0,3);b.size=sizes[i];if(b.enabled)b.position=esp_layout::stacked(b.side,x1,y1,x2,y2,b.size,lanes[b.side]);}
    float side_end=y1+std::max(lanes[2],lanes[3]);float bottom_shift=std::max(0.f,side_end+4-(y2+7));
    for(auto& b:blocks)if(b.enabled&&b.side==1)b.position.y+=bottom_shift;
    return blocks;
}
// Fit each complete lane as a group, retaining spacing when the model is zoomed.
inline void fit_preview(Blocks& blocks,ImVec2 lower,ImVec2 upper){
    struct Group{bool any=false;ImVec2 lo{1e9f,1e9f},hi{-1e9f,-1e9f};float height=0;};std::array<Group,4> groups{};
    for(auto& b:blocks)if(b.enabled){auto& g=groups[b.side];g.any=true;g.lo.x=std::min(g.lo.x,b.position.x);g.lo.y=std::min(g.lo.y,b.position.y);g.hi.x=std::max(g.hi.x,b.position.x+b.size.x);g.hi.y=std::max(g.hi.y,b.position.y+b.size.y);}
    for(int side=0;side<4;side++){auto& g=groups[side];if(!g.any)continue;g.height=g.hi.y-g.lo.y;float dx=0;if(g.lo.x<lower.x)dx=lower.x-g.lo.x;if(g.hi.x+dx>upper.x)dx=upper.x-g.hi.x;for(auto& b:blocks)if(b.enabled&&b.side==side)b.position.x+=dx;g.lo.x+=dx;g.hi.x+=dx;}
    bool split_sides=groups[2].any&&groups[3].any&&groups[2].hi.x+4>groups[3].lo.x&&groups[3].hi.x+4>groups[2].lo.x;
    float side_height=split_sides?groups[2].height+groups[3].height+4:std::max(groups[2].height,groups[3].height);
    float top_height=groups[0].height,bottom_height=groups[1].height;bool any_side=groups[2].any||groups[3].any;
    float gap_top=groups[0].any&&(any_side||groups[1].any)?4.f:0.f,gap_bottom=any_side&&groups[1].any?4.f:0.f;
    float top=groups[0].any?clampf(groups[0].lo.y,lower.y,std::max(lower.y,upper.y-top_height-side_height-bottom_height-gap_top-gap_bottom)):lower.y;
    float side_min=groups[0].any?top+top_height+gap_top:lower.y;
    float preferred_side=groups[2].any?groups[2].lo.y:groups[3].lo.y;
    float sides=any_side?clampf(preferred_side,side_min,std::max(side_min,upper.y-side_height-bottom_height-gap_bottom)):side_min;
    float bottom_min=any_side?sides+side_height+gap_bottom:side_min;
    float bottom=groups[1].any?clampf(groups[1].lo.y,bottom_min,std::max(bottom_min,upper.y-bottom_height)):bottom_min;
    float starts[]={top,bottom,sides,split_sides?sides+groups[2].height+4:sides};
    for(auto& b:blocks)if(b.enabled)b.position.y+=starts[b.side]-groups[b.side].lo.y;
}
struct AbilityRow{
    std::array<ability_tracking::View,4> states{};std::array<std::string,4> captions{};
    float icon=22,caption_font=10,slot_width=22,caption_height=0;ImVec2 size{};
};
inline AbilityRow ability_row(const std::array<ability_tracking::View,4>& states,float icon_size,bool show_seconds){
    AbilityRow row;row.states=states;row.icon=clampf(icon_size,14,40);row.caption_font=clampf(row.icon*.46f,10,14);row.slot_width=row.icon;
    for(int i=0;i<4;i++){
        auto state=states[i].state;char text[24]{};
        if(state==ability_tracking::State::Cooldown&&show_seconds)snprintf(text,sizeof(text),"%ds",states[i].seconds);
        else if(state==ability_tracking::State::Unknown)snprintf(text,sizeof(text),"?");
        else if(state==ability_tracking::State::Untrained)snprintf(text,sizeof(text),"-");
        else if(state==ability_tracking::State::Casting)snprintf(text,sizeof(text),"USE");
        row.captions[i]=text;
        if(*text){auto s=ImGui::GetFont()->CalcTextSizeA(row.caption_font,1e10f,0,text);row.slot_width=std::max(row.slot_width,s.x+4);row.caption_height=std::max(row.caption_height,s.y+3);}
    }
    row.size={row.slot_width*4+12,row.icon+row.caption_height};return row;
}
inline void draw_abilities(ImDrawList* d,ImVec2 origin,const AbilityRow& row,float alpha=1){
    for(int i=0;i<4;i++){
        const auto& state=row.states[i];bool ready=state.state==ability_tracking::State::Ready;float slot_x=origin.x+i*(row.slot_width+4);ImVec2 p{slot_x+(row.slot_width-row.icon)*.5f,origin.y};auto image=ability_icons::get(state.token);
        if(image)d->AddImage(image,p,{p.x+row.icon,p.y+row.icon},{0,0},{1,1},IM_COL32(255,255,255,(int)(alpha*(ready?255:150))));
        else{d->AddRectFilled(p,{p.x+row.icon,p.y+row.icon},IM_COL32(24,28,34,(int)(alpha*210)),2);char number[4];snprintf(number,sizeof(number),"%d",i+1);auto s=ImGui::CalcTextSize(number);d->AddText({p.x+(row.icon-s.x)*.5f,p.y+(row.icon-s.y)*.5f},IM_COL32(220,225,230,(int)(alpha*255)),number);}
        d->AddRect(p,{p.x+row.icon,p.y+row.icon},ready?IM_COL32(70,235,110,(int)(alpha*255)):IM_COL32(125,130,140,(int)(alpha*230)),2,0,1.5f);
        if(!row.captions[i].empty()){
            auto s=ImGui::GetFont()->CalcTextSizeA(row.caption_font,1e10f,0,row.captions[i].c_str());ImVec2 text{slot_x+(row.slot_width-s.x)*.5f,p.y+row.icon+3};
            d->AddRectFilled({text.x-2,text.y},{text.x+s.x+2,text.y+s.y},IM_COL32(8,10,14,(int)(alpha*220)),2);d->AddText(ImGui::GetFont(),row.caption_font,text,IM_COL32(255,255,255,(int)(alpha*255)),row.captions[i].c_str());
        }
    }
}
}

#pragma once
#include "imgui.h"
#include "esp_colors.h"
inline ImU32 esp_rgba(Color c,float alpha=1){return IM_COL32((int)(clampf(c.r,0,1)*255),(int)(clampf(c.g,0,1)*255),(int)(clampf(c.b,0,1)*255),(int)(clampf(c.a*alpha,0,1)*255));}
inline void esp_box(ImDrawList* d,float x1,float y1,float x2,float y2,bool visible,float alpha=1){
    if(!cfg.B("esp.box"))return;int style=cfg.I("esp.box_style");auto color=esp_rgba(esp_element_color("box",visible),alpha);float width=cfg.F("esp.box_width");
    if(style==2)d->AddRectFilled({x1,y1},{x2,y2},esp_rgba(cfg.C(visible?"esp.box_fill_visible":"esp.box_fill_hidden"),alpha));
    if(style!=1)d->AddRect({x1,y1},{x2,y2},color,0,0,width);
    else{float length=std::fmin(x2-x1,y2-y1)*.3f;for(int i=0;i<4;i++){float x=i&1?x2:x1,y=i&2?y2:y1;d->AddLine({x,y},{x+(i&1?-length:length),y},color,width);d->AddLine({x,y},{x,y+(i&2?-length:length)},color,width);}}
}
inline void esp_health(ImDrawList* d,float x1,float y1,float x2,float y2,float hp,float max_hp,bool visible,float alpha=1){
    if(!cfg.B("esp.health")||!std::isfinite(hp)||!std::isfinite(max_hp)||hp<0||max_hp<=0||hp>max_hp)return;float p=clampf(hp/max_hp,0,1);int style=cfg.I("esp.health_style");bool right=cfg.I("esp.health_position")==1;float x=right?x2+4:x1-8,h=y2-y1;
    if(style!=1){
        d->AddRectFilled({x,y1},{x+4,y2},IM_COL32(8,8,12,(int)(220*alpha)));
        d->AddRectFilled({x,y2-h*p},{x+4,y2},esp_rgba(esp_health_color(p,visible),alpha));
    }
    if(style!=0){char text[24];snprintf(text,sizeof(text),"%.0f",hp);auto size=ImGui::CalcTextSize(text);float tx=right?x+7:x-size.x-3;d->AddText({tx,y2-h*p-size.y*.5f},esp_rgba(esp_health_color(p,visible),alpha),text);}
}

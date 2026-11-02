#pragma once
#include "esp_visuals.h"
inline float esp_text_size(const char* feature){std::string p=std::string("esp.")+feature;return cfg.B((p+"_global_font").c_str())?cfg.F("esp.font_size"):cfg.F((p+"_font").c_str());}
inline void esp_text(ImDrawList* d,const char* feature,ImVec2 p,ImU32 color,const char* text){
    auto size=esp_text_size(feature);if(std::string(feature)!="souls"&&std::string(feature)!="ultimate"&&cfg.B((std::string("esp.")+feature+"_outline").c_str()))for(auto offset:{ImVec2{-1,0},ImVec2{1,0},ImVec2{0,-1},ImVec2{0,1}})d->AddText(ImGui::GetFont(),size,{p.x+offset.x,p.y+offset.y},IM_COL32(0,0,0,(color>>24)&255),text);
    d->AddText(ImGui::GetFont(),size,p,color,text);
}

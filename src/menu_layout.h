#pragma once
#include <vector>

// Menu-only geometry. This never changes the host viewport or global font/style.
namespace menu_layout {
struct Size {float width=0,height=0,pad_x=0,pad_y=0;};
struct Point {float x=0,y=0;};
constexpr float maximum(float a,float b){return a>b?a:b;}
constexpr float minimum(float a,float b){return a<b?a:b;}
constexpr float clamp(float value,float lower,float upper){return value<lower?lower:value>upper?upper:value;}
constexpr Size size(float host_width,float host_height,float percent,bool preview){
    percent=percent>=50&&percent<=150?percent:percent<50?50:percent>150?150:100;
    const float pad_x=preview?0:minimum(20,maximum(0,(host_width-1)*.5f));
    const float pad_y=preview?0:minimum(24,maximum(0,(host_height-1)*.5f));
    const float available_width=maximum(1,host_width-pad_x*2),available_height=maximum(1,host_height-pad_y*2);
    return {minimum(available_width,maximum(600,1180*percent*.01f)),minimum(available_height,maximum(420,780*percent*.01f)),pad_x,pad_y};
}
constexpr Point origin(float x,float y,const Size& menu,float host_width,float host_height){
    return {clamp(x,menu.pad_x,maximum(menu.pad_x,host_width-menu.pad_x-menu.width)),clamp(y,menu.pad_y,maximum(menu.pad_y,host_height-menu.pad_y-menu.height))};
}
struct Cell {float x=0,y=0,width=0;};
struct Tabs {std::vector<Cell> cells;float height=0,row_height=32;};
inline constexpr float tab_height=32,tab_gap=4;
constexpr Tabs tabs(const std::vector<float>& minimum_widths,float available,float minimum_height=tab_height){
    Tabs result;result.row_height=maximum(tab_height,minimum_height);if(minimum_widths.empty())return result;available=maximum(1,available);
    std::size_t start=0;float y=0;
    while(start<minimum_widths.size()){
        std::size_t end=start;float used=0;
        while(end<minimum_widths.size()){
            const float width=minimum(available,maximum(1,minimum_widths[end]));
            if(end>start&&used+width>available)break;
            used+=width;++end;
        }
        const float extra=maximum(0,(available-used)/static_cast<float>(end-start));float x=0;
        for(auto i=start;i<end;++i){const float width=minimum(available,maximum(1,minimum_widths[i]))+extra;result.cells.push_back({x,y,width});x+=width;}
        start=end;y+=result.row_height+tab_gap;
    }
    result.height=y-tab_gap;return result;
}
}

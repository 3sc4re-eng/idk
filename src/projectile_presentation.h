#pragma once
#include "utils.h"

// Display-only values. No entity, reader, action or player-ESP configuration.
namespace projectile_presentation {
struct Style {
    bool marker=true,name=true,distance=true;
    float marker_size=6,font_size=14;
    int max_dist=120;
    Color color{.9f,.5f,1,1};
};
constexpr bool visible(const Style& style){return style.marker||style.name||style.distance;}
constexpr float bounded(float value,float fallback,float lower,float upper){
    return value>=lower&&value<=upper?value:value<lower?lower:value>upper?upper:fallback;
}
constexpr Style normalized(Style style){
    style.marker_size=bounded(style.marker_size,6,2,24);
    style.font_size=bounded(style.font_size,14,8,32);
    style.max_dist=style.max_dist<1?1:style.max_dist>400?400:style.max_dist;
    style.color={bounded(style.color.r,.9f,0,1),bounded(style.color.g,.5f,0,1),bounded(style.color.b,1,0,1),bounded(style.color.a,1,0,1)};
    return style;
}
// Comparisons also reject NaN and infinity without consuming another sample.
constexpr bool in_range(float distance,int maximum){return distance>=0&&distance<=maximum;}
}

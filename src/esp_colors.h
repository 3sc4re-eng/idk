#pragma once
#include "config.h"
inline Color esp_element_color(const char* name,bool visible){return cfg.C((std::string("esp.")+name+(visible?"_visible":"_hidden")).c_str());}
inline Color color_mix(Color a,Color b,float p){p=clampf(p,0,1);return {a.r+(b.r-a.r)*p,a.g+(b.g-a.g)*p,a.b+(b.b-a.b)*p,a.a+(b.a-a.a)*p};}
inline Color esp_health_color(float fraction,bool visible){
    if(!cfg.B("esp.health_gradient"))return esp_element_color("health",visible);
    float p=clampf(fraction,0,1);auto middle=cfg.C("esp.health_mid");
    return p<.5f?color_mix(cfg.C("esp.health_low"),middle,p*2):color_mix(middle,cfg.C("esp.health_high"),(p-.5f)*2);
}
// Legacy profile fade values remain loadable but no longer alter opacity.
inline float esp_distance_alpha(float){return 1;}

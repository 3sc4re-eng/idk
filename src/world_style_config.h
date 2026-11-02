#pragma once
#include "world_style_catalog.h"
#include "config.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace world_style_config {
struct Keys {
    std::string enabled,custom,initialized,label,icon,distance,color,icon_size,text_size,draw_distance;
};
inline const Keys& keys(std::size_t category) {
    static const auto values=[] {
        std::array<Keys,world_style_catalog::count> result{};
        for(std::size_t i=0;i<result.size();++i){
            const std::string p=std::string("world.style.")+world_style_catalog::categories[i].key+".";
            result[i]={p+"enabled",p+"custom",p+"initialized",p+"label",p+"icon",p+"distance",p+"color",p+"icon_size",p+"text_size",p+"draw_distance"};
        }
        return result;
    }();
    return values[category];
}
inline std::size_t category_from_path(std::string_view path) {
    if(!path.starts_with("world.style."))return world_style_catalog::missing;
    for(std::size_t i=0;i<world_style_catalog::count;++i){
        const std::string prefix=std::string("world.style.")+world_style_catalog::categories[i].key+".";
        if(path.starts_with(prefix))return i;
    }
    return world_style_catalog::missing;
}
struct Style {
    bool enabled=true,label=true,icon=true,distance=true,outline=true;
    Color color{.6f,.6f,.6f,1};float icon_size=8,text_size=14;int draw_distance=120;
};
using Styles=std::array<Style,world_style_catalog::count>;
inline float finite_size(float value,float fallback,float lo,float hi){return std::isfinite(value)?std::clamp(value,lo,hi):fallback;}
inline Style shared(Settings& settings) {
    Settings::VisualScope global(false);Style result;
    result.distance=settings.B("esp.distance");result.color=settings.C("esp.color_world");
    result.text_size=finite_size(settings.F(settings.B("esp.name_global_font")?"esp.font_size":"esp.name_font"),14,8,32);
    result.draw_distance=std::clamp(settings.I("esp.draw_distance"),1,400);result.outline=settings.B("esp.name_outline");
    return result;
}
inline Styles resolve(Settings& settings) {
    Settings::VisualScope global(false);Styles result;const Style inherited=shared(settings);
    for(std::size_t i=0;i<result.size();++i){
        const auto& p=keys(i);auto& value=result[i];value=inherited;value.enabled=settings.B(p.enabled.c_str());
        if(!settings.B(p.custom.c_str()))continue;
        value.label=settings.B(p.label.c_str());value.icon=settings.B(p.icon.c_str());value.distance=settings.B(p.distance.c_str());
        value.color=settings.C(p.color.c_str());value.icon_size=finite_size(settings.F(p.icon_size.c_str()),8,2,40);
        value.text_size=finite_size(settings.F(p.text_size.c_str()),inherited.text_size,8,32);
        value.draw_distance=std::clamp(settings.I(p.draw_distance.c_str()),1,400);
    }
    return result;
}
constexpr bool visible(const Style& style){return style.enabled&&(style.label||style.icon||style.distance);}
constexpr bool any_visible(const Styles& styles){for(const auto& style:styles)if(visible(style))return true;return false;}
// Preserve an imported active custom style before its checkbox can be switched
// off on the first displayed frame. Appearance is never reseeded here.
inline void prepare_edit(Settings& settings,std::size_t category) {
    Settings::WriteScope settings_edit(settings);
    const auto& p=keys(category);
    if(settings.B(p.custom.c_str())&&!settings.B(p.initialized.c_str())){
        settings.find(p.initialized.c_str())->b=true;settings.mark_dirty();
    }
}
inline void register_all(Settings& settings) {
    Settings::WriteScope settings_edit(settings);
    for(std::size_t i=0;i<world_style_catalog::count;++i){const auto& p=keys(i);
        settings.defb(p.enabled.c_str(),"Show this category",true);settings.defb(p.custom.c_str(),"Custom style",true);
        settings.defb(p.initialized.c_str(),"World style initialized",false); // Internal first-edit state; never exposed/searchable.
        settings.defb(p.label.c_str(),"Show name",true);settings.defb(p.icon.c_str(),"Show icon",true);settings.defb(p.distance.c_str(),"Show distance",true);
        settings.defc(p.color.c_str(),"Marker and text color",.6f,.6f,.6f,1);
        settings.deff(p.icon_size.c_str(),"Icon size (px)",8,2,40,.5f);settings.deff(p.text_size.c_str(),"Text size (px)",14,8,32,.5f);
        settings.defi(p.draw_distance.c_str(),"Draw distance (m)",120,1,400);
    }
}
// Copy only appearance. The category master is never changed by style reset.
inline void copy_shared(Settings& settings,std::size_t category) {
    Settings::WriteScope settings_edit(settings);
    const auto value=shared(settings);const auto& p=keys(category);
    settings.find(p.label.c_str())->b=value.label;settings.find(p.icon.c_str())->b=value.icon;
    settings.find(p.distance.c_str())->b=value.distance;settings.find(p.color.c_str())->c=value.color;
    settings.find(p.icon_size.c_str())->f=value.icon_size;settings.find(p.text_size.c_str())->f=value.text_size;
    settings.find(p.draw_distance.c_str())->i=value.draw_distance;settings.find(p.initialized.c_str())->b=true;settings.mark_dirty();
}
}

#include "world_units.h"
#include "world_labels.h"
#include "world_style_config.h"
#include "esp.h"
#include "esp_render_snapshot.h"
#include "projectile_presentation.h"
#include "entity_health.h"
#include "target_geometry.h"
#include "dma.h"
#include "native_target.h" // ACE_NATIVE_JOINT_INTEGRATION_V1
#include "esp_layout.h"
#include "esp_elements.h"
#include "esp_colors.h"
#include "esp_visuals.h"
#include "esp_text.h"
#include "ability_tracking.h"
#include "ability_icons.h"
#include "config.h"
#include "input.h"
#include "imgui.h"
namespace esp{
static ImU32 col_of(Color c){ return IM_COL32((int)(c.r*255),(int)(c.g*255),(int)(c.b*255),(int)(clampf(c.a,0,1)*255)); }
static void draw_entity(ImDrawList* dl,const Entity& e,const esp_render::Local& me,u64 W,u64 H,const esp_render::Snapshot& frame){
    if(e.cls==EClass::Player&&e.alive_valid&&!e.alive)return;
    Settings::VisualScope side(e.cls==EClass::Player&&e.team==me.team);
    if(!(e.cls==EClass::Player?cfg.B("esp.enabled"):cfg.B("world.enabled")))return;
    float d=world_units::metres(me.pos.dist(e.pos));
    if(d>(float)cfg.I("esp.draw_distance")) return;
    Vec2 sp=esp_render::project(frame,e.pos,W,H);
    if(sp.x<-1000.f) return;
    Color base=e.cls==EClass::Player?(e.visible?cfg.C("esp.color_visible"):cfg.C("esp.color_hidden"))
              :e.cls==EClass::NPC?cfg.C("esp.color_npcs")
              :e.cls==EClass::World?cfg.C("esp.color_world"):cfg.C("esp.color_proj");
    float fade=esp_distance_alpha(d);if(fade<=0)return;
    auto element=[&](const char* feature){return esp_rgba(e.cls==EClass::Player?esp_element_color(feature,e.visible):base,fade);};
    ImU32 col=element("box");
    std::array<Vec3,8> corners;float x1=1e9f,y1=1e9f,x2=-1e9f,y2=-1e9f;
    if(target_geometry::corners(e,corners)){
        for(auto point:corners){Vec2 p=esp_render::project(frame,point,W,H);if(p.x<-1000)return;x1=std::min(x1,p.x);x2=std::max(x2,p.x);y1=std::min(y1,p.y);y2=std::max(y2,p.y);}
    }else{Vec2 top=esp_render::project(frame,e.pos+Vec3{0,0,80*std::clamp(e.scale,.1f,10.f)},W,H);if(top.x<-1000)return;float height=std::fabs(sp.y-top.y);x1=sp.x-height*.21f;x2=sp.x+height*.21f;y1=std::min(top.y,sp.y);y2=std::max(top.y,sp.y);}
    float bh=y2-y1;if(bh<2||x2-x1<1)return;
    if(e.cls==EClass::Player)esp_box(dl,x1,y1,x2,y2,e.visible,fade);
    else if(cfg.B("esp.box"))dl->AddRect({x1,y1},{x2,y2},esp_rgba(base,fade),0,0,cfg.F("esp.box_width"));
    if(entity_health::pair(e))esp_health(dl,x1,y1,x2,y2,e.hp,e.max_hp,e.visible,fade);
    if(cfg.B("esp.head_circle")){
        ImU32 col=element("head_circle");
        Vec3 region;if(frame.native_binding_epoch==snap.native_binding_epoch.load()&&native_target::point(e,0,region,frame.native_binding_epoch,mem.attachment_epoch(),now_ms())){Vec2 hpp=esp_render::project(frame,region,W,H);
            if(hpp.x>-1000.f)dl->AddCircle({hpp.x,hpp.y},clampf(bh*.055f,2.f,18.f),col,16,cfg.F("esp.head_circle_width"));}
    }
    if(cfg.B("esp.tracers")){
        dl->AddLine({(float)W/2.f,(float)H},{sp.x,y2},element("tracers"),cfg.F("esp.tracers_width"));
    }
    using namespace esp_elements;char soul_text[48]{},distance_text[32]{};snprintf(soul_text,sizeof(soul_text),"%llu souls",(unsigned long long)e.souls);snprintf(distance_text,sizeof(distance_text),"%.0fm",d);
    const char* actor_name=!e.display_name.empty()?e.display_name.c_str():(e.settings_subject.kind==profile_subject::Kind::Hero&&!e.settings_subject.name.empty()?e.settings_subject.name.c_str():"Player");
    const char* text[]={actor_name,soul_text,distance_text,"ULT READY"};std::array<bool,Count> enabled{};std::array<ImVec2,Count> sizes{};std::array<ability_tracking::View,4> states{};
    bool player=e.cls==EClass::Player;const float game_time=player?frame.game_time:0;
    // A newer reader publish cannot make this copied entity artificially fresh.
    const bool fresh=player&&frame.ability_fresh(now_ms());
    for(int i=0;i<4;i++)states[i]=ability_tracking::view(e,i,game_time,fresh);auto row=ability_row(states,cfg.F("esp.ability_size"),cfg.B("esp.ability_seconds"));
    enabled[Name]=player&&cfg.B("esp.name");enabled[Souls]=player&&e.souls>0&&cfg.B("esp.souls");enabled[Distance]=cfg.B("esp.distance");enabled[Ultimate]=player&&cfg.B("esp.ultimate")&&states[3].state==ability_tracking::State::Ready;enabled[Abilities]=player&&cfg.B("esp.abilities");
    for(int i=0;i<Count;i++)sizes[i]=i==Abilities?row.size:ImGui::GetFont()->CalcTextSizeA(esp_text_size(features[i]),1e10f,0,text[i]);
    auto blocks=place(enabled,sizes,x1,y1,x2,y2);
    for(int i=0;i<Count;i++)if(blocks[i].enabled){if(i==Abilities)draw_abilities(dl,blocks[i].position,row,fade);else esp_text(dl,features[i],blocks[i].position,i==Ultimate?esp_rgba(cfg.C("esp.ultimate_color"),fade):element(features[i]),text[i]);}

}
static void world_icon(ImDrawList* draw,ImVec2 p,world_labels::Icon icon,ImU32 color,float size){
    using Icon=world_labels::Icon;const float scale=size/8.f;
    auto q=[&](float x,float y){return ImVec2{p.x+x*scale,p.y+y*scale};};
    if(icon==Icon::Health){draw->AddRectFilled(q(-1.4f,-4),q(1.4f,4),color);draw->AddRectFilled(q(-4,-1.4f),q(4,1.4f),color);}
    else if(icon==Icon::Crate||icon==Icon::Drawing){draw->AddRect(q(-4,-4),q(4,4),color,scale,0,1.3f*scale);if(icon==Icon::Crate){draw->AddLine(q(-4,-4),q(4,4),color,scale);draw->AddLine(q(-4,4),q(4,-4),color,scale);}else draw->AddLine(q(-2,2),q(2,-2),color,scale);}
    else if(icon==Icon::Rejuvenator||icon==Icon::Idol){draw->AddQuad(q(0,-5),q(4,0),q(0,5),q(-4,0),color,1.4f*scale);if(icon==Icon::Rejuvenator)draw->AddCircleFilled(p,1.2f*scale,color,8);}
    else if(icon==Icon::Breakable){draw->AddLine(q(-4,4),q(4,-4),color,1.5f*scale);draw->AddLine(q(-1,-4),q(4,-1),color,1.5f*scale);}
    else if(icon==Icon::Candy){draw->AddCircle(p,2.5f*scale,color,10,1.4f*scale);draw->AddLine(q(-3,0),q(-5,-2),color,scale);draw->AddLine(q(3,0),q(5,2),color,scale);}
    else{draw->AddCircle(p,4*scale,color,12,1.4f*scale);if(icon==Icon::Currency)draw->AddCircleFilled(p,1.3f*scale,color,8);}
}
static void draw_world(ImDrawList* draw,const Entity& entity,const esp_render::Local& local,u64 width,u64 height,const esp_render::Snapshot& frame,const world_style_config::Styles& styles){
    if(!cfg.B("world.enabled")||!cfg.B("cls.world")||entity.cls!=EClass::World||!entity.base||entity.world_obstacle)return;
    const auto* definition=world_labels::find(entity.type_name);if(!definition)return;
    const auto category=world_style_catalog::index(*definition);if(category==world_style_catalog::missing)return;
    const auto& style=styles[category];if(!world_style_config::visible(style))return;
    const float distance=world_units::metres(local.pos.dist(entity.pos));if(!std::isfinite(distance)||distance>style.draw_distance)return;
    const float fade=esp_distance_alpha(distance);if(fade<=0)return;
    const auto point=esp_render::project(frame,entity.pos,width,height);const float margin=style.icon?std::max(6.f,style.icon_size*5.f/8.f+1.f):2.f;
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||point.x<margin||point.y<margin||point.x>static_cast<float>(width)-margin||point.y>static_cast<float>(height)-margin)return;
    const ImU32 color=esp_rgba(style.color,fade);
    if(style.icon)world_icon(draw,{point.x,point.y},definition->icon,color,style.icon_size);
    if(!style.label&&!style.distance)return;
    char text[128]{};
    if(style.label&&style.distance)snprintf(text,sizeof(text),"%s  %.0fm",definition->label,distance);
    else if(style.label)snprintf(text,sizeof(text),"%s",definition->label);
    else snprintf(text,sizeof(text),"%.0fm",distance);
    const auto size=ImGui::GetFont()->CalcTextSizeA(style.text_size,1e10f,0,text);
    const ImVec2 position{point.x-size.x*.5f,point.y+(style.icon?style.icon_size*5.f/8.f+2.f:0.f)};
    if(style.outline)for(auto offset:{ImVec2{-1,0},ImVec2{1,0},ImVec2{0,-1},ImVec2{0,1}})draw->AddText(ImGui::GetFont(),style.text_size,{position.x+offset.x,position.y+offset.y},IM_COL32(0,0,0,(color>>24)&255),text);
    draw->AddText(ImGui::GetFont(),style.text_size,position,color,text);
}
// Independent screen-space presentation of the already captured projectile point.
static void draw_projectile(ImDrawList* draw,const Vec3& position,const esp_render::Snapshot& frame,u64 width,u64 height,const projectile_presentation::Style& style){
    if(!projectile_presentation::visible(style))return;
    const float distance=world_units::metres(frame.local.pos.dist(position));
    if(!projectile_presentation::in_range(distance,style.max_dist))return;
    const auto point=esp_render::project(frame,position,width,height);
    const float radius=style.marker_size*.5f,margin=style.marker?radius+2.f:1.f;
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||point.x<margin||point.y<margin||point.x>static_cast<float>(width)-margin||point.y>static_cast<float>(height)-margin)return;
    const ImU32 color=IM_COL32(static_cast<int>(style.color.r*255),static_cast<int>(style.color.g*255),static_cast<int>(style.color.b*255),static_cast<int>(style.color.a*255));
    const ImU32 outline=IM_COL32(0,0,0,(color>>24)&255);
    if(style.marker){
        const ImVec2 top{point.x,point.y-radius},right{point.x+radius,point.y},bottom{point.x,point.y+radius},left{point.x-radius,point.y};
        draw->AddQuad(top,right,bottom,left,outline,3.f);draw->AddQuad(top,right,bottom,left,color,1.5f);
    }
    if(!style.name&&!style.distance)return;
    char text[64]{};
    if(style.name&&style.distance)snprintf(text,sizeof(text),"Projectile  %.0fm",distance);
    else if(style.name)snprintf(text,sizeof(text),"Projectile");
    else snprintf(text,sizeof(text),"%.0fm",distance);
    auto* font=ImGui::GetFont();const auto size=font->CalcTextSizeA(style.font_size,1e10f,0,text);
    const ImVec2 label{clampf(point.x-size.x*.5f,1.f,std::max(1.f,static_cast<float>(width)-size.x-1.f)),clampf(point.y+(style.marker?radius+3.f:0.f),1.f,std::max(1.f,static_cast<float>(height)-size.y-1.f))};
    for(auto offset:{ImVec2{-1,0},ImVec2{1,0},ImVec2{0,-1},ImVec2{0,1}})draw->AddText(font,style.font_size,{label.x+offset.x,label.y+offset.y},outline,text);
    draw->AddText(font,style.font_size,label,color,text);
}

static void draw_minion(ImDrawList* dl,const Entity& e,const esp_render::Local& me,u64 W,u64 H,const esp_render::Snapshot& frame){
    if(!cfg.B("minions.enabled")||(e.alive_valid&&!e.alive))return;bool friendly=e.team==me.team;bool neutral=e.type_name.find("Neutral")!=std::string::npos;bool boss=e.type_name.find("Boss")!=std::string::npos||e.type_name.find("MidBoss")!=std::string::npos;
    if(neutral&&!cfg.B("minions.neutral"))return;if(boss&&!cfg.B("minions.bosses"))return;if(!neutral&&!boss&&!cfg.B("minions.troopers"))return;
    if(friendly?!cfg.B("minions.friendly"):!cfg.B("minions.enemy"))return;float distance=world_units::metres(me.pos.dist(e.pos));if(distance>cfg.I("minions.distance_limit"))return;
    Vec3 lower=e.pos,upper=e.pos+Vec3{0,0,64};if(e.bounds_valid){lower=e.pos+e.bounds_min*e.scale;upper=e.pos+e.bounds_max*e.scale;}
    auto bottom=esp_render::project(frame,lower,W,H),top=esp_render::project(frame,upper,W,H);if(bottom.x<-1000||top.x<-1000)return;float h=std::fabs(bottom.y-top.y);if(h<2)return;float x=(bottom.x+top.x)/2,w=h*.42f,y1=std::min(top.y,bottom.y),y2=std::max(top.y,bottom.y);
    ImU32 color=esp_rgba(cfg.C(friendly?"minions.color_friendly":"minions.color_enemy"));if(cfg.B("minions.box"))dl->AddRect({x-w/2,y1},{x+w/2,y2},color);
    if(cfg.B("minions.health")&&entity_health::pair(e)){float p=clampf(e.hp/e.max_hp,0,1);dl->AddRectFilled({x-w/2-5,y1},{x-w/2-2,y2},IM_COL32(8,8,8,200));dl->AddRectFilled({x-w/2-5,y2-h*p},{x-w/2-2,y2},color);}
    if(cfg.B("minions.name")){const char* name=boss?"Boss":neutral?"Neutral camp":"Trooper";auto size=ImGui::CalcTextSize(name);dl->AddText({x-size.x/2,y1-size.y-3},color,name);}
    if(cfg.B("minions.distance")){char text[24];snprintf(text,sizeof(text),"%.0fm",distance);auto size=ImGui::CalcTextSize(text);dl->AddText({x-size.x/2,y2+3},color,text);}
}
void draw(u64 sw,u64 sh){
    if(!cfg.B("visuals.enabled"))return;
    esp_render::Demand demand;const bool players=cfg.B("cls.players");
    auto player_elements=[](){return cfg.B("esp.box")||cfg.B("esp.health")||cfg.B("esp.tracers")||cfg.B("esp.head_circle")||cfg.B("esp.name")||cfg.B("esp.souls")||cfg.B("esp.distance")||cfg.B("esp.ultimate")||cfg.B("esp.abilities");};
    {Settings::VisualScope side(false);demand.enemies=players&&cfg.B("esp.enabled")&&player_elements();}
    {Settings::VisualScope side(true);demand.teammates=players&&cfg.B("esp.enabled")&&player_elements();}
    // Non-player settings are global even if a caller had a teammate scope.
    Settings::VisualScope global(false);
    demand.npcs=cfg.B("minions.enabled")&&(cfg.B("minions.enemy")||cfg.B("minions.friendly"))&&(cfg.B("minions.troopers")||cfg.B("minions.neutral")||cfg.B("minions.bosses"))&&(cfg.B("minions.box")||cfg.B("minions.health")||cfg.B("minions.name")||cfg.B("minions.distance"));
    const bool world_master=cfg.B("world.enabled")&&cfg.B("cls.world");
    const auto world_styles=world_master?world_style_config::resolve(cfg):world_style_config::Styles{};
    demand.world=world_master&&world_style_config::any_visible(world_styles);
    const auto projectile_style=projectile_presentation::normalized(projectile_presentation::Style{cfg.B("projectiles.marker"),cfg.B("projectiles.name"),cfg.B("projectiles.distance"),cfg.F("projectiles.marker_size"),cfg.F("projectiles.font_size"),cfg.I("projectiles.max_dist"),cfg.C("esp.color_proj")});
    demand.projectiles=cfg.B("world.enabled")&&cfg.B("cls.projectiles")&&projectile_presentation::visible(projectile_style);
    if(!demand.any())return;
    // Capacity reuse only: every successful draw recaptures data under ONE lock.
    // Failed capture returns without consuming old buffer contents.
    static thread_local esp_render::Snapshot frame;
    if(!esp_render::capture(snap,snap_mtx,demand,[]{return now_ms();},frame))return;
    const auto& me=frame.local;ImDrawList* dl=ImGui::GetBackgroundDrawList();
    for(const auto& e:frame.enemies)draw_entity(dl,e,me,sw,sh,frame);
    for(const auto& e:frame.npcs)draw_minion(dl,e,me,sw,sh,frame);
    for(const auto& e:frame.world)draw_world(dl,e,me,sw,sh,frame,world_styles);
    for(const auto& e:frame.projectiles)draw_projectile(dl,e.pos,frame,sw,sh,projectile_style);
    for(const auto& e:frame.teammates)draw_entity(dl,e,me,sw,sh,frame);
}

}

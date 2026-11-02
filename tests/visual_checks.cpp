#include "esp_visuals.h"
#include "esp_elements.h"
#include "hero_catalog.h"
#include <cassert>
#include <cstdio>
int main(){
    cfg.register_all();auto low=esp_health_color(0,true),mid=esp_health_color(.5f,true),high=esp_health_color(1,true);
    assert(low.r>low.g&&mid.r>.9f&&mid.g>.9f&&high.g>high.r);
    auto left=esp_health_color(.49999f,true),right=esp_health_color(.50001f,true);assert(std::fabs(left.r-right.r)<.001f&&std::fabs(left.g-right.g)<.001f);
    cfg.find("esp.fade_distance")->b=true;cfg.find("esp.fade_start")->f=20;cfg.find("esp.fade_end")->f=100;
    assert(esp_distance_alpha(10)==1&&esp_distance_alpha(60)==1&&esp_distance_alpha(1000)==1);
    cfg.find("esp.ultimate_position")->i=2;cfg.find("esp.abilities_position")->i=3;
    {Settings::VisualScope team(true);assert(!cfg.B("esp.enabled"));cfg.find("esp.health_style")->i=2;cfg.find("esp.box_visible")->c={.1f,.2f,.3f,1};}
    assert(cfg.I("esp.health_style")==0);cfg.find("esp.box_visible")->c={.7f,.8f,.9f,1};
    {Settings::VisualScope team(true);assert(cfg.I("esp.health_style")==2&&cfg.C("esp.box_visible").r==.1f);}
    assert(cfg.save("visual_test.json"));cfg=Settings{};cfg.register_all();assert(cfg.load("visual_test.json"));
    {Settings::VisualScope team(true);assert(cfg.I("esp.health_style")==2&&cfg.C("esp.box_visible").r==.1f);}
    assert(cfg.I("esp.ultimate_position")==2&&cfg.I("esp.abilities_position")==3);
    auto* context=ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={800,600};io.DeltaTime=.016f;unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);ImGui::NewFrame();
    using namespace esp_elements;
    std::array<bool,Count> present{};present.fill(true);std::array<ImVec2,Count> sizes={ImVec2{20,12},ImVec2{30,13},ImVec2{28,12},ImVec2{70,15},ImVec2{140,42}};
    auto disjoint=[](const Blocks& blocks){for(int i=0;i<Count;i++)for(int j=i+1;j<Count;j++)if(blocks[i].enabled&&blocks[j].enabled){auto a=blocks[i],b=blocks[j];bool hit=a.position.x<b.position.x+b.size.x&&b.position.x<a.position.x+a.size.x&&a.position.y<b.position.y+b.size.y&&b.position.y<a.position.y+a.size.y;assert(!hit);}};
    for(auto key:position_keys)cfg.find(key)->i=0;auto stacked=place(present,sizes,100,100,160,200);disjoint(stacked);
    cfg.find(position_keys[Souls])->i=2;cfg.find(position_keys[Distance])->i=3;cfg.find(position_keys[Ultimate])->i=2;cfg.find(position_keys[Abilities])->i=3;
    auto fitted=place(present,sizes,-60,-100,250,480);fit_preview(fitted,{0,0},{190,400});disjoint(fitted);
    for(auto& b:fitted){assert(b.position.x>=-.01f&&b.position.y>=-.01f&&b.position.x+b.size.x<=190.01f&&b.position.y+b.size.y<=400.01f);}
    std::array<ability_tracking::View,4> states{};for(auto& state:states)state.state=ability_tracking::State::Ready;states[1].state=ability_tracking::State::Cooldown;states[1].seconds=123;
    auto with_timer=ability_row(states,22,true),without_timer=ability_row(states,22,false);assert(with_timer.captions[1]=="123s"&&without_timer.captions[1].empty());assert(with_timer.size.y>without_timer.size.y&&with_timer.slot_width>=ImGui::GetFont()->CalcTextSizeA(with_timer.caption_font,1e10f,0,"123s").x+4);
    auto* draw=ImGui::GetBackgroundDrawList();draw_abilities(draw,{20,20},with_timer);
auto health=[&](int style){cfg.find("esp.health_style")->i=style;int before=draw->VtxBuffer.Size;esp_health(draw,100,100,180,400,75,100,true);return draw->VtxBuffer.Size-before;};
    int bar=health(0),number=health(1),both=health(2);assert(bar>100&&number>0&&both==bar+number);
    cfg.find("esp.health")->b=false;assert(health(0)==0&&health(1)==0&&health(2)==0);
    cfg.find("esp.box")->b=true;auto box=[&](int style){cfg.find("esp.box_style")->i=style;int before=draw->VtxBuffer.Size;esp_box(draw,100,100,180,400,true,.5f);return draw->VtxBuffer.Size-before;};
    int outline=box(0),corner=box(1),filled=box(2);assert(outline>0&&corner>0&&filled>outline);
    ImGui::EndFrame();ImGui::DestroyContext(context);
    assert(std::string(current_hero_name(11))=="Dynamo"&&std::string(current_hero_name(13))=="Haze"&&std::string(current_hero_name(3))=="Vindicta");
    std::remove("visual_test.json");std::puts("PASS: continuous health gradient, actual bar/number/both draw buffers, filled/corner/outline boxes, legacy fade ignored, five-element layout/cooldown spacing, persisted ultimate/icon anchors, independent team colors/settings and JSON persistence, current game hero IDs");
}

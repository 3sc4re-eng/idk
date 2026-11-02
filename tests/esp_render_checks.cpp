#include "esp_render_snapshot.h"
#include <cassert>
#include <limits>
namespace {
Vec2 legacy(const float* m,Vec3 point,u64 width,u64 height){
    float w=point.x*m[12]+point.y*m[13]+point.z*m[14]+m[15];if(!std::isfinite(w)||w<.01f)return {-99999,-99999};
    float x=point.x*m[0]+point.y*m[1]+point.z*m[2]+m[3],y=point.x*m[4]+point.y*m[5]+point.z*m[6]+m[7];
    if(!std::isfinite(x)||!std::isfinite(y))return {-99999,-99999};return {(float)width*.5f*(1+x/w),(float)height*.5f*(1-y/w)};
}
}
void esp_render_boundary_checks(){
    Snapshots source;std::mutex mutex;esp_render::Snapshot frame;esp_render::Demand wanted;
    source.backend_managed=true;source.frame_valid=true;source.last_read_ms=1000;source.matrix_sample_ms=1005;source.game_time=9;
    source.local.base=1;source.local.team=1;source.local.pos={1,2,3};source.view[0]=source.view[5]=source.view[10]=source.view[15]=1;
    Entity enemy;enemy.base=2;enemy.team=2;enemy.abilities.resize(2);enemy.abilities[0].type="owned copy";Entity ally=enemy;ally.base=3;ally.team=1;
    Entity world=enemy;world.cls=EClass::World;Entity collision_only=world;collision_only.world_obstacle=true;
    source.enemies={enemy,ally};source.friendlies={ally};source.npcs={enemy};source.world={world,collision_only,enemy};source.projectiles={enemy};
    assert(!esp_render::capture(source,mutex,wanted,[]{return u64(1010);},frame)&&!frame.ready);
    wanted.enemies=true;assert(esp_render::capture(source,mutex,wanted,[]{return u64(1010);},frame));
    assert(frame.enemies.size()==1&&frame.teammates.empty()&&frame.npcs.empty()&&frame.world.empty()&&frame.projectiles.empty());
    assert(frame.enemies[0].base==2&&frame.game_time==9&&frame.local.team==1&&frame.ability_fresh(1250)&&!frame.ability_fresh(1251));
    {std::lock_guard<std::mutex> lock(mutex);source.game_time=20;source.view[0]=8;source.enemies[0].abilities[0].type="changed source";source.last_read_ms=1200;}
    assert(frame.game_time==9&&frame.view[0]==1&&frame.enemies[0].abilities[0].type=="owned copy"&&!frame.ability_fresh(1251));
    wanted.enemies=false;wanted.teammates=true;assert(esp_render::capture(source,mutex,wanted,[]{return u64(1210);},frame));assert(frame.enemies.size()==1&&frame.enemies[0].team==1&&frame.teammates.size()==1); // Actual team, not vector name.
    wanted={true,true,true,true,true};assert(esp_render::capture(source,mutex,wanted,[]{return u64(1210);},frame));assert(frame.enemies.size()==2&&frame.teammates.size()==1&&frame.npcs.size()==1&&frame.world.size()==1&&frame.projectiles.size()==1);
    source.frame_valid=false;assert(!esp_render::capture(source,mutex,wanted,[]{return u64(1211);},frame)&&!frame.ready);source.frame_valid=true;
    source.matrix_sample_ms=900;assert(!esp_render::capture(source,mutex,wanted,[]{return u64(1210);},frame));source.matrix_sample_ms=1211;assert(!esp_render::capture(source,mutex,wanted,[]{return u64(1210);},frame));
    source.matrix_sample_ms=1200;source.last_read_ms=900;assert(!esp_render::capture(source,mutex,wanted,[]{return u64(1210);},frame));source.last_read_ms=1211;assert(!esp_render::capture(source,mutex,wanted,[]{return u64(1210);},frame));
    source.last_read_ms=1200;source.local.base=0;assert(!esp_render::capture(source,mutex,wanted,[]{return u64(1210);},frame));
    esp_render::Snapshot projection;projection.view[0]=projection.view[5]=projection.view[10]=projection.view[15]=1;
    for(auto point:{Vec3{},Vec3{1,-1,2},Vec3{-100,20,9},Vec3{std::numeric_limits<float>::infinity(),0,0}}){auto old=legacy(projection.view.data(),point,1920,1080),next=esp_render::project(projection,point,1920,1080);assert(old.x==next.x&&old.y==next.y);}
    for(float w:{.009f,.01f,.011f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){projection.view[15]=w;auto old=legacy(projection.view.data(),{},800,600),next=esp_render::project(projection,{},800,600);assert(old.x==next.x&&old.y==next.y);}
}

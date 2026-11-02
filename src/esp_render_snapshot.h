#pragma once
#include "entities.h"
#include <algorithm>

namespace esp_render {
struct Demand {
    bool enemies=false,teammates=false,npcs=false,world=false,projectiles=false;
    bool any()const{return enemies||teammates||npcs||world||projectiles;}
};
struct Local {u64 base=0;u32 team=0;Vec3 pos;};
struct Snapshot {
    Local local;std::array<float,16> view{};float game_time=0;bool frame_valid=false,ready=false;
    u64 entity_sample=0,matrix_sample=0,native_binding_epoch=0; // ACE_NATIVE_JOINT_INTEGRATION_V1
    std::vector<Entity> enemies,teammates,npcs,world,projectiles;
    bool ability_fresh(u64 now)const{return ready&&frame_valid&&now>=entity_sample&&now-entity_sample<=250;}
};
// Exact existing world_to_screen arithmetic, order, clip boundary and sentinel.
// No global state, lock, read callback or matrix resampling occurs here.
inline Vec2 project(const Snapshot& frame,Vec3 point,u64 width,u64 height){
    const auto& m=frame.view;
    float w=point.x*m[12]+point.y*m[13]+point.z*m[14]+m[15];if(!std::isfinite(w)||w<.01f)return {-99999,-99999};
    float x=point.x*m[0]+point.y*m[1]+point.z*m[2]+m[3],y=point.x*m[4]+point.y*m[5]+point.z*m[6]+m[7];
    if(!std::isfinite(x)||!std::isfinite(y))return {-99999,-99999};return {(float)width*.5f*(1+x/w),(float)height*.5f*(1-y/w)};
}
inline void copy_players(const std::vector<Entity>& source,std::vector<Entity>& destination,u32 local_team,const Demand& demand){
    if(demand.enemies&&demand.teammates){destination=source;return;}
    // Use actual team rather than assuming a vector name establishes side.
    auto wanted=[&](const Entity& e){return e.team==local_team?demand.teammates:demand.enemies;};
    const auto count=std::count_if(source.begin(),source.end(),wanted);destination.resize(std::size_t(count));
    std::size_t at=0;for(const auto& e:source)if(wanted(e))destination[at++]=e;
}
inline void copy_world(const std::vector<Entity>& source,std::vector<Entity>& destination){
    // These are exactly draw_world's existing early rejections. Collision-only
    // records remain in the shared source for their actual consumers.
    auto wanted=[](const Entity& e){return e.cls==EClass::World&&e.base&&!e.world_obstacle;};
    const auto count=std::count_if(source.begin(),source.end(),wanted);destination.resize(std::size_t(count));
    std::size_t at=0;for(const auto& e:source)if(wanted(e))destination[at++]=e;
}
template<class Clock> inline bool capture(const Snapshots& source,std::mutex& mutex,const Demand& demand,Clock&& clock,Snapshot& out){
    out.ready=false;
    // Disabled vectors are cleared outside the snapshot lock. Active vector
    // assignment can reuse both outer capacity and existing Entity allocations.
    if(!demand.enemies&&!demand.teammates){out.enemies.clear();out.teammates.clear();}
    if(!demand.npcs)out.npcs.clear();if(!demand.world)out.world.clear();if(!demand.projectiles)out.projectiles.clear();
    if(!demand.any())return false;
    std::lock_guard<std::mutex> lock(mutex);
    // A publisher may finish while capture waits. Read host time only after
    // acquiring the same lock, beside the publication's timestamps.
    const u64 now=clock();
    const auto matrix=source.matrix_sample_ms.load(),entities=source.last_read_ms.load();
    if(source.backend_managed&&(!source.frame_valid||!matrix||!entities||now<matrix||now<entities||now-matrix>250||now-entities>250))return false;
    if(!source.local.base)return false;
    out.local={source.local.base,source.local.team,source.local.pos};
    std::copy(std::begin(source.view),std::end(source.view),out.view.begin());
    out.game_time=source.game_time;out.frame_valid=source.frame_valid.load();out.entity_sample=entities;out.matrix_sample=matrix;out.native_binding_epoch=source.native_binding_epoch.load();
    if(demand.enemies||demand.teammates){copy_players(source.enemies,out.enemies,out.local.team,demand);copy_players(source.friendlies,out.teammates,out.local.team,demand);}
    if(demand.npcs)out.npcs=source.npcs;if(demand.world)copy_world(source.world,out.world);if(demand.projectiles)out.projectiles=source.projectiles;
    out.ready=true;return true;
}
} // namespace esp_render

#include "defense.h"
#include "consumer_demand.h"
#include "config.h"
#include "input.h"
#include "world_units.h"
#include "km_protocol.h"
#include "collision.h"
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <cstring>
namespace defense{
static std::mutex status_mutex;static std::string message="Waiting for live data";
static std::unordered_map<u64,u64> handled;static u64 reserved_until=0;static unsigned last_hero=0;
static void note(std::string text){std::lock_guard<std::mutex> lock(status_mutex);message=std::move(text);}
bool busy(){return now_ms()<reserved_until;}
std::string status(){std::lock_guard<std::mutex> lock(status_mutex);return message;}
void reset(){handled.clear();reserved_until=0;last_hero=0;}
std::string prefix(unsigned hero){return "defense."+std::to_string(hero)+".";}
std::string selection(const std::string& prefix,u32 token){return prefix+"watch."+std::to_string(token);}
static bool native_area_impact(const ability_data::Entry& e){return std::strcmp(e.key,"citadel_ability_bull_leap")==0;}
static bool lockon_impact(const ability_data::Entry& e){return std::strcmp(e.key,"citadel_ability_lash_ultimate")==0;}
static bool charge_impact(const ability_data::Entry& e){return std::strcmp(e.key,"citadel_ability_bull_charge")==0;}
bool observable(const ability_data::Entry& entry){
    return entry.enemy&&!entry.passive&&(entry.projectile||native_area_impact(entry)||lockon_impact(entry)||charge_impact(entry)||(entry.aoe&&entry.self_centered));
}
bool counterspell_observable(const ability_data::Entry& e){
    if(!observable(e))return false;
    // These sustained tick/aura effects are deliberately not Counterspell
    // targets, even if a legacy config still has their watch flag enabled.
    if(std::strcmp(e.key,"citadel_ability_bull_heal")==0||std::strcmp(e.key,"ability_nurse_fear")==0||std::strcmp(e.key,"ability_frank_painaura")==0)return false;
    // An AoE radius or generic channel flag does not provide a discrete impact
    // time. Only moving projectile paths and the explicit native readers qualify.
    return e.projectile||native_area_impact(e)||lockon_impact(e)||charge_impact(e);
}
const char* observation(const ability_data::Entry& e){return !e.enemy?"Non-hostile":(e.passive?"Passive":(e.projectile?"Projectile":(native_area_impact(e)?"Native area impact":(e.aoe?"Area / cast target":(observable(e)?"Charge":"Unavailable")))));}
const char* counterspell_observation(const ability_data::Entry& e){
    return e.projectile?"Reacts when the enemy projectile's current path will hit you, with range and line-of-sight checks.":(native_area_impact(e)?"Uses the game's impact position and future impact time, with range and line-of-sight checks.":(lockon_impact(e)?"Requires your handle in the cast's lock-on targets and a future scheduled cast-state transition.":"Requires an approaching charge path that intersects your position, with line-of-sight checks."));
}
static const Ability* ability(const Entity& e,u32 token){for(auto& a:e.abilities)if(a.token==token&&a.valid)return &a;return nullptr;}
static const ability_data::Entry* named(const char* key){for(auto& d:ability_data::entries)if(std::strcmp(d.key,key)==0)return &d;return nullptr;}
static bool contains(const Threat& t,u32 handle){return std::find(t.victims.begin(),t.victims.end(),handle)!=t.victims.end();}
static float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static float intercept(const Entity& projectile,const Entity& victim,float radius){
    if(!victim.bounds_valid)return -1;
    Vec3 relative=projectile.pos-victim.pos,velocity=projectile.vel-victim.vel;
    float position[3]={relative.x,relative.y,relative.z},speed[3]={velocity.x,velocity.y,velocity.z};
    float low[3]={victim.bounds_min.x-radius,victim.bounds_min.y-radius,victim.bounds_min.z-radius};
    float high[3]={victim.bounds_max.x+radius,victim.bounds_max.y+radius,victim.bounds_max.z+radius};
    float enter=0,exit=2;
    for(int axis=0;axis<3;axis++){
        if(std::fabs(speed[axis])<.00001f){if(position[axis]<low[axis]||position[axis]>high[axis])return -1;continue;}
        float first=(low[axis]-position[axis])/speed[axis],last=(high[axis]-position[axis])/speed[axis];if(first>last)std::swap(first,last);
        enter=std::max(enter,first);exit=std::min(exit,last);if(enter>exit)return -1;
    }
    return enter>=0&&enter<=2?enter:-1;
}
static bool clear_path(const World& world,Vec3 from,Vec3 to,unsigned channel){if(!world.require_los)return true;auto ray=collision::trace(from,to,channel);return ray.known&&ray.clear;}
std::vector<Threat> detect(const World& world){
    std::vector<Threat> result;std::vector<const Entity*> victims{&world.local};for(auto& e:world.friends)if(e.alive)victims.push_back(&e);
    for(auto& projectile:world.projectiles){
        auto* data=ability_data::find(projectile.ability_token);if(!data||!data->projectile||!observable(*data))continue;
        auto owner=std::find_if(world.enemies.begin(),world.enemies.end(),[&](auto& enemy){return enemy.base==projectile.owner;});if(owner==world.enemies.end())continue;
        Threat threat;threat.id=projectile.handle;threat.source=owner->base;threat.token=data->token;threat.projectile=true;threat.aoe=data->aoe;threat.eta=100;
        float radius=std::max(0.f,projectile.collision_radius)+std::max(0.f,data->radius);
        for(auto* victim:victims){float time=intercept(projectile,*victim,radius);if(time>=0&&clear_path(world,projectile.pos,projectile.pos+projectile.vel*time,collision::Projectile)&&clear_path(world,projectile.pos+projectile.vel*time,victim->pos+victim->vel*time+(victim->bounds_min+victim->bounds_max)*.5f,collision::Ability)){threat.victims.push_back(victim->handle);threat.eta=std::min(threat.eta,time);}}
        if(!threat.victims.empty())result.push_back(std::move(threat));
    }
    for(auto& enemy:world.enemies)if(enemy.alive)for(auto& cast:enemy.abilities){
        auto* data=ability_data::find(cast.token);if(!data||!cast.valid||!observable(*data)||data->projectile)continue;
        bool charge=charge_impact(*data),native_area=native_area_impact(*data),lockon=lockon_impact(*data);
        if(native_area&&(!cast.has_impact_position||!std::isfinite(cast.impact_time)||cast.impact_time<=world.time))continue;
        if(lockon&&(cast.targets.empty()||!std::isfinite(cast.impact_time)||cast.impact_time<=world.time))continue;
        if(!cast.casting&&!cast.channeling&&!(cast.has_impact_position&&cast.impact_time>world.time))continue;
        Threat threat;threat.source=enemy.base;threat.token=data->token;threat.aoe=data->aoe||native_area;threat.id=enemy.base^((u64)data->token<<17)^((u64)(std::max(cast.cast_start,cast.channel_start)*1000));
        float start=cast.casting?cast.cast_start:cast.channel_start;
        threat.eta=cast.impact_time>world.time?cast.impact_time-world.time:(cast.casting?data->cast:data->channel)-(world.time-start);
        if(charge){
            Entity movement=enemy;movement.pos=enemy.pos;threat.eta=100;
            for(auto* victim:victims){float t=intercept(movement,*victim,world_units::units(2.2f));if(t>=0&&clear_path(world,enemy.pos+Vec3{0,0,40},victim->pos+Vec3{0,0,40},collision::Ability)){threat.eta=std::min(threat.eta,t);threat.victims.push_back(victim->handle);}}
        }else if(!cast.targets.empty()){
            // Explicit lock-on target handles prevent a nearby, unrelated cast triggering a dodge.
            for(auto* victim:victims)if(std::find(cast.targets.begin(),cast.targets.end(),victim->handle)!=cast.targets.end()&&clear_path(world,enemy.pos+Vec3{0,0,40},victim->pos+Vec3{0,0,40},collision::Ability))threat.victims.push_back(victim->handle);
        }else if(threat.aoe&&data->radius>0&&(data->self_centered||cast.has_impact_position)){
            Vec3 center=cast.has_impact_position?cast.impact_position:enemy.pos;
            for(auto* victim:victims)if((victim->pos+victim->vel*std::max(0.f,threat.eta)).dist(center)<=data->radius+24&&clear_path(world,center+Vec3{0,0,16},victim->pos+Vec3{0,0,40},collision::Ability))threat.victims.push_back(victim->handle);
        }
        if(threat.eta>=0&&threat.eta<=2&&!threat.victims.empty())result.push_back(std::move(threat));
    }
    std::sort(result.begin(),result.end(),[](auto& a,auto& b){return a.eta<b.eta;});return result;
}
Decision choose(const World& world,const std::vector<Threat>& threats){
    Decision none;if(!world.usable||!world.local.alive||world.local.invuln||!world.local.modifiers_valid)return none;
    auto p=prefix(world.local.hero);const char* key=world.local.hero==11?"citadel_ability_void_sphere":(world.local.hero==77?"ability_fencer_riposte":(world.local.hero==63?"ability_vampirebat_batblink":nullptr));
    auto* defensive=key?named(key):nullptr;auto* dodge=defensive?ability(world.local,defensive->token):nullptr;
    auto* counter=named("upgrade_counterspell");auto* owned=counter?ability(world.local,counter->token):nullptr;auto* parry_data=named("citadel_ability_melee_parry");auto* parry=parry_data?ability(world.local,parry_data->token):nullptr;
    for(auto& threat:threats){
        if(threat.eta*1000>cfg.F("defense.lead_ms"))continue;
        auto* incoming=ability_data::find(threat.token);if(!incoming)continue;
        bool self=contains(threat,world.local.handle);
        bool nearby_friend=false;if(world.local.hero==11&&threat.aoe&&!incoming->single)for(auto& ally:world.friends)if(ally.alive&&contains(threat,ally.handle)&&ally.pos.dist(world.local.pos)<=world_units::units(13))nearby_friend=true;
        bool selected=cfg.B(selection(p,threat.token).c_str());
        if(dodge&&dodge->ready&&cfg.B((p+"enabled").c_str())&&selected&&(self||nearby_friend)){
            if(nearby_friend&&cfg.B((p+"protect_team").c_str())){int alt=cfg.I((p+"alt_key").c_str());if(alt){bool modifier=cfg.I((p+"alt_mode").c_str())==0;return {Decision::GroupDodge,threat.id,modifier?cfg.I((p+"key").c_str()):alt,modifier?alt:0,std::string("Group dodge: ")+incoming->title};}}
            if(self)return {Decision::Dodge,threat.id,cfg.I((p+"key").c_str()),0,std::string("Defensive ability: ")+incoming->title};
        }
        if(self&&counterspell_observable(*incoming)&&owned&&owned->ready&&parry&&parry->ready&&cfg.B("defense.counterspell.enabled")&&cfg.B(selection("defense.counterspell.",threat.token).c_str()))return {Decision::Counterspell,threat.id,cfg.I("defense.counterspell.key"),0,std::string("Counterspell: ")+incoming->title};
    }
    return none;
}
void tick(){
    consumer_demand::note_defensive_tick();
    if(inp.panic||!game_data_permits_output()){note("Waiting for fresh, playable game data");return;}
    World world;unsigned actual_hero=0;bool enabled=false;
    {std::lock_guard<std::mutex> guard(snap_mtx);
        actual_hero=snap.local.hero;enabled=consumer_demand::defensive(cfg,actual_hero);
        if(enabled){world.local=snap.local;world.enemies=snap.enemies;world.friends=snap.friendlies;world.projectiles=snap.projectiles;world.time=snap.game_time;world.host_time=now_ms();world.usable=snap.frame_valid;world.require_los=snap.backend_managed;}
        else world.host_time=now_ms();
    }
    // Identity/reservation bookkeeping still runs on playable disabled frames.
    if(actual_hero!=last_hero){reset();last_hero=actual_hero;}
    if(world.host_time<reserved_until)return;for(auto it=handled.begin();it!=handled.end();)if(it->second<world.host_time)it=handled.erase(it);else ++it;
    if(!enabled){note("Defensive automation disabled");return;}
    auto threats=detect(world);threats.erase(std::remove_if(threats.begin(),threats.end(),[&](auto& t){return handled.count(t.id)!=0;}),threats.end());
    auto decision=choose(world,threats);if(decision.type==Decision::None){note("Watching selected threats");return;}
    if(!decision.key){note("Action blocked: bind its output key");return;}
    if(!hw::can_send(decision.key)||(decision.modifier&&!hw::can_send(decision.modifier))){note("Action blocked: keyboard injection is not verified on this device");return;}
    constexpr int press=35;
    if(decision.modifier)hw::key_chord(decision.modifier,decision.key,press);else hw::key_tap(decision.key,press);
    handled[decision.threat]=world.host_time+3000;reserved_until=world.host_time+(decision.type==Decision::Dodge||decision.type==Decision::GroupDodge?450:900);
    action_note(decision.reason);note(decision.reason);
}
}

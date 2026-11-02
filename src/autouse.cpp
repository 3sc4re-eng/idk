#include "autouse.h"
#include "consumer_demand.h"
#include "entity_health.h"
#include "ability_catalog.h"
#include "config.h"
#include "input.h"
#include "defense.h"
#include "world_units.h"
#include "collision.h"
#include "target_rules.h"
#include "ability_range.h"
#include <unordered_map>
namespace autouse{
static std::mutex status_mutex;static std::string message="Automatic usage disabled";
static void note(std::string text){std::lock_guard<std::mutex> lock(status_mutex);message=std::move(text);}
std::string status(){std::lock_guard<std::mutex> lock(status_mutex);return message;}
bool reload_window(float begin,float end,float pause,float fraction,float width,float time){
    if(!std::isfinite(begin+end+pause+fraction+width+time)||begin<0||end<=begin||pause<0||fraction<=0||fraction>=1||width<=0||width>2)return false;
    float effective=begin+pause;if(effective>=end)return false;float center=effective+(end-effective)*fraction;
    // Match the native primary-weapon window; wait until its centre, with a 20ms trailing margin.
    return time>=center&&time<center+width*.5f-.02f;
}
static bool clear_path(Vec3 from,Vec3 to,bool required){if(!required)return true;auto result=collision::trace(from,to,collision::Ability);return result.known&&result.clear;}
Choice choose(const Entity& local,const std::vector<Entity>& enemies,const std::vector<Entity>& friends,float time,bool require_los,u64 width,u64 height){
    Choice none;if(!local.alive||!local.modifiers_valid||!local.abilities_valid||local.invuln)return none;
    // Selected, removable enemy effects take priority over offensive item usage.
    for(int pass=0;pass<2;pass++)for(const auto& rule:rules()){
        if((pass==0)!=(rule.kind==Cleanse)||(rule.hero&&rule.hero!=local.hero))continue;
        auto p=prefix(rule.token);if(!cfg.B((p+"enabled").c_str()))continue;
        const Ability* own=nullptr;for(const auto& a:local.abilities)if(a.token==rule.token&&a.valid&&a.ready)own=&a;
        if(!own||(own->upgrade_bits&rule.upgrade_bits)!=rule.upgrade_bits)continue;
        int key=cfg.I((p+"key").c_str());if(!key)continue;
        if(rule.kind==Cleanse){bool selected=false;for(auto token:local.removable_debuffs)if(cfg.B(("cleanse.watch."+std::to_string(token)).c_str()))selected=true;if(!selected)continue;return {rule.token,own->base,key,false,std::string(rule.title)+": selected removable debuff"};}
        if(std::string(rule.key)=="upgrade_surging_power"&&(local.disarmed||local.reloading||!local.weapon_valid))continue;
        float health=0;if(!entity_health::percentage(local,health)||health>cfg.F((p+"health").c_str()))continue;
        float radius=world_units::units(cfg.F((p+"range").c_str()));
        float effective=ability_range::known_lower_bound(local,rule.token,rule.radius);
        if(rule.kind==Area||rule.kind==Targeted)radius=std::min(radius,effective);
        if(rule.kind==Targeted){
            // Unit-target items cast at the current reticle. Do not send an item key at an off-screen candidate.
            if(!width||!height)continue;
            for(const auto& enemy:enemies){
                if(!target_rules::selected(local,enemy,p)||enemy.pos.dist(local.pos)>radius)continue;
                if(std::string(rule.key)=="ability_ult_combo"){
                    int support=0;float support_range=world_units::units(cfg.F((p+"friend_range").c_str()));
                    for(const auto& ally:friends)if(ally.alive&&ally.team==local.team&&ally.pos.dist(enemy.pos)<=support_range&&clear_path(ally.pos+Vec3{0,0,40},enemy.pos+Vec3{0,0,55},require_los))support++;
                    if(support<cfg.I((p+"friends").c_str()))continue;
                }
                if(!clear_path(local.pos+Vec3{0,0,40},enemy.pos+Vec3{0,0,55},require_los))continue;
                auto screen=world_to_screen(enemy.pos+Vec3{0,0,55},width,height);if(screen.x<-1000||(screen-Vec2{width*.5f,height*.5f}).len()>6)continue;
                return {rule.token,own->base,key,false,std::string(rule.title)+": selected visible hero under crosshair"};
            }continue;
        }
        int count=0;for(const auto& enemy:enemies){if(!enemy.alive||!enemy.visible||enemy.invuln||enemy.stealthed)continue;
            if(enemy.pos.dist(local.pos)>radius||(enemy.pos+enemy.vel*rule.delay).dist(local.pos+local.vel*rule.delay)>radius)continue;
            if(!clear_path(local.pos+Vec3{0,0,40},enemy.pos+Vec3{0,0,40},require_los))continue;
            if(std::string(rule.key)=="upgrade_surging_power"&&enemy.bullet_invuln)continue;count++;
        }if(count<cfg.I((p+"enemies").c_str()))continue;
        int teammates=0;float teammate_range=world_units::units(cfg.F((p+"friend_range").c_str()));
        for(const auto& ally:friends)if(ally.alive&&ally.pos.dist(local.pos)<=teammate_range&&clear_path(local.pos+Vec3{0,0,40},ally.pos+Vec3{0,0,40},require_los))teammates++;
        if(teammates<cfg.I((p+"friends").c_str()))continue;
        return {rule.token,own->base,key,false,std::string(rule.title)+": "+std::to_string(count)+" visible enemies"};
    }
    if(cfg.B("auto.active_reload")&&local.weapon_valid&&local.reloading&&local.active_reload_allowed&&!local.disarmed&&reload_window(local.reload_begin,local.reload_end,local.reload_pause,local.reload_fraction,local.reload_width,time)){
        for(const auto& a:local.abilities)if(a.type=="CCitadel_Item_ActiveReload"&&a.valid&&a.ready&&!a.active_reload_failed)return {a.token,a.base,cfg.I("auto.reload_key"),true,"Active Reload: native success window"};
    }return none;
}
void tick(u64 width,u64 height){
    consumer_demand::note_automatic_tick();
    if(!game_data_permits_output()||inp.panic||defense::busy()){note("Waiting for playable data / defensive action");return;}
    Entity local;std::vector<Entity> enemies,friends;float time=0;bool require_los=false,enabled=false;u64 pawn=0;
    {std::lock_guard<std::mutex> lock(snap_mtx);
        pawn=snap.local.base;enabled=consumer_demand::automatic(cfg,snap.local.hero);
        if(enabled){local=snap.local;enemies=snap.enemies;friends=snap.friendlies;time=snap.game_time;require_los=snap.backend_managed;}
    }
    static u64 hero_pawn=0;static std::unordered_map<u64,u64> attempts;static u64 last_reload=0;
    if(hero_pawn!=pawn){attempts.clear();last_reload=0;hero_pawn=pawn;}
    if(!enabled){note("Automatic usage disabled");return;}
    auto choice=choose(local,enemies,friends,time,require_los,width,height);if(!choice.token){note("Watching configured conditions");return;}
    u64 reload_id=(u64)(local.reload_begin*1000);if(choice.reload&&reload_id==last_reload)return;
    u64 now=now_ms();auto found=attempts.find(choice.ability);if(found!=attempts.end()&&now-found->second<1500)return;
    if(cfg.I("input.mode")==2){if(cfg.B("dbg.action_log"))action_note("Automatic use (test only): "+choice.reason);}
    else{if(!hw::can_send(choice.key)){note("Action blocked: output key capability is not verified");return;}hw::key_tap(choice.key,30);}
    attempts[choice.ability]=now;if(choice.reload)last_reload=reload_id;note(((cfg.I("input.mode")==2)?"Test only: ":"Input attempted: ")+choice.reason);
}
}

#include "world_units.h"
#include "triggerbot.h"
#include "aim_model.h"
#include "native_joint_demand.h" // ACE_NATIVE_JOINT_INTEGRATION_V1
#include "lead.h"
#include "aimbot.h"
#include "input.h"
#include "profile_scope.h"
#include <windows.h>
namespace trig{
struct State{u64 last_shot=0,ready_at=0,last_tick=0,magnet_target=0;int magnet_bone=-1;bool armed=false;aim_model::Movement magnet;};
static State state;static bool toggled=false,last_key=false;static unsigned session=1;static profile_scope::Context previous_context;
bool is_armed(){return state.armed;}
u64 remaining_delay_ms(){u64 now=now_ms();return state.armed&&state.ready_at>now?state.ready_at-now:0;}
static void disarm(){state={};}
void tick(u64 sw,u64 sh){
    native_joint_demand::Tick native_demand(native_joint_demand::Consumer::Trigger);
    native_joint_demand::Tick native_magnet_demand(native_joint_demand::Consumer::Magnet);
    Entity local;std::vector<Entity> enemies;
    {std::lock_guard<std::mutex> guard(snap_mtx);local=snap.local;enemies=snap.enemies;
        if(!cfg.B("target.team_check"))enemies.insert(enemies.end(),snap.friendlies.begin(),snap.friendlies.end());}
    const auto context=profile_scope::for_entity(local);Settings::Scope active_settings(context.key,context.parents);
    bool key=game_key_down(cfg.I("trigger.key"));
    if(previous_context!=context){previous_context=context;toggled=false;disarm();}
    if(cfg.I("trigger.activation")==1&&key&&!last_key)toggled=!toggled;last_key=key;
    if(!game_data_permits_output()||(snap.backend_managed&&(!local.weapon_valid||local.disarmed||local.reloading))||!cfg.B("trigger.enabled")){toggled=false;disarm();return;}
    if(snap.backend_managed&&(!local.primary_shot.source||(local.selected_ability&&local.current_shot.source!=local.primary_shot.source))){disarm();return;}
    if(!aim_model::armed(cfg.I("trigger.activation"),key,toggled)||!local.base||inp.panic){disarm();return;}
    native_demand.request(native_joint_demand::selected_regions(),native_joint_demand::trigger_range());
    if(cfg.B("trigger.magnet"))native_magnet_demand.request(native_joint_demand::selected_regions(),native_joint_demand::magnet_range());
    u64 now=now_ms();
    if(!state.armed){state.armed=true;state.ready_at=now+aim_model::sample_delay(cfg.F("trigger.first_shot_delay_min"),cfg.F("trigger.first_shot_delay_max"));session=(unsigned)(rnd()*4294967295.0);}
    aim_model::Point shot,magnet;int shot_index=-1,magnet_index=-1;
    float magnet_radius=aim_model::fov_radius((float)sw,cfg.F("trigger.magnet_fov"));
    for(size_t i=0;i<enemies.size();i++){
        auto& e=enemies[i];if(!e.base||!e.alive||(cfg.B("target.team_check")&&e.team==local.team))continue;
        if(cfg.B("trigger.skip_fast")&&e.vel.len()>cfg.F("trigger.max_speed"))continue;
        const auto distance=local.pos.dist(e.pos);if(!std::isfinite(distance))continue;
        float roll=aim_model::acquisition_roll(e.base,session);
        aim_model::Point p;if(distance<=native_joint_demand::trigger_range())p=aim_model::weighted_select(e,sw,sh,roll,cfg.B("trigger.prediction")&&snap.backend_managed?1e30f:8);if(p.valid()&&cfg.B("trigger.prediction")&&snap.backend_managed){auto predicted=lead::point(local.primary_shot,e,p.bone,sw,sh);if(!predicted.valid)p={};else{p.screen=predicted.screen;p.distance=(p.screen-Vec2{sw*.5f,sh*.5f}).len();}}if(p.valid()&&p.distance<=8&&(!shot.valid()||p.distance<shot.distance)){shot=p;shot_index=(int)i;}
        int previous=e.base==state.magnet_target?state.magnet_bone:-1;
        p={};if(cfg.B("trigger.magnet")&&distance<=native_joint_demand::magnet_range())p=aim_model::weighted_select(e,sw,sh,roll,magnet_radius,previous);if(p.valid()&&cfg.B("trigger.magnet_prediction")&&snap.backend_managed){auto predicted=lead::point(local.primary_shot,e,p.bone,sw,sh);if(!predicted.valid)p={};else{p.screen=predicted.screen;p.distance=(p.screen-Vec2{sw*.5f,sh*.5f}).len();}}if(p.valid()&&(!snap.backend_managed||(e.visible_since&&now_ms()-e.visible_since>=(u64)cfg.F("trigger.magnet_visible_ms")))&&(!magnet.valid()||p.distance<magnet.distance)){magnet=p;magnet_index=(int)i;}
    }
    float dt=state.last_tick?(now-state.last_tick)/1000.f:1.f/60;state.last_tick=now;
    if(cfg.B("trigger.magnet")&&magnet_index>=0&&!aim::active){
        if(state.magnet_target!=enemies[magnet_index].base||state.magnet_bone!=magnet.bone){state.magnet={};state.magnet_target=enemies[magnet_index].base;state.magnet_bone=magnet.bone;}
        Vec2 center{sw*.5f,sh*.5f};const float zone=aim_model::deadzone_radius_pixels((float)sw,aim_model::capped_deadzone_fov(cfg.F("trigger.magnet_deadzone_fov"),cfg.F("trigger.magnet_fov")));Vec2 delta=aim_model::inside_deadzone(magnet.screen,center,zone)?Vec2{}:magnet.screen-center;
        Vec2 step;if(delta.len()<.001f||cfg.F("trigger.magnet_strength")<=0)state.magnet={};else step=hw::guided_move(delta,state.magnet,sw,sh,dt,true);step=step+state.magnet.remainder;int x=(int)step.x,y=(int)step.y;state.magnet.remainder={step.x-x,step.y-y};
        if(x||y)hw::mouse_move(x,y);
    }else{state.magnet={};state.magnet_target=0;state.magnet_bone=-1;}
    if(shot_index<0||now<state.ready_at)return;
    auto& enemy=enemies[shot_index];float angle=rad2deg(std::atan2(shot.distance,(float)sw*.5f));
    float chance=aim_model::hitchance(angle,enemy.vel.len(),local.vel.len()/600,cfg.F(aim_model::key(shot.bone,"_chance").c_str()));
    if(chance<cfg.F("trigger.hitchance_min"))return;
    hw::mouse_click(0,20);
    state.last_shot=now;state.ready_at=now+aim_model::sample_delay(cfg.F("trigger.followup_delay_min"),cfg.F("trigger.followup_delay_max"));
}
}

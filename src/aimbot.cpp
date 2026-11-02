#include "world_units.h"
#include "aimbot.h"
#include "entity_health.h"
#include "aim_model.h"
#include "native_joint_demand.h" // ACE_NATIVE_JOINT_INTEGRATION_V1
#include "lead.h"
#include "input.h"
#include "profile_scope.h"
#include <windows.h>
namespace aim{
std::string wait_reason="Waiting for activation";
float flight_seconds=0;std::string prediction_status="Prediction has not acquired a target";
bool active=false;int target_idx=-1;Vec2 target_screen{-99999,-99999};u64 override_target=0;
struct State{u64 base=0,since=0,engage_at=0,last_tick=0;u64 pending_base=0,pending_at=0;int bone=-1;unsigned stamp=0;aim_model::Movement movement;};
static State state;static bool toggle=false,last_key=false,was_engaged=false;static unsigned session=1;static u64 activation_started=0;
static void reset(){state={};active=false;target_idx=-1;target_screen={-99999,-99999};}
static profile_scope::Context previous_context;
void tick(u64 sw,u64 sh){
    native_joint_demand::Tick native_demand(native_joint_demand::Consumer::Aim);
    Entity local;std::vector<Entity> enemies;
    {std::lock_guard<std::mutex> guard(snap_mtx);local=snap.local;enemies=snap.enemies;enemies.insert(enemies.end(),snap.friendlies.begin(),snap.friendlies.end());if(!snap.backend_managed||native_target::supports_entity(EClass::NPC))enemies.insert(enemies.end(),snap.npcs.begin(),snap.npcs.end());}
    const auto context=profile_scope::for_entity(local);
    Settings::Scope active_settings(context.key,context.parents);
    if(context!=previous_context){reset();previous_context=context;}
    enemies.erase(std::remove_if(enemies.begin(),enemies.end(),[](const Entity& e){
        if(e.cls==EClass::Player)return !cfg.B("aim.target_players");
        bool neutral=e.type_name.find("Neutral")!=std::string::npos||e.type_name.find("Boss")!=std::string::npos;
        return e.cls!=EClass::NPC||!cfg.B(neutral?"aim.target_neutrals":"aim.target_minions");
    }),enemies.end());
    active=false;target_idx=-1;target_screen={-99999,-99999};wait_reason="No eligible visible aim region";
    bool key=game_key_down(cfg.I("aim.key"));
    if(cfg.I("aim.mode")==1&&key&&!last_key)toggle=!toggle;last_key=key;
    int mode=cfg.I("aim.mode");bool enabled=mode==2||(mode==1?toggle:key);
    if(enabled&&!was_engaged){session=(unsigned)(rnd()*4294967295.0);activation_started=now_ms();}if(!enabled)activation_started=0;was_engaged=enabled;
    const bool output_permitted=game_data_permits_output();
    const bool visibility_check=cfg.B("aim.visible_check");
    unsigned eligible=0,native_points=0,screen_points=0,known_points=0,clear_points=0;bool visibility_delay=false;
    if(!output_permitted||(snap.backend_managed&&(!local.weapon_valid||local.disarmed||local.reloading))||!enabled||!cfg.B("aim.enabled")||inp.panic||!local.base){reset();
        wait_reason=!cfg.B("aim.enabled")?"Aim disabled":(inp.panic?"Panic enabled":(!enabled?"Waiting for activation":(!output_permitted?game_output_reason():(!local.weapon_valid?"Weapon data unavailable":(local.disarmed?"Disarmed":(local.reloading?"Reloading":"Local player unavailable"))))));return;}
    if(activation_started&&now_ms()-activation_started<(u64)cfg.F("aim.start_delay_ms")){wait_reason="Aim start delay";state.movement={};return;}
    if(snap.backend_managed&&!cfg.B("aim.target_players")){reset();wait_reason="Player targets disabled";return;}
    if(snap.backend_managed&&!native_joint_demand::selected_regions()){reset();wait_reason="Select Head or Neck in target point priority";return;}
    if(cfg.B("aim.target_players"))native_demand.request(native_joint_demand::selected_regions(),world_units::units(cfg.F("aim.max_dist")));
    Vec2 center{sw*.5f,sh*.5f};float radius=aim_model::fov_radius((float)sw,cfg.F("aim.fov"));
    auto viable=[&](const Entity& e){return (!visibility_check||!snap.backend_managed||(e.visible_since&&now_ms()-e.visible_since>=(u64)cfg.F("aim.visible_ms")))&&e.base&&e.alive&&(!cfg.B("target.team_check")||e.team!=local.team)&&local.pos.dist(e.pos)<=world_units::units(cfg.F("aim.max_dist"))&&(!cfg.B("aim.skip_fast")||e.vel.len()<=cfg.F("aim.max_speed"));};
    int current=-1,best=-1;aim_model::Point locked,best_point;float best_score=1e30f;
    for(size_t i=0;i<enemies.size();i++){
        auto& e=enemies[i];
        if(!e.base||!e.alive||(cfg.B("target.team_check")&&e.team==local.team)||local.pos.dist(e.pos)>world_units::units(cfg.F("aim.max_dist"))||(cfg.B("aim.skip_fast")&&e.vel.len()>cfg.F("aim.max_speed")))continue;
        ++eligible;
        for(int region=0;region<aim_model::bone_count;++region)if(cfg.I(aim_model::key(region).c_str())>=0){
            Vec3 point;if(!aim_model::region_world(e,region,point))continue;++native_points;
            if(!aim_model::on_screen(world_to_screen(point,sw,sh),(float)sw,(float)sh))continue;++screen_points;
            if(e.bone_visibility_known[region])++known_points;if(e.bone_visible[region])++clear_points;
        }
        if(!viable(e)){visibility_delay|=visibility_check;continue;}
        if(snap.backend_managed&&cfg.I("aim.priority")==2&&!entity_health::current(e))continue;unsigned stamp=aim_model::bone_stamp();
        int preferred=e.base==state.base&&stamp==state.stamp?state.bone:-1;
        auto p=aim_model::weighted_select(e,sw,sh,aim_model::acquisition_roll(e.base,session),(e.base==state.base&&cfg.B("aim.hard_lock"))?1e30f:radius,preferred,visibility_check);if(!p.valid())continue;
        if(e.base==state.base){current=(int)i;locked=p;}
        if(p.distance>radius)continue;
        float score=cfg.I("aim.priority")==1?local.pos.dist(e.pos):(cfg.I("aim.priority")==2?e.hp:p.distance);
        if(e.base==override_target)score=-1;
        if(score<best_score){best_score=score;best=(int)i;best_point=p;}
    }
    u64 now=now_ms();
    if(current>=0&&cfg.B("aim.hard_lock")){best=current;best_point=locked;}
    if(best<0){
        wait_reason=!eligible?"No players pass target filters":!native_points?"Selected native joints unavailable (see joint log)":!screen_points?"Native joints outside the screen":visibility_check&&snap.backend_managed&&!known_points?"Visibility unknown: map or joint clearance unavailable":visibility_check&&snap.backend_managed&&!clear_points?"Selected joints obstructed":visibility_delay?"Waiting for minimum time in view":"No point passes FOV or target-state checks";
        state.movement={};state.pending_base=0;return;
    }
    if(state.base&&enemies[best].base!=state.base){
        if(state.pending_base!=enemies[best].base){state.pending_base=enemies[best].base;state.pending_at=now;}
        if(now-state.pending_at<(u64)cfg.F("aim.retarget_ms")){if(current>=0){best=current;best_point=locked;}else{state.movement={};return;}}
    }else state.pending_base=0;
    if(state.base!=enemies[best].base){state={};state.base=enemies[best].base;state.since=now;
        state.engage_at=now+aim_model::sample_delay(cfg.F("aim.act_delay_min"),cfg.F("aim.act_delay_max"));state.last_tick=now;}
    if(now<state.engage_at){wait_reason="Reaction delay";state.last_tick=now;return;}
    if(state.bone!=best_point.bone){state.bone=best_point.bone;state.movement={};state.stamp=aim_model::bone_stamp();}
    state.stamp=aim_model::bone_stamp();
    float dt=state.last_tick?(now-state.last_tick)/1000.f:1.f/60;state.last_tick=now;
    flight_seconds=0;if(cfg.B("aim.prediction")&&snap.backend_managed){auto predicted=lead::point(local.current_shot,enemies[best],best_point.bone,sw,sh,visibility_check);prediction_status=predicted.reason;if(!predicted.valid){wait_reason=prediction_status;state.movement={};return;}best_point.screen=predicted.screen;flight_seconds=predicted.solution.flight;prediction_status=local.current_shot.name+": estimated flight "+std::to_string((int)(flight_seconds*1000))+" ms";}else prediction_status="Projectile lead is off";
    // Handoff is measured against the SAME final point used for movement.
    // Prediction-unavailable returns above; an unverified intercept is never used.
    target_idx=best;target_screen=best_point.screen;
    const float deadzone=aim_model::deadzone_radius_pixels((float)sw,aim_model::capped_deadzone_fov(cfg.F("aim.deadzone_fov"),cfg.F("aim.fov")));
    if(aim_model::inside_deadzone(best_point.screen,center,deadzone)){wait_reason="Manual aim inside dead zone";state.movement={};return;}
    Vec2 delta=best_point.screen-center;
    if(delta.len()<.001f||cfg.F("aim.strength")<=0){wait_reason=cfg.F("aim.strength")<=0?"Aim strength is zero":"Manual aim inside dead zone";state.movement={};return;}
    wait_reason="Applying smooth aim";active=true;
    Vec2 step=hw::guided_move(delta,state.movement,sw,sh,dt,false)+state.movement.remainder;int x=(int)step.x,y=(int)step.y;state.movement.remainder={step.x-x,step.y-y};
    if(x||y)hw::mouse_move(x,y);
}
}

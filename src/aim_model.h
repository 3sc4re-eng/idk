#include "decision_frame.h"
#pragma once
#include "config.h"
#include "entities.h"
#include "target_geometry.h"
#include "dma.h"
#include "native_target.h" // ACE_NATIVE_JOINT_INTEGRATION_V1
#include <array>
namespace aim_model{
inline constexpr int bone_count=7;
inline constexpr const char* bone_names[]={"Head","Neck","Chest","Waist","Abdomen","Arms","Legs"};
inline constexpr const char* bone_keys[]={"head","neck","chest","waist","abdomen","arms","legs"};
inline constexpr float heights[]={78,68,55,44,36,50,20};
inline std::string key(int index,const char* suffix=""){return std::string("aim.bone_")+bone_keys[index]+suffix;}
struct Point{Vec2 screen{-99999,-99999};int bone=-1,priority=-1;float distance=1e30f;bool valid()const{return bone>=0;}};
inline bool on_screen(Vec2 p,float w,float h){return std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=0&&p.y>=0&&p.x<w&&p.y<h;}
inline bool region_world(const Entity& e,int index,Vec3& point){
    if(decision_frame::managed())return native_target::point(e,index,point,decision_frame::binding_epoch(),mem.attachment_epoch(),now_ms());
    if(target_geometry::region(e,index,point))return true;
    if(decision_frame::managed()||index<0||index>=bone_count)return false;
    point=e.pos+Vec3{0,0,heights[index]};return true; // Isolated settings/test fixtures only.
}
// Clearance is recorded at the same sampled native joint used for selection.
// Diagnostic bypass never creates absent joint coordinates.
inline bool clearance_current(const Entity& e){
    if(!decision_frame::managed())return true;
    const auto now=now_ms();if(!e.visibility_sample_ms||now<e.visibility_sample_ms||now-e.visibility_sample_ms>250)return false;
    const collision::Revision observed{e.visibility_scene,e.visibility_obstacles};
    if(const auto* frame=decision_frame::active())if(observed!=frame->geometry)return false;
    return observed==collision::revision();
}
inline Point select(const Entity& e,u64 w,u64 h,float limit=1e30f,bool require_visibility=true){
    Point best;if((require_visibility&&(!e.visible||!clearance_current(e)))||e.stealthed||e.invuln||e.bullet_invuln||(decision_frame::managed()&&!e.modifiers_valid))return best;
    Vec2 crosshair{w*.5f,h*.5f};
    for(int i=0;i<bone_count;i++){
        int priority=cfg.I(key(i).c_str());if(i==4||i==5||priority<0||(decision_frame::managed()&&require_visibility&&(!e.bone_visibility_known[i]||!e.bone_visible[i])))continue;
        Vec3 world;if(!region_world(e,i,world))continue;auto p=world_to_screen(world,w,h);if(!on_screen(p,(float)w,(float)h))continue;
        float distance=(p-crosshair).len();if(distance>limit)continue;
        if(priority>best.priority||(priority==best.priority&&distance<best.distance))best={p,i,priority,distance};
    }return best;
}
inline float projection_x(){if(const auto* frame=decision_frame::active())return frame->managed?frame->projection_x():1;if(!decision_frame::managed())return 1;std::lock_guard<std::mutex> lock(snap_mtx);return std::sqrt(snap.view[0]*snap.view[0]+snap.view[1]*snap.view[1]+snap.view[2]*snap.view[2]);}
inline unsigned bone_stamp(){unsigned v=2166136261u;for(int i=0;i<bone_count;i++)v=(v^(unsigned)(cfg.I(key(i).c_str())+2))*16777619u;return v;}
inline float acquisition_roll(u64 entity,unsigned session){u64 v=entity^(u64(session)<<32);v^=v>>33;v*=0xff51afd7ed558ccdULL;v^=v>>33;return (v&0xffffff)/16777216.f;}
inline Point weighted_select(const Entity& e,u64 w,u64 h,float roll,float limit=1e30f,int preferred=-1,bool require_visibility=true){
    Point none;if((require_visibility&&(!e.visible||!clearance_current(e)))||e.stealthed||e.invuln||e.bullet_invuln||(decision_frame::managed()&&!e.modifiers_valid))return none;
    std::array<Point,bone_count> points{};std::array<float,bone_count> weights{};float total=0;
    Vec2 center{w*.5f,h*.5f};
    for(int i=0;i<bone_count;i++){
        int priority=cfg.I(key(i).c_str());if(i==4||i==5||priority<0||(decision_frame::managed()&&require_visibility&&(!e.bone_visibility_known[i]||!e.bone_visible[i])))continue;
        Vec3 world;if(!region_world(e,i,world))continue;auto p=world_to_screen(world,w,h);if(!on_screen(p,(float)w,(float)h))continue;
        float distance=(p-center).len();if(distance>limit)continue;
        points[i]={p,i,priority,distance};
        if(i==preferred)return points[i];
        float importance=priority==2?6.f:(priority==1?3.f:1.f);
        weights[i]=importance/std::pow(distance+18.f,1.5f);total+=weights[i];
    }
    if(total<=0)return none;float pick=clampf(roll,0,.999999f)*total;
    for(int i=0;i<bone_count;i++){if(weights[i]<=0)continue;pick-=weights[i];if(pick<0)return points[i];}
    return none;
}
inline float fov_radius(float width,float degrees,float projection){return width*.5f*projection*std::tan(deg2rad(degrees*.5f));}
inline float fov_radius(float width,float degrees){return fov_radius(width,degrees,projection_x());}
// A manual handoff ring cannot extend beyond its acquisition cone.
inline constexpr float capped_deadzone_fov(float angle,float acquisition_fov){
    constexpr float largest=std::numeric_limits<float>::max();
    if(!(angle>=0)||!(acquisition_fov>0)||angle>largest||acquisition_fov>largest)return 0;
    return std::min(angle,std::min(20.f,acquisition_fov));
}
// Both angular settings describe the FULL cone; the projected ring is its radius.
inline float deadzone_radius_pixels(float width,float full_cone_degrees,float projection){
    if(!std::isfinite(width)||!std::isfinite(full_cone_degrees)||!std::isfinite(projection)||width<=0||projection<=0)return 0;
    return fov_radius(width,clampf(full_cone_degrees,0,20),projection);
}
inline float deadzone_radius_pixels(float width,float full_cone_degrees){return deadzone_radius_pixels(width,full_cone_degrees,projection_x());}
inline bool inside_deadzone(Vec2 region,Vec2 crosshair,float radius){return std::isfinite(radius)&&radius>=0&&std::isfinite((region-crosshair).len())&&(region-crosshair).len()<=radius;}

inline Vec2 bubble_center(Vec2 target,Vec2 crosshair,float radius,int mode,int direction,float angle){
    if(mode==0||radius<=0)return target;
    Vec2 unit{std::cos(deg2rad(angle)),std::sin(deg2rad(angle))};
    Vec2 away=crosshair-target;if(direction==1&&away.len()>.001f)unit=away*(1.f/away.len());
    return target+unit*radius;
}
inline Vec2 correction(Vec2 target,Vec2 crosshair,float radius,int mode,int direction,float angle){
    Vec2 center=bubble_center(target,crosshair,radius,mode,direction,angle);
    Vec2 delta=center-crosshair;
    if(mode==0&&delta.len()<=radius)return {};
    if(mode==1&&delta.len()<.5f)return {};
    return delta;
}
inline float ease(int curve,float t){
    t=clampf(t,0,1);if(curve==0)return t;
    if(curve==1)return 1-(1-t)*(1-t)*(1-t);
    if(curve==2)return t*t*(3-2*t);
    if(curve==4)return t-std::sin(2*std::numbers::pi_v<float>*t)/(2*std::numbers::pi_v<float>);
    float u=1-t;return 3*u*u*t*.15f+3*u*t*t*.9f+t*t*t;
}
struct Movement{float progress=0,elapsed=0;Vec2 remainder{},velocity{};};
inline float smoothing_factor(float percent){return std::pow(400.f,(clampf(percent,1,100)-1)/99.f);}
inline Vec2 movement(Vec2 delta,Movement& state,float smooth,int curve,float dt,float strength=1){
    if(!std::isfinite(dt)||dt<=0||!std::isfinite(smooth)||!std::isfinite(strength)||!std::isfinite(delta.len())){state={};return {};}smooth=clampf(smooth,1,400);dt=std::min(dt,1.f/15);strength=clampf(strength,0,1);
    float old=state.progress;state.progress=clampf(old+dt/(smooth*.045f),0,1);float t=(old+state.progress)*.5f,u=1-t;
    float response=curve==0?1.f:(curve==1?.45f+2*u*u:(curve==2?.4f+1.8f*std::sin(std::numbers::pi_v<float>*t):.45f*u*u+4.5f*u*t+.3f*t*t));
    if(curve==4)response=.3f+2.f*std::sin(std::numbers::pi_v<float>*t)*std::sin(std::numbers::pi_v<float>*t);
    float fraction=1-std::exp(-dt*60/smooth*response*strength);Vec2 step=delta*clampf(fraction,0,1);
    float cap=strength<=.4f?8.f:120.f;
    step.x=clampf(step.x,-cap,cap);step.y=clampf(step.y,-cap,cap);return step;
}
// Angular controller: proportional response plus damping, with no integral accumulator.
// Limits use degrees/second and degrees/second squared, independent of render rate.
inline Vec2 controlled_step(Vec2 error,Movement& state,float smooth,int curve,float dt,float strength,int mode,float damping,float max_speed,float max_acceleration,float ramp_ms,bool distance_scale,float near_strength,float ramp_angle){
    if(!std::isfinite(error.len())||strength<=0||error.len()<1e-7f){state={};return {};}
    state.elapsed+=dt;
    float ramp=1;if(ramp_ms>0){ramp=clampf(state.elapsed/(ramp_ms*.001f),0,1);ramp=ramp*ramp*(3-2*ramp);}
    float distance=clampf(error.len()/std::max(.01f,ramp_angle),0,1);distance=distance*distance*(3-2*distance);
    float gain=clampf(strength,0,1)*ramp*(distance_scale?clampf(near_strength,0,1)+(1-clampf(near_strength,0,1))*distance:1);
    Vec2 desired;
    if(mode==0){auto delta=movement(error,state,smooth,curve,dt,gain);desired=delta*(1/dt);}
    else{
        state.progress=clampf(state.progress+dt/(clampf(smooth,1,400)*.045f),0,1);
        float omega=4/(clampf(smooth,1,400)*.045f)*std::sqrt(gain),z=clampf(damping,1,2);
        auto axis=[&](float e,float v){
            if(omega<1e-5f)return 0.f;
            if(z<1.001f){float decay=std::exp(-omega*dt);return (v+(omega*omega*e-omega*v)*dt)*decay;}
            float root=std::sqrt(z*z-1),a=-omega*(z-root),b=-omega*(z+root),c=(-v-b*e)/(a-b),d=e-c;
            return -a*c*std::exp(a*dt)-b*d*std::exp(b*dt);
        };desired={axis(error.x,state.velocity.x),axis(error.y,state.velocity.y)};
    }
    float length=desired.len();if(length>max_speed)desired=desired*(max_speed/length);
    Vec2 change=desired-state.velocity;float acceleration=max_acceleration*dt;if(change.len()>acceleration)change=change*(acceleration/change.len());
    Vec2 next=state.velocity+change,step=(state.velocity+next)*(.5f*dt);state.velocity=next;
    // A near-target stop never issues a final jump past the requested point.
    auto stop_crossing=[](float error,float& step,float& velocity){if((error>=0&&step>error)||(error<=0&&step<error)){step=error;velocity=0;}};
    stop_crossing(error.x,step.x,state.velocity.x);stop_crossing(error.y,step.y,state.velocity.y);
    return step;
}
inline Vec2 controlled_motion(Vec2 error,Movement& state,float smooth,int curve,float dt,float strength,int mode,float damping,float max_speed,float max_acceleration,float ramp_ms,bool distance_scale,float near_strength,float ramp_angle){
    if(!std::isfinite(dt)||dt<=0||dt>.1f||!std::isfinite(error.len())||!std::isfinite(smooth)||!std::isfinite(strength)||!std::isfinite(damping)||!std::isfinite(max_speed)||!std::isfinite(max_acceleration)||!std::isfinite(ramp_ms)||!std::isfinite(near_strength)||!std::isfinite(ramp_angle)||max_speed<=0||max_acceleration<=0){state={};return {};}
    if(!std::isfinite(state.velocity.len())||!std::isfinite(state.progress)||!std::isfinite(state.elapsed))state={};
    int count=std::max(1,(int)std::ceil(dt*240));float step_time=dt/count;Vec2 result{};
    for(int i=0;i<count;i++)result=result+controlled_step(error-result,state,smooth,curve,step_time,strength,mode,damping,max_speed,max_acceleration,ramp_ms,distance_scale,near_strength,ramp_angle);
    return result;
}
// One bounded curve policy for Aim and Magnet; profile ramp/controller keys are retired.
inline constexpr float internal_max_turn=180.f,internal_max_acceleration=1440.f;
inline Vec2 consistent_motion(Vec2 error,Movement& state,float smooth,int curve,float dt,float strength){
    return controlled_motion(error,state,smooth,curve,dt,strength,0,1.f,internal_max_turn,internal_max_acceleration,0.f,false,1.f,1.f);
}
inline float hitchance(float angle,float target_speed,float own_motion,float modifier){
    float chance=100-clampf(angle*2.2f,0,40)-clampf(target_speed/25,0,25)-(own_motion*100)*.12f;
    return clampf(clampf(chance,0,100)*clampf(modifier,0,2),0,100);
}
inline bool fast(float own,float target){return own>600||target>600;}
inline bool armed(int mode,bool key,bool toggled){return mode==1?toggled:key;}
inline u64 sample_delay(float a,float b){return (u64)rnd_rng(std::fmax(1.f,std::fmin(a,b)),std::fmax(1.f,std::fmax(a,b)));}
}

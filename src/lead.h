#pragma once
#include "aim_model.h"
#include "collision.h"
namespace lead{
struct Result{bool valid=false;Vec2 screen{};prediction::Solution solution;std::string reason;};
inline prediction::Solution calculate(const prediction::Shot& shot,Vec3 target,Vec3 velocity,u64 target_host_sample=0){
    Vec3 camera,own_velocity;u64 stamp=0;bool valid=false;{std::lock_guard<std::mutex> lock(snap_mtx);camera=snap.camera;valid=snap.camera_valid&&snap.frame_valid;own_velocity=snap.local.vel;stamp=snap.last_read_ms;}
    u64 now=now_ms();if(!valid||now<stamp||now-stamp>250)return {};
    if(target_host_sample&&(now<target_host_sample||now-target_host_sample>250))return {};
    const auto point_sample=target_host_sample?target_host_sample:stamp;
    float delay=(now-point_sample+cfg.I("input.latency_ms")+cfg.I("input.delay_ms"))*.001f; // Host acquisition delay only; native pose age is unknown.
    return prediction::solve(shot,camera,own_velocity,target,velocity,delay);
}
inline bool clear_path(const prediction::Shot& shot,const prediction::Solution& solution){
    int steps=std::clamp((int)std::ceil(solution.flight/.01f),1,128);Vec3 before=solution.launch;
    for(int i=1;i<=steps;i++){auto next=shot.kind==prediction::Kind::Beam?solution.point:prediction::position(solution,solution.flight*i/steps,shot.gravity);auto hit=collision::trace(before,next,collision::Bullet);if(!hit.known||!hit.clear)return false;before=next;}return true;
}
inline Result point(const prediction::Shot& shot,const Entity& target,int bone,u64 width,u64 height,bool visibility_check=true){
    Result result;result.reason=shot.reason;if(!shot.valid||bone<0||bone>=aim_model::bone_count||shot.kind==prediction::Kind::UnitTarget)return result;
    Vec3 region;if(!aim_model::region_world(target,bone,region)){result.reason=snap.backend_managed?"Native target joint unavailable":"Target bounds unavailable"; // ACE_NATIVE_JOINT_INTEGRATION_V1
    return result;}result.solution=calculate(shot,region,target.vel,snap.backend_managed?target.native_joints.started_ms:0);if(!result.solution.valid){result.reason="No reachable projectile intercept";return result;}
    if(visibility_check&&!clear_path(shot,result.solution)){result.reason="Projectile path blocked or collision unavailable";return result;}
    result.screen=world_to_screen(result.solution.point,width,height);result.valid=aim_model::on_screen(result.screen,(float)width,(float)height);if(!result.valid)result.reason="Predicted aim point is outside the screen";return result;
}
}

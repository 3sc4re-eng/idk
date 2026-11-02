#pragma once
#include "utils.h"
namespace prediction{
enum class Kind{Unavailable,Projectile,Beam,UnitTarget};
struct Shot{
    Kind kind=Kind::Unavailable;bool valid=false,variable_speed=false;
    float speed=0,gravity=0,inherit_velocity=0,delay=0,lifetime=0,range=0,radius=0;
    u64 source=0;u32 token=0;std::string name,reason="Ballistics not available";
};
struct Solution{bool valid=false;Vec3 point{},launch{},velocity{},target{};float flight=0,delay=0;};
inline bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline Solution solve(const Shot& shot,Vec3 origin,Vec3 shooter_velocity,Vec3 target,Vec3 target_velocity,float additional_delay=0){
    Solution result;
    if(!shot.valid||shot.kind==Kind::Unavailable||!finite(origin)||!finite(shooter_velocity)||!finite(target)||!finite(target_velocity)||!std::isfinite(additional_delay)||additional_delay<0||additional_delay>1||!std::isfinite(shot.delay)||shot.delay<0||shot.delay>5)return result;
    float delay=shot.delay+additional_delay;Vec3 launch=origin+shooter_velocity*delay,at_launch=target+target_velocity*delay;
    if(shot.kind==Kind::Beam){if(shot.range>0&&launch.dist(at_launch)>shot.range)return result;result.valid=true;result.point=at_launch;result.launch=launch;result.target=at_launch;result.delay=delay;return result;}
    if(!std::isfinite(shot.speed)||shot.speed<=0||!std::isfinite(shot.gravity)||shot.gravity<0||!std::isfinite(shot.inherit_velocity)||shot.inherit_velocity<0||shot.inherit_velocity>2)return result;
    float maximum=shot.lifetime>0?std::min(shot.lifetime,8.f):8.f;if(shot.range>0)maximum=std::min(maximum,shot.range/shot.speed);if(!std::isfinite(maximum)||maximum<=0)return result;
    Vec3 r=at_launch-launch,v=target_velocity-shooter_velocity*shot.inherit_velocity;
    auto required=[&](float t){return r+v*t+Vec3{0,0,.5f*shot.gravity*t*t};};
    auto error=[&](float t){return required(t).len()-shot.speed*t;};
    float low=0,high=0,previous=error(0);if(previous<.001f)return result;
    // Find the first physically reachable root. The later high arc is deliberately not chosen.
    for(int i=1;i<=256;i++){float t=maximum*i/256,now=error(t);if(!std::isfinite(now))return result;if(now<=0&&previous>=0){high=t;break;}low=t;previous=now;}
    if(high<=0)return result;
    for(int i=0;i<36;i++){float mid=(low+high)*.5f;if(error(mid)>0)low=mid;else high=mid;}
    float flight=(low+high)*.5f;Vec3 needed=required(flight);float distance=needed.len();if(distance<=0||!finite(needed))return result;
    result.valid=true;result.point=launch+needed;result.launch=launch;result.velocity=needed*(shot.speed/distance)+shooter_velocity*shot.inherit_velocity;result.target=target+target_velocity*(delay+flight);result.flight=flight;result.delay=delay;return result;
}
inline Vec3 position(const Solution& result,float time,float gravity){return result.launch+result.velocity*time+Vec3{0,0,-.5f*gravity*time*time};}
}

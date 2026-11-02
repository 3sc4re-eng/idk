#pragma once
#include "entities.h"
#include <array>
// Regions are proportions of each entity's collision bounds, not animated bones.
namespace target_geometry{
inline bool finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool bounds(const Entity& e,Vec3& low,Vec3& high){
    if(!e.bounds_valid||!finite(e.bounds_min)||!finite(e.bounds_max)||!std::isfinite(e.scale)||e.scale<.01f||e.scale>100)return false;
    low=e.bounds_min*e.scale;high=e.bounds_max*e.scale;Vec3 size=high-low;
    return size.x>.01f&&size.y>.01f&&size.z>.01f&&size.len()<10000;
}
inline Vec3 transform(const Entity& e,Vec3 p){float yaw=deg2rad(e.facing.y),c=std::cos(yaw),s=std::sin(yaw);return e.pos+Vec3{p.x*c-p.y*s,p.x*s+p.y*c,p.z};}
inline bool region(const Entity& e,int index,Vec3& result){
    static constexpr float fraction[]={.93f,.83f,.68f,.52f,.42f,.62f,.24f};
    Vec3 low,high;if(index<0||index>=7||!bounds(e,low,high)||!finite(e.pos)||!finite(e.facing))return false;
    result=transform(e,{(low.x+high.x)*.5f,(low.y+high.y)*.5f,low.z+(high.z-low.z)*fraction[index]});return finite(result);
}
inline bool corners(const Entity& e,std::array<Vec3,8>& result){
    Vec3 low,high;if(!bounds(e,low,high)||!finite(e.pos)||!finite(e.facing))return false;
    for(int i=0;i<8;i++)result[i]=transform(e,{i&1?high.x:low.x,i&2?high.y:low.y,i&4?high.z:low.z});return true;
}
}

#pragma once
#include "entities.h"
namespace native_target {
// Published live acquisition capabilities, not a proposed anatomical mapping.
inline constexpr bool supports_region(int region) noexcept {return region==0||region==1;}
inline constexpr bool supports_entity(EClass kind) noexcept {return kind==EClass::Player;}
// Only verified cached joint translations. No collision-height fallback or
// transform is applied; the native cache already contains WORLD coordinates.
inline bool point(const Entity& e,int region,Vec3& world,u64 binding_epoch,u64 active_epoch,u64 now){
    if(!supports_entity(e.cls)||!supports_region(region)||!binding_epoch||binding_epoch!=active_epoch)return false;
    const auto& p=e.native_joints;
    if(!e.host_sample_ms||now<e.host_sample_ms||now-e.host_sample_ms>250||p.started_ms<e.host_sample_ms)return false;
    if(p.binding_epoch!=binding_epoch||!(p.valid_mask&(1u<<region))||
       !p.started_ms||p.completed_ms<p.started_ms||now<p.completed_ms||
       now<p.started_ms||now-p.started_ms>250)return false;
    const auto value=p.world[region];
    if(!std::isfinite(value.x)||!std::isfinite(value.y)||!std::isfinite(value.z))return false;
    world=value;return true;
}
} // namespace native_target

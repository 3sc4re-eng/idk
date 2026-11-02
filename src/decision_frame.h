#pragma once
#include "entities.h"
#include "collision.h"
#include <utility>

// One owned publication per automatic decision. Camera, actors, calibration,
// clock/key observations and attachment identity cannot drift between helpers.
namespace decision_frame {
struct Snapshot {
    Entity local;std::vector<Entity> targets;std::array<float,16> view{};
    std::array<bool,256> game_keys{};Vec3 camera{};
    float sensitivity=0,fov_sensitivity=1,units_per_metre=0;
    u64 binding_epoch=0,entity_sample_ms=0,matrix_sample_ms=0;
    bool managed=false,frame_valid=false,action_valid=false,camera_valid=false;
    bool input_valid=false,in_match=false,life_known=false,dead=false;
    bool scoreboard=false,spectating=false,raw_ui_valid=false,ace_menu_open=false;
    unsigned raw_ui_flags=0;
    collision::Revision geometry{};
    bool fresh(u64 now)const noexcept {
        return !managed||(frame_valid&&entity_sample_ms&&matrix_sample_ms&&now>=entity_sample_ms&&now>=matrix_sample_ms&&now-entity_sample_ms<=250&&now-matrix_sample_ms<=250);
    }
    Vec2 project(Vec3 point,u64 width,u64 height)const {
        const auto& m=view;const float w=point.x*m[12]+point.y*m[13]+point.z*m[14]+m[15];
        if(!std::isfinite(w)||w<.01f)return {-99999,-99999};
        const float x=point.x*m[0]+point.y*m[1]+point.z*m[2]+m[3],y=point.x*m[4]+point.y*m[5]+point.z*m[6]+m[7];
        if(!std::isfinite(x)||!std::isfinite(y))return {-99999,-99999};
        return {float(width)*.5f*(1+x/w),float(height)*.5f*(1-y/w)};
    }
    float projection_x()const{return std::sqrt(view[0]*view[0]+view[1]*view[1]+view[2]*view[2]);}
};
inline Snapshot capture(bool allies,bool npcs){
    Snapshot out;{
    std::lock_guard<std::mutex> lock(snap_mtx);
    out.managed=snap.backend_managed.load();out.frame_valid=snap.frame_valid.load();out.action_valid=snap.action_valid.load();
    out.local=snap.local;out.targets=snap.enemies;
    if(allies)out.targets.insert(out.targets.end(),snap.friendlies.begin(),snap.friendlies.end());
    if(npcs)out.targets.insert(out.targets.end(),snap.npcs.begin(),snap.npcs.end());
    std::copy(std::begin(snap.view),std::end(snap.view),out.view.begin());out.game_keys=snap.game_keys;
    out.camera=snap.camera;out.camera_valid=snap.camera_valid;out.sensitivity=snap.sensitivity;out.fov_sensitivity=snap.fov_sensitivity;out.units_per_metre=snap.units_per_metre.load();
    out.binding_epoch=snap.native_binding_epoch.load();out.entity_sample_ms=snap.last_read_ms.load();out.matrix_sample_ms=snap.matrix_sample_ms.load();
    out.input_valid=snap.input_valid.load();out.in_match=snap.in_match.load();out.life_known=snap.local_alive_valid.load();out.dead=snap.dead.load();
    out.scoreboard=snap.in_scoreboard.load();out.spectating=snap.spectating.load();out.raw_ui_valid=snap.raw_ui_valid.load();out.raw_ui_flags=snap.raw_ui_flags.load();out.ace_menu_open=snap.overlay_menu.load();
    }out.geometry=collision::revision();return out;
}
inline thread_local const Snapshot* current_snapshot=nullptr;
inline const Snapshot* active()noexcept{return current_snapshot;}
inline bool managed()noexcept{return active()?active()->managed:snap.backend_managed.load();}
inline u64 binding_epoch()noexcept{return active()?active()->binding_epoch:snap.native_binding_epoch.load();}
class Scope {
    Snapshot value_;const Snapshot* previous_;
public:
    explicit Scope(bool allies,bool npcs=false):value_(capture(allies,npcs)),previous_(current_snapshot){current_snapshot=&value_;}
    explicit Scope(Snapshot sample):value_(std::move(sample)),previous_(current_snapshot){current_snapshot=&value_;}
    ~Scope(){current_snapshot=previous_;}
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
    Snapshot& get()noexcept{return value_;}
};
}

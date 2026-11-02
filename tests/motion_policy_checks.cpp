// Source-only proposal checks: syntax checked, never executed as a binary.
#include "aim_model.h"
#include <cassert>
#include <limits>
namespace motion_policy_checks {
static_assert(aim_model::internal_max_turn==180.f);
static_assert(aim_model::internal_max_acceleration==1440.f);
void source_only_checks(){
    aim_model::Movement state;state.velocity={3,4};state.remainder={.2f,.4f};
    auto step=aim_model::consistent_motion({40,10},state,3,1,.016f,0);assert(step.len()==0&&state.velocity.len()==0&&state.remainder.len()==0);
    state.velocity={3,4};step=aim_model::consistent_motion({40,10},state,3,1,.2f,1);assert(step.len()==0&&state.velocity.len()==0);
    state={};constexpr float dt=1.f/240;
    for(int i=0;i<240;++i){const auto previous=state.velocity;step=aim_model::consistent_motion({180,90},state,2,1,dt,1);assert(std::isfinite(step.len()));assert(state.velocity.len()<=180.001f);assert((state.velocity-previous).len()<=1440*dt+.01f);}
    state={};step=aim_model::consistent_motion({40,10},state,2,1,.016f,1);assert(step.len()>0); // no configurable acquisition ramp
    state={};step=aim_model::consistent_motion({std::numeric_limits<float>::infinity(),0},state,2,1,.016f,1);assert(step.len()==0&&state.velocity.len()==0);
    Settings settings;settings.register_all();assert(!settings.B("system.game_menu_hotkey"));assert(!settings.find("aim.controller")&&!settings.find("trigger.magnet_ramp_ms"));assert(settings.find("aim.smooth")&&settings.find("aim.curve")&&settings.find("aim.strength"));assert(settings.find("trigger.magnet_smooth")&&settings.find("trigger.magnet_curve")&&settings.find("trigger.magnet_strength"));
}
}

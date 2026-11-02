#pragma once
#include "config.h"
#include "autouse.h"
#include <atomic>

// Necessary configuration conditions only. Inventory/readiness is deliberately
// not cached here: an enabled consumer must keep receiving current observations.
namespace consumer_demand {
// A saved option is not an active consumer when its dispatcher is retired.
// Tick heartbeats avoid reading remote ability details for dormant routes.
inline constexpr u64 dispatcher_lease_ms=250;
inline std::atomic<u64> automatic_until{0},defensive_until{0};
inline constexpr bool dispatcher_active(u64 until,u64 now){return until&&now<=until&&until-now<=dispatcher_lease_ms;}
inline void note_automatic_tick(){automatic_until.store(now_ms()+dispatcher_lease_ms,std::memory_order_release);}
inline void note_defensive_tick(){defensive_until.store(now_ms()+dispatcher_lease_ms,std::memory_order_release);}
inline bool automatic_running(u64 now){return dispatcher_active(automatic_until.load(std::memory_order_acquire),now);}
inline bool defensive_running(u64 now){return dispatcher_active(defensive_until.load(std::memory_order_acquire),now);}
inline bool automatic(Settings& settings,u32 actual_hero){
    if(settings.B("auto.active_reload"))return true;
    for(const auto& rule:autouse::rules()){
        if(rule.hero&&rule.hero!=actual_hero)continue;
        if(settings.B((autouse::prefix(rule.token)+"enabled").c_str()))return true;
    }
    return false;
}
inline bool defensive(Settings& settings,u32 actual_hero){
    if(settings.B("defense.counterspell.enabled"))return true;
    // These are the actual native dodge implementations in defense::choose.
    if(actual_hero!=11&&actual_hero!=77&&actual_hero!=63)return false;
    return settings.B(("defense."+std::to_string(actual_hero)+".enabled").c_str());
}
} // namespace consumer_demand

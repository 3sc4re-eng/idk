#pragma once
#include "entities.h"
#include "ability_catalog.h"
#include "world_units.h"
namespace autouse{
enum Kind{Buff,Area,Cleanse,Targeted};
struct Rule{const char* key;const char* title;u32 token,hero;Kind kind;float radius,delay;unsigned upgrade_bits;};
inline const std::vector<Rule>& rules(){
    static const std::vector<Rule> result=[](){std::vector<Rule> entries;
        auto add=[&](const char* key,Kind kind,float radius,float delay,unsigned upgrade_bits=0){for(const auto& data:ability_data::entries)if(std::string(data.key)==key)entries.push_back({data.key,data.title,data.token,data.hero,kind,radius,delay,upgrade_bits});};
        add("upgrade_surging_power",Buff,0,0);add("upgrade_metal_skin",Buff,0,0);add("upgrade_return_fire",Buff,0,0);add("upgrade_unstoppable",Buff,0,0);add("upgrade_infuser",Buff,0,0);add("upgrade_silencer",Buff,0,0);add("upgrade_dps_aura",Buff,0,0);add("upgrade_colossus",Area,world_units::units(14),0);add("upgrade_nullification_aura",Area,world_units::units(20),.6f);add("ability_nano_shadow_step",Cleanse,0,0,4);add("upgrade_cold_front",Area,world_units::units(10),.6f);add("upgrade_arctic_blast",Area,world_units::units(16),.6f);
        add("citadel_ability_self_vacuum",Area,world_units::units(7),.2f);add("synth_affliction",Area,world_units::units(9),.6f);
        add("ability_ult_combo",Targeted,world_units::units(4),0);
        add("upgrade_reduce_debuff_duration",Cleanse,0,0);add("ability_smoke_bomb",Cleanse,0,0,8);
        add("upgrade_containment",Targeted,world_units::units(25),.1f);add("upgrade_greater_withering_whip",Targeted,world_units::units(32),.1f);add("upgrade_glitch",Targeted,world_units::units(20),.1f);
        return entries;
    }();return result;
}
inline std::string prefix(u32 token){return "auto."+std::to_string(token)+".";}
std::string status();void tick(u64 width=0,u64 height=0);
struct Choice{u32 token=0;u64 ability=0;int key=0;bool reload=false;std::string reason;};
Choice choose(const Entity& local,const std::vector<Entity>& enemies,const std::vector<Entity>& friends,float time,bool require_los,u64 width=0,u64 height=0);
bool reload_window(float begin,float end,float pause,float fraction,float width,float time);
}

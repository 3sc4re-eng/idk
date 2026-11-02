#pragma once
#include "entities.h"
#include "ability_catalog.h"
#include <cmath>
namespace ability_tracking{
enum class State{Unknown,Untrained,Ready,Cooldown,Casting,Unavailable};
struct View{unsigned token=0;State state=State::Unknown;int seconds=0,charges=0;};
inline View view(const Entity& entity,int slot,float game_time,bool fresh){
    View result;const auto* definition=ability_data::signature(entity.hero,slot);if(definition)result.token=definition->token;
    if(!fresh||!std::isfinite(game_time))return result;
    if(slot==3&&entity.public_ultimate_valid&&entity.public_ultimate_sample_ms&&now_ms()>=entity.public_ultimate_sample_ms&&now_ms()-entity.public_ultimate_sample_ms<=250){
        if(!entity.public_ultimate_trained){result.state=State::Untrained;return result;}
        float remaining=std::max(entity.public_ultimate_stopped_remaining,entity.public_ultimate_cooldown_end-game_time);
        if(remaining>0){result.state=State::Cooldown;result.seconds=(int)std::ceil(std::min(36000.f,remaining));return result;}
        result.state=State::Ready;return result;
    }
    if(!entity.abilities_valid)return result;
    for(auto& ability:entity.abilities)if(ability.slot==slot&&ability.valid){
        // Reject an unexpected token in a signature slot rather than labeling the wrong ability.
        if(!definition||ability.token!=definition->token)continue;
        result.charges=ability.charges;
        if(!ability.trained){result.state=State::Untrained;return result;}
        if(ability.casting||ability.channeling){result.state=State::Casting;return result;}
        if(ability.ready){result.state=State::Ready;return result;}
        if(ability.cooldown_end>game_time&&std::isfinite(ability.cooldown_end)){result.seconds=(int)std::ceil(std::min(36000.f,ability.cooldown_end-game_time));result.state=State::Cooldown;return result;}
        result.state=State::Unavailable;return result;
    }return result;
}
}

#pragma once
#include "entities.h"
namespace entity_health {
inline bool current(const Entity& e){return e.health_current_valid&&std::isfinite(e.hp)&&e.hp>=0&&e.hp<10000000;}
inline bool maximum(const Entity& e){return e.health_max_valid&&std::isfinite(e.max_hp)&&e.max_hp>0&&e.max_hp<10000000;}
inline bool pair(const Entity& e){return e.health_pair_valid&&current(e)&&maximum(e)&&e.hp<=e.max_hp;}
inline bool percentage(const Entity& e,float& value){if(!pair(e))return false;value=100.f*e.hp/e.max_hp;return std::isfinite(value)&&value>=0&&value<=100;}
} // namespace entity_health

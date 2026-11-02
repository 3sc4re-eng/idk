// Source-only helpers: syntax-checked, never executed as a test binary.
#include "entity_health.h"
#include <cassert>
#include <limits>
void entity_health_source_checks(){
    Entity e;e.valid=true;e.hp=2449;e.max_hp=500;e.health_current_valid=e.health_max_valid=true;e.health_pair_valid=false;e.life_state_valid=e.alive_valid=e.alive=true;
    float percentage=-123;assert(entity_health::current(e)&&!entity_health::pair(e));assert(!entity_health::percentage(e,percentage)&&percentage==-123);assert(e.valid&&e.alive);
    e.max_hp=2449;e.health_pair_valid=true;assert(entity_health::percentage(e,percentage)&&percentage==100);
    e.health_max_valid=false;assert(!entity_health::percentage(e,percentage)&&e.alive);
    e.health_max_valid=true;e.hp=std::numeric_limits<float>::quiet_NaN();assert(!entity_health::current(e)&&!entity_health::pair(e));
}

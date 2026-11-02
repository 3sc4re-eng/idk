#pragma once
#include "entities.h"
#include "ability_catalog.h"
#include <vector>
#include <string>
namespace defense{
struct Threat{u64 id=0,source=0;u32 token=0;float eta=0;bool aoe=false,projectile=false;std::vector<u32> victims;};
struct Decision{enum Type{None,Dodge,GroupDodge,Counterspell}type=None;u64 threat=0;int key=0,modifier=0;std::string reason;};
struct World{Entity local;std::vector<Entity> enemies,friends,projectiles;float time=0;u64 host_time=0;bool usable=false,require_los=false;};
std::vector<Threat> detect(const World&);Decision choose(const World&,const std::vector<Threat>&);
bool busy();void tick();std::string status();void reset();
std::string prefix(unsigned hero);std::string selection(const std::string& prefix,u32 token);
bool observable(const ability_data::Entry&);const char* observation(const ability_data::Entry&);
bool counterspell_observable(const ability_data::Entry&);const char* counterspell_observation(const ability_data::Entry&);
}

#include "ability_read_demand.h"
#include "consumer_demand.h"
using ability_read_demand::Consumers;
using ability_read_demand::required;
constexpr Consumers none{};
constexpr Consumers enemy_icons{true,true,false,true,false,false,false};
constexpr Consumers team_icons{true,false,true,false,true,false,false};
constexpr Consumers visual_master_off{false,true,true,true,true,false,false};
constexpr Consumers actions{false,false,false,false,false,true,true};
static_assert(required(none,true,false,false)); // local reload/weapon gates always need data
static_assert(!required(none,false,true,false));
static_assert(!required(enemy_icons,false,false,false));
static_assert(required(enemy_icons,false,true,false)&&!required(enemy_icons,false,true,true));
static_assert(required(team_icons,false,true,true)&&!required(team_icons,false,true,false));
static_assert(!required(visual_master_off,false,true,false)&&!required(visual_master_off,false,true,true));
static_assert(required(actions,false,true,false)&&required(actions,false,true,true));
static_assert(!consumer_demand::dispatcher_active(0,0));
static_assert(consumer_demand::dispatcher_active(1250,1000));
static_assert(consumer_demand::dispatcher_active(1250,1250));
static_assert(!consumer_demand::dispatcher_active(1250,1251));
static_assert(!consumer_demand::dispatcher_active(1250,999));

constexpr Consumers ultimate_only{true,true,true,false,false,false,false};
static_assert(!required(ultimate_only,false,true,false));
static_assert(required(ultimate_only,false,true,false,true));
static_assert(required(ultimate_only,false,true,true,true));
static_assert(!required(none,false,true,false,true));
static_assert(!required(visual_master_off,false,true,false,true));

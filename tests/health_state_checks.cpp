// Compile-time validity separation. No binary or live memory needed.
#include "health_state.h"
namespace health_state_checks {
constexpr health_state::PlayerMaximum upgraded{true,2449,2449,true};
constexpr auto full=health_state::evaluate(2449,500,0,true,&upgraded);
static_assert(full.current_valid&&full.maximum_valid&&full.pair_valid&&full.alive_valid&&full.alive);
static_assert(full.maximum==2449&&full.source==health_state::Source::ControllerGlobal);
constexpr auto missing=health_state::evaluate(2449,500,0,true,nullptr);
static_assert(missing.current_valid&&missing.alive_valid&&missing.alive&&!missing.maximum_valid&&!missing.pair_valid,"unknown max must not erase independent alive data");
constexpr auto above=health_state::evaluate(2450,500,0,true,&upgraded);
static_assert(above.current_valid&&above.maximum_valid&&above.alive_valid&&above.alive&&!above.pair_valid,"inconsistent pair cannot invalidate current HP/life");
constexpr health_state::PlayerMaximum bad_max{true,-1,500,true};
constexpr auto bad=health_state::evaluate(501,500,0,true,&bad_max);
static_assert(bad.current_valid&&bad.alive&&!bad.maximum_valid&&!bad.pair_valid);
constexpr auto invalid_current=health_state::evaluate(-1,500,0,true,&upgraded);
static_assert(!invalid_current.current_valid&&!invalid_current.alive_valid&&!invalid_current.alive);
constexpr auto unread_current=health_state::evaluate(0,500,0,true,&upgraded,false);
static_assert(!unread_current.current_valid&&!unread_current.alive_valid&&unread_current.maximum_valid);
constexpr auto unread_life=health_state::evaluate(500,500,0,true,&upgraded,true,true,false);
static_assert(unread_life.current_valid&&!unread_life.alive_valid&&unread_life.pair_valid);
constexpr auto zero_alive=health_state::evaluate(0,500,0,true,&upgraded);
static_assert(zero_alive.current_valid&&!zero_alive.alive_valid&&!zero_alive.alive);
constexpr auto dead=health_state::evaluate(0,500,2,true,&upgraded);
static_assert(dead.current_valid&&dead.life_valid&&dead.alive_valid&&!dead.alive);
static_assert(health_state::evaluate(600,600,0,false).alive);
}

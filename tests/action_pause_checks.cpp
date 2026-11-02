// SOURCE-ONLY proposal fixture. No main or output/hardware calls.
// static_asserts are compiler-evaluated; Settings parser assertions are unrun.
#include "action_pause_policy.h"
#include "config.h"
#include <cassert>

namespace action_pause_checks {
using namespace action_pause_policy;
static_assert(!operative_bool(legacy_key,true));
static_assert(!operative_bool(legacy_key,false));
static_assert(!operative_bool(diagnostic_key,diagnostic_default));
static_assert(operative_bool(diagnostic_key,true));
static_assert(operative_bool("input.disable_in_overlay",true));
static_assert(!regular_search_key(legacy_key)&&!regular_search_key(diagnostic_key));
static_assert(regular_search_key("input.disable_in_overlay"));
static_assert(!diagnostic_pause(false,true,3));
static_assert(!diagnostic_pause(true,false,3));
static_assert(diagnostic_pause(true,true,1)&&diagnostic_pause(true,true,2));
static_assert(!diagnostic_pause(true,true,4));
static_assert(ace_pause(true,true)&&!ace_pause(false,true)&&!ace_pause(true,false));
constexpr bool entire_byte_policy(){
    for(unsigned raw=0;raw<256;++raw){
        if(diagnostic_pause(false,true,raw)||diagnostic_pause(true,false,raw))return false;
        if(diagnostic_pause(true,true,raw)!=((raw&3u)!=0))return false;
        // Actual ACE pause is independent of both diagnostic/raw observations.
        if(!ace_pause(true,true)||ace_pause(false,true))return false;
    }
    return true;
}
static_assert(entire_byte_policy());

void source_only_saved_profile_checks_not_executed(){
    Settings saved;saved.register_all();
    assert(!saved.B(diagnostic_key)&&saved.B("input.disable_in_overlay"));
    assert(saved.import_json(json{{legacy_key,true}},false));
    assert(saved.find(legacy_key)->b&&!saved.B(legacy_key)&&!saved.B(diagnostic_key));
    assert(saved.export_json()[legacy_key]==true);
    saved.find(diagnostic_key)->b=true;
    assert(saved.import_json(json{{legacy_key,true}},false)&&!saved.B(diagnostic_key));
    assert(saved.import_json(json{{legacy_key,true},{diagnostic_key,false}},false)&&!saved.B(diagnostic_key));
    assert(saved.import_json(json{{legacy_key,false},{diagnostic_key,true}},false)&&saved.B(diagnostic_key));
    assert(diagnostic_pause(saved.B(diagnostic_key),true,3));
    assert(!diagnostic_pause(saved.B(diagnostic_key),false,3));
    saved.find("input.disable_in_overlay")->b=true;
    assert(ace_pause(saved.B("input.disable_in_overlay"),true));
    saved.find(diagnostic_key)->b=false;
    assert(ace_pause(saved.B("input.disable_in_overlay"),true));
    saved.find("input.disable_in_overlay")->b=false;
    assert(!ace_pause(saved.B("input.disable_in_overlay"),true));
    Settings shared;shared.register_all();
    assert(shared.import_json(json::parse(saved.export_json().dump()),false));
    assert(!shared.B(legacy_key)&&!shared.B(diagnostic_key)&&!shared.B("input.disable_in_overlay"));
}

// Compile-time specification checks: all optional-state combinations and mandatory
// admission failures. They generate no program and perform no runtime operations.
constexpr bool optional_states_are_independent(){
    for(unsigned options=0;options<16;++options){
        Preferences p;p.pause_when_dead=options&1;p.pause_in_scoreboard=options&2;
        p.pause_when_spectator=options&4;p.pause_in_ace_menu=options&8;
        for(unsigned observed=0;observed<16;++observed){
            Observations s;s.managed=s.snapshot_valid=s.input_valid=s.fresh=s.in_match=s.life_known=true;
            s.dead=observed&1;s.scoreboard=observed&2;s.spectator=observed&4;s.ace_menu_open=observed&8;
            if((evaluate(p,s)==Reason::Ready)!=((options&observed)==0))return false;
        }
    }
    return true;
}
constexpr bool mandatory_admission_cannot_be_disabled(){
    Preferences p;p.pause_when_dead=p.pause_in_scoreboard=p.pause_when_spectator=p.pause_in_ace_menu=false;
    Observations s;s.managed=s.snapshot_valid=s.input_valid=s.fresh=s.in_match=true;
    s.dead=s.scoreboard=s.spectator=s.ace_menu_open=true;
    if(evaluate(p,s)!=Reason::Ready)return false;
    s.snapshot_valid=false;if(evaluate(p,s)!=Reason::InvalidSnapshot)return false;s.snapshot_valid=true;
    s.input_valid=false;if(evaluate(p,s)!=Reason::InvalidInput)return false;s.input_valid=true;
    s.fresh=false;if(evaluate(p,s)!=Reason::StaleSnapshot)return false;s.fresh=true;
    s.in_match=false;if(evaluate(p,s)!=Reason::InactiveMatch)return false;s.in_match=true;
    p.pause_when_dead=true;if(evaluate(p,s)!=Reason::UnknownLife)return false;
    s.life_known=true;if(evaluate(p,s)!=Reason::Dead)return false;
    s.dead=false;if(evaluate(p,s)!=Reason::Ready)return false;
    p.diagnostic_raw_ui=true;s.raw_ui_readable=true;s.raw_ui_flags=1;
    if(evaluate(p,s)!=Reason::RawUiSignal)return false;
    s.managed=false;s.snapshot_valid=s.input_valid=s.fresh=s.in_match=false;
    if(evaluate(p,s)!=Reason::RawUiSignal)return false;
    p.diagnostic_raw_ui=false;if(evaluate(p,s)!=Reason::Ready)return false;
    p.pause_in_ace_menu=true;if(evaluate(p,s)!=Reason::AceMenu)return false;
    return true;
}
static_assert(optional_states_are_independent());
static_assert(mandatory_admission_cannot_be_disabled());
} // namespace action_pause_checks

#include "consumer_demand.h"
#include <cassert>

// Source-only fixture: configuration boundaries, no target/hardware calls.
void consumer_demand_checks(){
    Settings settings;settings.register_all();
    assert(!consumer_demand::automatic(settings,0)&&!consumer_demand::automatic(settings,11));
    settings.find("auto.active_reload")->b=true;assert(consumer_demand::automatic(settings,0)&&consumer_demand::automatic(settings,87));settings.find("auto.active_reload")->b=false;
    for(const auto& rule:autouse::rules()){
        auto key=autouse::prefix(rule.token)+"enabled";settings.find(key.c_str())->b=true;
        assert(consumer_demand::automatic(settings,rule.hero));
        const u32 different=rule.hero==11?77:11;
        assert(consumer_demand::automatic(settings,different)==(rule.hero==0));
        {Settings::Scope edited_other("Deadlock/hero/Unrelated");assert(consumer_demand::automatic(settings,rule.hero));}
        settings.find(key.c_str())->b=false;
    }
    assert(!settings.find("parry.enabled")&&!consumer_demand::defensive(settings,87));
    for(u32 hero:{11u,77u,63u}){
        auto key="defense."+std::to_string(hero)+".enabled";settings.find(key.c_str())->b=true;
        assert(consumer_demand::defensive(settings,hero));assert(!consumer_demand::defensive(settings,hero==11?77:11));assert(!consumer_demand::defensive(settings,0));settings.find(key.c_str())->b=false;
    }
    settings.find("defense.counterspell.enabled")->b=true;assert(consumer_demand::defensive(settings,0)&&consumer_demand::defensive(settings,87));settings.find("defense.counterspell.enabled")->b=false;
    assert(settings.import_json(json{{"parry.enabled",true},{"parry.heavy",true},{"parry.light",true}}));
    assert(!settings.find("parry.enabled")&&!consumer_demand::defensive(settings,87));
    assert(!settings.export_json().contains("parry.enabled"));
    // Output-disabled test reporting is still a consumer, not an all-off case.
    settings.find("input.mode")->i=2;settings.find("auto.active_reload")->b=true;assert(consumer_demand::automatic(settings,11)&&!consumer_demand::defensive(settings,11));
}

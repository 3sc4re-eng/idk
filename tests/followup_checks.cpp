#include "ability_tracking.h"
#include "ability_art.h"
#include "target_rules.h"
#include "hero_catalog.h"
#include "aim_model.h"
#include "input_timing.h"
#include <cassert>
#include <cstdio>
#include <set>
int main(){
    input_timing::Schedule schedule;assert(schedule.ready_at(100,4,2)==104);schedule.sent(100);assert(schedule.ready_at(101,2,8)==108);assert(schedule.ready_at(500,4,2)==504);
    cfg.register_all();
    for(const auto& p:cfg.m)assert(!p.second.help.empty());
    assert(!cfg.find("input.automatic_enabled")&&!cfg.find("input.block_output")&&cfg.B("system.autosave"));
    cfg.find("auto.reload_key")->i=0;cfg.find("esp.name_font")->f=25;
    assert(cfg.save("followup_roundtrip.json"));cfg.register_all();assert(cfg.load("followup_roundtrip.json"));assert(cfg.I("auto.reload_key")==0&&cfg.F("esp.name_font")==25);
    {std::ofstream f("followup_legacy_output.json");f<<"{\"dbg.dry_run\":false}";}assert(cfg.load("followup_legacy_output.json")&&!cfg.find("input.automatic_enabled"));
    {std::ofstream f("followup_legacy_output.json");f<<"{\"dbg.dry_run\":true}";}assert(cfg.load("followup_legacy_output.json")&&!cfg.find("input.automatic_enabled"));
    static_assert(!Settings::action_output_key_allowed(VK_LBUTTON)&&!Settings::action_output_key_allowed(VK_RBUTTON));
    static_assert(Settings::action_output_key_allowed(VK_MBUTTON)&&Settings::action_output_key_allowed(VK_XBUTTON1));
    {std::ofstream f("followup_output_policy.json");f<<"{\"defense.parry_key\":1,\"auto.reload_key\":2,\"aim.key\":1,\"trigger.key\":2,\"input.automatic_enabled\":false,\"input.block_output\":true}";}
    assert(cfg.load("followup_output_policy.json")&&cfg.I("defense.counterspell.key")==0&&cfg.I("auto.reload_key")==0&&cfg.I("aim.key")==VK_LBUTTON&&cfg.I("trigger.key")==VK_RBUTTON);
    assert(!cfg.find("input.automatic_enabled")&&!cfg.find("input.block_output"));
    assert(!cfg.find("parry.enabled")&&!cfg.find("defense.parry_key"));
    auto sanitized=cfg.export_json();assert(sanitized["defense.counterspell.key"]==0&&sanitized["auto.reload_key"]==0&&sanitized["aim.key"]==VK_LBUTTON&&sanitized["trigger.key"]==VK_RBUTTON);
    std::remove("followup_output_policy.json");
    std::set<unsigned> tokens;
    for(const auto& image:ability_art::images){assert(tokens.insert(image.token).second);assert(image.offset+image.size<=sizeof(ability_art::pixels));auto* data=ability_art::pixels+image.offset;assert(image.size>24&&data[0]==137&&data[1]=='P'&&data[2]=='N'&&data[3]=='G');}
    for(const auto& hero:current_heroes)for(int slot=0;slot<4;slot++){auto* a=ability_data::signature(hero.id,slot);assert(a&&tokens.count(a->token));}
    Entity unit;unit.hero=1;unit.abilities_valid=true;Ability a;a.slot=3;a.token=ability_data::signature(unit.hero,3)->token;a.valid=true;unit.abilities={a};
    using State=ability_tracking::State;
    assert(ability_tracking::view(unit,3,100,false).state==State::Unknown);
    assert(ability_tracking::view(unit,3,100,true).state==State::Untrained);
    unit.abilities[0].trained=true;unit.abilities[0].ready=true;assert(ability_tracking::view(unit,3,100,true).state==State::Ready);
    unit.abilities[0].ready=false;unit.abilities[0].cooldown_end=105.1f;auto status=ability_tracking::view(unit,3,100,true);assert(status.state==State::Cooldown&&status.seconds==6);
    unit.abilities[0].casting=true;assert(ability_tracking::view(unit,3,100,true).state==State::Casting);
    unit.abilities[0].casting=false;unit.abilities[0].cooldown_end=0;assert(ability_tracking::view(unit,3,100,true).state==State::Unavailable);
    unit.abilities[0].token=0;assert(ability_tracking::view(unit,3,100,true).state==State::Unknown);
    Entity local,enemy;local.team=1;local.souls=10000;enemy.team=2;enemy.hero=1;enemy.alive=true;enemy.visible=true;enemy.souls=15000;enemy.abilities_valid=true;
    std::string p="auto.1813726886.";assert(!target_rules::selected(local,enemy,p));cfg.find((p+"hero.1").c_str())->b=true;
    a={};a.token=3516947824u;a.valid=true;a.ready=true;enemy.abilities={a};assert(target_rules::selected(local,enemy,p));
    enemy.abilities[0].ready=false;assert(!target_rules::selected(local,enemy,p));enemy.abilities[0].ready=true;
    cfg.find((p+"soul_lead_only").c_str())->b=true;cfg.find((p+"soul_lead").c_str())->i=6000;assert(!target_rules::selected(local,enemy,p));cfg.find((p+"soul_lead").c_str())->i=3000;assert(target_rules::selected(local,enemy,p));
    enemy.hero=25;assert(!target_rules::selected(local,enemy,p));enemy.hero=1;enemy.visible=false;assert(!target_rules::selected(local,enemy,p));enemy.visible=true;enemy.abilities_valid=false;assert(!target_rules::selected(local,enemy,p));
    assert(target_rules::movement(2398133749u));
    for(int hz:{60,144,240}){aim_model::Movement motion;Vec2 error{8,3};float previous=error.len();for(int n=0;n<hz*60;n++){auto step=aim_model::controlled_motion(error,motion,18,4,1.f/hz,.25f,0,1,60,300,500,true,.4f,2);assert(std::isfinite(step.len())&&step.len()<=60.f/hz+.001f);error=error-step;assert(error.len()<=previous+.001f);previous=error.len();}assert(error.len()<.1f);}
    std::puts("PASS: current icons for all 44 heroes, ready/cooldown/unknown tracking, movement-ready and hero/soul filters, config migration/unbound keys, help metadata and bounded bell-curve tracking");
}

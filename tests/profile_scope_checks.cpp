// Source-only contract fixtures. No main; no backend/device/software execution.
#include "profile_scope.h"
#include <cassert>
static_assert(profile_scope::segment("Mo & Krill"));
static_assert(profile_scope::segment("weapon_ak47"));
static_assert(!profile_scope::segment("../Rifles")&&!profile_scope::segment("Rifles/Snipers"));
static_assert(!profile_scope::segment("")&&!profile_scope::segment(".."));

void profile_scope_checks(){
    using profile_subject::Kind;using profile_subject::Subject;
    Subject hero{Kind::Hero,"deadlock","Dynamo",{},true};
    auto context=profile_scope::for_subject(hero,&games::DEADLOCK);
    assert(context.key=="Deadlock/hero/Dynamo"&&context.parents.empty());
    assert(profile_scope::for_subject(hero,&games::CS2).key=="Counter-Strike 2/Global");
    hero.verified=false;assert(profile_scope::for_subject(hero,&games::DEADLOCK).key=="Deadlock/Global");
    Subject weapon{Kind::Weapon,"cs2","weapon_ak47","Rifles",true};
    context=profile_scope::for_subject(weapon,&games::CS2);
    assert(context.key=="Counter-Strike 2/weapon-item/weapon_ak47");
    assert(context.parents==std::vector<std::string>{"Counter-Strike 2/weapon/Rifles"});
    weapon.name.clear();assert(profile_scope::for_subject(weapon,&games::CS2).key=="Counter-Strike 2/weapon/Rifles");
    weapon.category.clear();assert(profile_scope::for_subject(weapon,&games::CS2).key=="Counter-Strike 2/Global");
    assert(profile_scope::for_subject(weapon,nullptr).key.empty());

    Settings settings;settings.register_all();
    const std::string global="Counter-Strike 2/Global",category="Counter-Strike 2/weapon/Rifles",item="Counter-Strike 2/weapon-item/weapon_ak47";
    settings.enable_override(global,true);settings.overrides[global]["aim.smooth"].f=20;
    settings.enable_override(category,true);settings.overrides[category]["aim.smooth"].f=60;
    {
        Settings::Scope scoped(item,{category});
        assert(settings.F("aim.smooth")==60);
        settings.enable_override(item,true,{category});
        assert(settings.F("aim.smooth")==60); // enabling starts with effective inherited settings
        settings.overrides[item]["aim.smooth"].f=90;assert(settings.F("aim.smooth")==90);
        settings.enable_override(item,false);assert(settings.F("aim.smooth")==60);
        settings.enable_override(category,false);assert(settings.F("aim.smooth")==20);
        settings.enable_override(category,true);
        {Settings::Scope foreign("Deadlock/hero/Dynamo");assert(settings.F("aim.smooth")==settings.m["aim.smooth"].f);}
        assert(settings.F("aim.smooth")==60); // parent chain restored by nested scope
        Settings::context="Counter-Strike 2/weapon-item/weapon_other";
        assert(settings.F("aim.smooth")==20); // direct context edit cannot borrow another item's parents
    }
    {
        Settings::Scope scoped(item,{"Deadlock/hero/Dynamo",category});
        settings.enable_override("Deadlock/hero/Dynamo",true);settings.overrides["Deadlock/hero/Dynamo"]["aim.smooth"].f=3;
        assert(settings.F("aim.smooth")==60); // no cross-game parent leakage
    }
    auto saved=settings.export_json();Settings restored;assert(restored.import_json(saved));
    {Settings::Scope scoped(item,{category});assert(restored.F("aim.smooth")==60);}
}

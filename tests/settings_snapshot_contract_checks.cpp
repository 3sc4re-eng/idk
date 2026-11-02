// Compiler-checked contract scenarios. No main; no target/device calls.
// Runtime thread assertions are authored for later execution, not run here.
#include "config.h"
#include <cassert>
#include <future>
#include <thread>

void settings_snapshot_contract_checks(){
    Settings a,b;a.deff("aim.smooth","Smooth",1,0,100);a.deff("aim.strength","Strength",2,0,100);b.deff("aim.smooth","Other",9,0,100);
    const auto first_revision=a.published_revision();
    {
        Settings::ReadScope stable(a);assert(a.F("aim.smooth")==1);
        std::thread writer([&]{Settings::WriteScope edit(a);a.find("aim.smooth")->f=20;a.find("aim.strength")->f=30;a.mark_dirty();});writer.join();
        assert(a.F("aim.smooth")==1&&a.F("aim.strength")==2);assert(b.F("aim.smooth")==9);
        {Settings::ReadScope nested(b);assert(b.F("aim.smooth")==9&&a.F("aim.smooth")==1);}
        assert(a.F("aim.smooth")==1);
    }
    assert(a.published_revision()>first_revision);{Settings::ReadScope fresh(a);assert(a.F("aim.smooth")==20&&a.F("aim.strength")==30);}
    // A reader can capture the last published image while a writer is editing.
    // It must not need the mutex or wait for the writer's release.
    std::promise<void> held,release;auto released=release.get_future();
    std::thread editing([&]{Settings::WriteScope edit(a);a.find("aim.smooth")->f=40;a.mark_dirty();held.set_value();released.wait();});
    held.get_future().wait();{Settings::ReadScope concurrent(a);assert(a.F("aim.smooth")==20);release.set_value();}editing.join();
    // Borrowed UI pointers remain valid until their transaction ends; structural
    // profile replacement cannot invalidate one halfway through a widget call.
    std::atomic<bool> attempted=false,finished=false;std::thread importer;
    {
        Settings::WriteScope edit(a);auto* borrowed=a.find("aim.smooth");
        importer=std::thread([&]{attempted=true;assert(a.import_json(json{{"aim.smooth",60},{"_motion_settings_version",2}}));finished=true;});
        while(!attempted.load())std::this_thread::yield();assert(!finished.load());borrowed->f=45;a.mark_dirty();
    }
    importer.join();assert(finished.load());
    // Value scopes are associated with the Settings instance, not global cfg.
    Settings copy=a;{Settings::ReadScope image(copy);assert(copy.F("aim.smooth")==60&&b.F("aim.smooth")==9);}
    Settings scoped;scoped.deff("aim.smooth","Smooth",1,0,100);
    {Settings::WriteScope edit(scoped);
        scoped.overrides["Example/Global"]["aim.smooth"]=scoped.m.at("aim.smooth");scoped.overrides["Example/Global"]["aim.smooth"].f=3;
        scoped.overrides["Example/weapon/Rifle"]["aim.smooth"]=scoped.m.at("aim.smooth");scoped.overrides["Example/weapon/Rifle"]["aim.smooth"].f=7;
        scoped.override_enabled["Example/weapon/Rifle"]=true;scoped.mark_dirty();
    }
    {Settings::ReadScope image(scoped);Settings::Scope context("Example/weapon-item/Carbine",{"Example/weapon/Rifle"});assert(scoped.F("aim.smooth")==7);}
}

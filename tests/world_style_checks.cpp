#include "world_style_config.h"
#include <cassert>
#include <string>
using namespace world_style_catalog;
static_assert(categories.size()==16&&world_labels::definitions.size()==20&&complete());
static_assert(index("C_Citadel_BreakableProp")==index("C_Citadel_BreakableProp_Hideout"));
static_assert(index("C_Citadel_BreakableProp")==index("C_BreakableProp"));
static_assert(index("C_Citadel_BreakableProp_PunchableTroll")!=index("C_Citadel_BreakableProp"));
static_assert(class_count(index("C_Citadel_BreakableProp"))==3);
static_assert(class_count(index("C_CitadelItemPickup"))==2);
static_assert(class_count(index("CCitadelItemPickupRejuv"))==2);
static_assert(index("C_Citadel_Pickup_Health")!=index("C_Citadel_Pickup_Gold"));
static_assert(index("C_Citadel_BreakableProp_GoldenStatue")==missing);
static_assert(index("CCitadel_XPOrbVData")==missing);
static_assert(index("Sinner")==missing&&index("HeavyBox")==missing);
constexpr bool category_master_and_elements(){
    using namespace world_style_config;Style s;
    if(!visible(s))return false;s.enabled=false;if(visible(s))return false;
    s.enabled=true;s.label=s.icon=s.distance=false;if(visible(s))return false;
    s.distance=true;if(!visible(s))return false;
    Styles values{};for(auto& value:values)value.enabled=false;
    if(any_visible(values))return false;values[3]=s;return any_visible(values);
}
static_assert(category_master_and_elements());

// Actual settings/import/export APIs, compiled only on the main PC. These are
// source fixtures, not a claim that runtime file or ImGui tests were executed.
void world_style_config_roundtrip_fixture(){
    Settings original;original.register_all();
    const auto health=index("C_Citadel_Pickup_Health"),gold=index("C_Citadel_Pickup_Gold");
    original.find("esp.color_world")->c={.1f,.8f,.3f,.7f};original.find("esp.draw_distance")->i=215;
    original.find("esp.distance")->b=false;original.find("esp.font_size")->f=19;
    auto inherited=world_style_config::resolve(original);assert(inherited[health].color.g==.8f&&inherited[health].draw_distance==215&&!inherited[health].distance&&inherited[health].text_size==19);
    const auto& p=world_style_config::keys(health);world_style_config::copy_shared(original,health);
    original.find(p.custom.c_str())->b=true;original.find(p.color.c_str())->c={.9f,.2f,.1f,1};
    original.find(p.icon.c_str())->b=false;original.find(p.label.c_str())->b=false;original.find(p.distance.c_str())->b=true;
    original.find(p.icon_size.c_str())->f=24;original.find(p.text_size.c_str())->f=23;original.find(p.draw_distance.c_str())->i=17;
    auto styles=world_style_config::resolve(original);assert(styles[health].draw_distance==17&&styles[gold].draw_distance==215);
    assert(styles[health].color.r==.9f&&styles[gold].color.r==.1f&&world_style_config::visible(styles[health]));
    const auto exported=original.export_json();assert(exported[p.custom]&&exported[p.draw_distance]==17);
    Settings loaded;loaded.register_all();assert(loaded.import_json(exported));
    auto restored=world_style_config::resolve(loaded);assert(restored[health].text_size==23&&restored[health].draw_distance==17&&!restored[health].icon&&!restored[health].label&&restored[health].distance);
    assert(restored[gold].draw_distance==215&&restored[gold].text_size==19);
    loaded.find(p.enabled.c_str())->b=false;assert(!world_style_config::visible(world_style_config::resolve(loaded)[health]));
    loaded.find(p.custom.c_str())->b=false;assert(world_style_config::resolve(loaded)[health].draw_distance==215);
    // Existing game-profile isolation copies Settings. Editing another profile
    // must not alter this profile's registered category descriptors.
    Settings other=loaded;other.find(p.draw_distance.c_str())->i=399;assert(loaded.I(p.draw_distance.c_str())==17);
}

// Imported custom styles may predate the hidden initialization bookkeeping.
// The first visible interaction can turn inheritance on; preserve the imported
// appearance before that checkbox changes, so the next custom edit keeps it.
void world_style_first_import_toggle_fixture(){
    Settings imported;imported.register_all();
    const auto health=index("C_Citadel_Pickup_Health");const auto& p=world_style_config::keys(health);
    auto payload=imported.export_json();payload[p.custom]=true;payload.erase(p.initialized);
    payload[p.color]=json::array({.9f,.15f,.25f,.6f});payload[p.draw_distance]=37;payload[p.icon]=false;
    assert(imported.import_json(payload));assert(!imported.B(p.initialized.c_str()));
    world_style_config::prepare_edit(imported,health);
    assert(imported.B(p.initialized.c_str()));
    imported.find(p.custom.c_str())->b=false;
    imported.find("esp.draw_distance")->i=222;
    assert(world_style_config::resolve(imported)[health].draw_distance==222);
    // Follow the real rising-edge branch after an off/on UI transition.
    const bool was_custom=imported.B(p.custom.c_str());
    world_style_config::prepare_edit(imported,health);
    imported.find(p.custom.c_str())->b=true;
    if(!was_custom&&imported.B(p.custom.c_str())&&!imported.B(p.initialized.c_str()))world_style_config::copy_shared(imported,health);
    const auto restored=world_style_config::resolve(imported)[health];
    assert(restored.draw_distance==37&&restored.color.r==.9f&&restored.color.a==.6f&&!restored.icon);
    // Repeated edit preparation does not mutate or reseed an initialized style.
    imported.dirty=false;world_style_config::prepare_edit(imported,health);assert(!imported.dirty);
}

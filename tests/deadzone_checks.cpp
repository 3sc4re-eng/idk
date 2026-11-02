// SOURCE ONLY: syntax-checked, not built/run as an executable.
#include "aim_model.h"
#include <cassert>
namespace deadzone_proposal_checks {
void full_cone_geometry(){
    const float width=1920,projection=1.4f,angle=1.08f;
    const float acquisition=aim_model::fov_radius(width,angle,projection);
    const float zone=aim_model::deadzone_radius_pixels(width,angle,projection);
    assert(std::fabs(acquisition-zone)<1e-5f);
    assert(std::fabs(zone-width*.5f*projection*std::tan(deg2rad(.54f)))<1e-5f);
    assert(aim_model::deadzone_radius_pixels(width,0,projection)==0);
    assert(aim_model::inside_deadzone({960,540},{960,540},zone));
    assert(aim_model::inside_deadzone({970,540},{960,540},10));
    assert(!aim_model::inside_deadzone({962+zone,540},{960,540},zone));
    // Handoff is determined by the measured region, not a lead intercept.
    const Vec2 measured{960+zone*.5f,540},lead{960+zone*2,540};
    assert(aim_model::inside_deadzone(measured,{960,540},zone));
    assert(!aim_model::inside_deadzone(lead,{960,540},zone));
}
void profile_migration(){
    json angular={{"aim.deadzone_units",1},{"aim.deadzone_degrees",.54f}};Settings::migrate_deadzone(angular);assert(std::fabs(angular["aim.deadzone_fov"].get<float>()-1.08f)<1e-6f);
    json explicit_new={{"aim.deadzone_fov",2.5f},{"aim.deadzone_units",1},{"aim.deadzone_degrees",.54f}};Settings::migrate_deadzone(explicit_new);assert(explicit_new["aim.deadzone_fov"]==2.5f);
    json old_pixels={{"aim.deadzone",9},{"trigger.magnet_deadzone",3}};Settings::migrate_deadzone(old_pixels);assert(old_pixels["aim.deadzone_fov"]==1.08f&&old_pixels["trigger.magnet_deadzone_fov"]==.36f);assert(old_pixels["_deadzone_migration"]["aim"]=="legacy_pixels_default_without_reference");
    json zero={{"aim.deadzone",0},{"trigger.magnet_deadzone",0}};Settings::migrate_deadzone(zero);assert(zero["aim.deadzone_fov"]==0&&zero["trigger.magnet_deadzone_fov"]==0);
    json reference={{"aim.deadzone",9},{"_deadzone_reference_width",1920},{"_deadzone_reference_projection",1.4f}};Settings::migrate_deadzone(reference);const float migrated=reference["aim.deadzone_fov"].get<float>();assert(std::fabs(aim_model::deadzone_radius_pixels(1920,migrated,1.4f)-9)<1e-4f);
    json width_only={{"aim.deadzone",9},{"_deadzone_reference_width",1920}};Settings::migrate_deadzone(width_only);assert(width_only["aim.deadzone_fov"]==1.08f); // width does not establish projection/zoom
    json inherited={{"aim.deadzone_degrees",.25f}};Settings::migrate_deadzone(inherited,&angular);assert(inherited["aim.deadzone_fov"]==.5f);
    json empty=json::object();Settings::migrate_deadzone(empty,&angular);assert(!empty.contains("aim.deadzone_fov")); // preserve override inheritance
    Settings cfg;cfg.register_all();json imported={{"_motion_settings_version",2},{"aim.deadzone_units",1},{"aim.deadzone_degrees",.6f},{"_overrides",{{"Deadlock/hero/Example",{{"aim.deadzone",0},{"aim.deadzone_units",0}}}}}};assert(cfg.import_json(imported));assert(std::fabs(cfg.F("aim.deadzone_fov")-1.2f)<1e-6f);assert(cfg.overrides.at("Deadlock/hero/Example").at("aim.deadzone_fov").f==0);assert(cfg.export_json()["_deadzone_settings_version"]==2);
}
}

static_assert(aim_model::capped_deadzone_fov(8,4)==4);
static_assert(aim_model::capped_deadzone_fov(2,10)==2);
static_assert(aim_model::capped_deadzone_fov(20,2)==2);
static_assert(aim_model::capped_deadzone_fov(-1,4)==0);
static_assert(aim_model::capped_deadzone_fov(4,0)==0);

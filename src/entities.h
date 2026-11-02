#pragma once
#include "utils.h"
#include "action_pause_policy.h"
#include "prediction.h"
#include "profile_subject.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <array>
#include <string>
enum class EClass{ Player=0, Deployable, NPC, World, Projectile };
struct Ability{
    prediction::Shot shot;
    u64 base=0;u32 handle=0,token=0;int16_t slot=-1;int charges=0;
    bool valid=false,ready=false,casting=false,channeling=false,trained=false;
    float cooldown_end=0,cast_start=0,channel_start=0,completed=0,impact_time=0;
    u32 upgrade_bits=0;std::vector<u32> imbued_tokens;bool imbued_valid=false;bool active_reload_failed=false;int melee_state=0,melee_type=0;float melee_state_start=0;bool melee_valid=false;std::vector<u32> targets;std::string type;
    Vec3 impact_position{};bool has_impact_position=false;
};
struct Entity{
    profile_subject::Subject settings_subject;std::string display_name;
    // ACE_NATIVE_JOINT_INTEGRATION_V1: independently validated native WORLD joints.
    struct NativeJoints{std::array<Vec3,2> world{};std::array<int,2> model_index{-1,-1};u64 binding_epoch=0,started_ms=0,completed_ms=0;unsigned valid_mask=0;} native_joints;
    prediction::Shot primary_shot,current_shot;u32 selected_ability=0;
    u64 base=0; u32 idx=0, team=0, hero=0;
    u64 host_sample_ms=0; // Original actor/header acquisition; never refreshed by later joint work.
    EClass cls=EClass::Player;
    Vec3 pos, vel; float hp=0, max_hp=0; bool alive=false;
    // valid is identity/pose; health fields and alive knowledge are independent.
    bool health_current_valid=false,health_max_valid=false,health_pair_valid=false,life_state_valid=false,alive_valid=false;
    bool visible=false, execute_eligible=false, stealthed=false, invuln=false;
    float soul_diff=0; u64 souls=0; u64 steam_id=0;
    float windup_pct=0; bool melee_windup=false; bool ult_ready=false; bool casting=false;
    u64 visible_since=0,last_seen=0,owner=0;u32 handle=0,ability_token=0;
    bool disarmed=false,reloading=true,weapon_valid=false,active_reload_allowed=false;float reload_begin=0,reload_end=0,reload_pause=0,reload_fraction=0,reload_width=0;std::vector<u32> removable_debuffs;
    bool world_obstacle=false;float scale=1;bool bullet_invuln=false,team_visible=false,occlusion_valid=false;std::array<bool,7> bone_visible{},bone_visibility_known{};
    bool public_ultimate_valid=false,public_ultimate_trained=false;float public_ultimate_cooldown_start=0,public_ultimate_cooldown_end=0,public_ultimate_stopped_remaining=0;u64 public_ultimate_sample_ms=0;
    bool charging=false;bool valid=false,visibility_known=false,abilities_valid=false,modifiers_valid=false,ability_blocked=false;
    Vec3 bounds_min{-16,-16,0},bounds_max{16,16,72},facing{};bool bounds_valid=false;float collision_radius=0;std::string type_name;std::vector<Ability> abilities;
};
struct Snapshots{
    std::atomic<bool> backend_managed{false},overlay_menu{false},frame_valid{false},action_valid{false},input_valid{false},visibility_valid{false};
    std::array<bool,256> game_keys{};float game_time=0;std::string backend_status="not attached";
    unsigned entity_count=0;Vec3 camera{};bool camera_valid=false;float sensitivity=0,fov_sensitivity=1;std::string game_id;u32 game_revision=0;std::atomic<float> units_per_metre{0};std::string map_name,map_status="Automatic map detection has not sampled yet";Entity local;
    std::vector<Entity> enemies, friendlies, npcs, world, projectiles;
    float view[16]={};
    std::atomic<bool> in_match{false};
    std::atomic<bool> dead{true};
    std::atomic<bool> local_alive_valid{false};
    // Legacy in_menu is the raw engine UI mask candidate, not proven Escape visibility.
    std::atomic<bool> in_menu{false};
    std::atomic<unsigned> raw_ui_flags{0},raw_input_enabled{0};
    std::atomic<bool> raw_ui_valid{false},raw_input_enabled_valid{false};
    std::atomic<bool> in_scoreboard{false};
    std::atomic<bool> spectating{false};
    std::atomic<u64> last_read_ms{0};
    std::atomic<u64> native_binding_epoch{0};
    // Matrix timestamp records host acquisition, not proof that the
    // target updated the matrix. change_ms is diagnostic only.
    std::atomic<u64> matrix_sample_ms{0},matrix_change_ms{0};
    std::atomic<int> read_hz{0};std::atomic<float> read_hz_exact{0};
    std::atomic<u64> poll_ms{0},publish_ms{0},scan_ms{0},maintenance_ms{0},wait_ms{0},iteration_ms{0};
    std::atomic<u64> discovery_completed_ms{0},discovery_interval_ms{0};
    std::atomic<unsigned> discovery_pages_read{0},discovery_pages_unavailable{0},discovery_accepted{0},discovery_pawns{0},discovery_controllers{0};
};
extern Snapshots snap;
extern std::mutex snap_mtx;
const char* hero_name_by_id(u32 id);
void request_entity_rescan();void request_recache();std::string recache_status();
bool consume_attach_recovery();bool entity_reader_active();
void entity_thread_start();void entity_thread_stop();
bool game_key_state(int virtual_key,bool& held);bool game_key_down(int virtual_key);
action_pause_policy::Preferences configured_action_pause_preferences();
bool game_ui_pause_active();bool ace_menu_pause_active();bool game_data_permits_output();
std::string backend_status();std::string game_output_reason();std::string map_detection_status();
Vec2 world_to_screen(Vec3 w,u64 sw,u64 sh);

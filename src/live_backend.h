#pragma once
#include "entities.h"
#include "health_state.h"
#include "memory_batch.h"
#include "scan_incremental.h"
#include "engine_map_reader.h"
#include "native_joint_backend.h" // ACE_NATIVE_JOINT_INTEGRATION_V1
#include <windows.h>
#include <functional>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <memory>
#include <map>
namespace live_backend{
using Read=std::function<bool(u64,void*,u32)>;
struct Binding{u64 client=0,engine=0,input=0;};
struct DiscoveryDiagnostics{
    u64 attempted_ms=0,completed_ms=0,elapsed_ms=0,interval_ms=0;
    unsigned chunks=0,invalid_chunks=0,pages_read=0,pages_unavailable=0,records_read=0;
    unsigned empty_records=0,flags_rejected=0,invalid_addresses=0,serial_mismatches=0,candidates=0,object_unreadable=0,vtable_outside_client=0,unknown_classes=0,known_classes=0,identity_mismatches=0;
    unsigned accepted=0,pawns=0,controllers=0;
    u64 first_unavailable_page=0,first_unknown_vtable=0;
    std::string status="Entity discovery has not run";
};
struct Frame{
    Entity local;std::vector<Entity> enemies,friendlies,npcs,world,projectiles;
    u64 native_binding_epoch=0;native_joint_backend::Stats native_joint_performance;
    std::array<float,16> view{};std::array<bool,256> keys{};
    u32 game_revision=0;float world_units_per_metre=0;bool actions_supported=false;std::string game_id,map_name,map_status="Automatic map detection is waiting for engine state";bool clock_valid=false;float sensitivity=0,fov_sensitivity=1;
    bool attached=false,in_match=false,dead=true,menu=false,scoreboard=false,spectating=false;
    bool input_valid=false,ui_valid=false,visibility_valid=false,matrix_valid=false;float game_time=0;
    u64 time=0,matrix_sample_ms=0,matrix_change_ms=0,scan_ms=0;std::string status;unsigned read_failures=0,entities=0;DiscoveryDiagnostics discovery;
    struct Performance {std::array<u64,8> phase_ms{};u64 scatter_calls=0,scatter_requests=0,direct_reads=0,cache_hits=0,failed_ranges=0;unsigned auxiliary_sampled=0,ability_players=0,ability_players_skipped=0;std::string local_rejection;std::int32_t local_health=0,local_base_max=0,local_global_max=0,local_global_health=0;u32 local_health_handle=0;unsigned local_life=0,raw_ui_flags=0,raw_input_enabled=0;int raw_game_state=0,raw_paused=-1;bool local_global_readable=false,local_health_current_valid=false,local_health_max_valid=false,local_alive_valid=false,local_health_pair_valid=false,raw_ui_valid=false,raw_input_enabled_valid=false,raw_state_valid=false,raw_pause_valid=false;health_state::Source local_health_source=health_state::Source::Unavailable;health_state::Reason local_health_reason=health_state::Reason::CurrentUnreadable;} performance;
    bool visuals_ready()const{return attached&&!game_id.empty()&&std::isfinite(world_units_per_metre)&&world_units_per_metre>0&&matrix_valid&&local.valid;}
    bool actions_ready()const{return actions_supported&&visuals_ready()&&ui_valid&&clock_valid&&input_valid;}
};
class Client{
    mutable std::recursive_mutex mutex_;Read read_;BatchRead batch_;Binding binding_;std::unordered_map<u32,const void*> classes_;
    struct Identity{u64 entity=0;u32 handle=0;int kind=0;std::string name;u64 identity=0;};
    std::unordered_map<u32,Identity> identities_;std::vector<Entity> cached_world_;u64 scan_time_=0,world_time_=0,last_clock_change_=0;int last_frame_=-1;std::array<int,256> key_codes_{};bool keymap_ready_=false;u64 keymap_retry_=0;
    DiscoveryDiagnostics discovery_;u64 scan_report_time_=0;std::string scan_report_state_;
    std::array<float,16> last_view_{};u64 last_view_change_=0;bool have_last_view_=false;
    memory_batch::FrameCache frame_cache_;bool cache_active_=false;Frame::Performance performance_;u64 performance_log_=0,local_base_=0;std::size_t auxiliary_cursor_=0;
    std::unique_ptr<incremental_discovery::Engine> discovery_engine_;
    u64 discovery_identity_revision_=0,identity_sync_updates_=0,identity_sync_skips_=0;
    std::unique_ptr<native_joint_backend::Bank> native_joint_bank_;u64 native_joint_log_=0;
    void sample_native_joints(Frame&);
    struct DefinitionCache {u64 vdata=0,loaded_ms=0,nodes=0;u32 capacity=0;std::map<std::pair<u64,u32>,std::vector<unsigned char>> blocks;};
    std::unordered_map<u32,DefinitionCache> definitions_;
    struct HelperDetails {bool automatic=false,active_reload=false,defensive=false;};
    HelperDetails helper_details_;
    prediction::Shot shot(u32 handle,u64 vdata,u64 modifiers,int tick);
    void queue(u64 a,u32 n){frame_cache_.queue(a,n);}void flush(unsigned phase);
    void prefetch_roots(const std::vector<const Identity*>&);void prefetch_dependencies(const std::vector<const Identity*>&);
    void prefetch_abilities(const std::vector<const Identity*>&);void prefetch_ability_details(const std::vector<const Identity*>&);
    bool fresh(u64 a,void* p,u32 n){++performance_.direct_reads;return read_(a,p,n);}
    engine_map::Monitor map_monitor_;bool map_layout_ready_=false;
    std::string map_layout_error_="Map layout has not been validated",detected_map_,map_resource_;
    u64 map_client_=0;u32 map_spawn_=0;float map_clock_=0;
    bool world_display_requested_=false;
    // Distinct class/subclass evidence from fields already acquired in roots.
    // Limited per binding; no model-pointer reads or speculative variant names.
    std::unordered_set<std::string> world_label_observations_;
    u64 world_label_window_ms_=0;unsigned world_label_window_count_=0;
    std::string map_probe_stage_;u64 map_probe_report_ms_=0;
    void detect_map(Frame&);
    void load_keymap(Binding binding,bool announce);
    bool get(u64 address,void* data,u32 size);
    template<class T>bool get(u64 address,T& value){return get(address,&value,sizeof(value));}
    bool scan();u64 resolve(u32 handle)const;bool entity(const Identity&,Entity&,const health_state::PlayerMaximum* maximum=nullptr);void abilities(Entity&,float,bool,bool=false);
public:
    explicit Client(Read read,BatchRead batch={}):read_(std::move(read)),batch_(std::move(batch)){}
    void recache();
    bool bind(Binding binding,std::string& error,bool* metadata_failure=nullptr);void reset();Frame poll(int scoreboard_key=VK_TAB);
    // Reader-owner calls AFTER publishing the preceding fast frame. One bounded
    // discovery step; returned time is blocking work, not total cycle duration.
    u64 maintenance();
    Binding binding()const{std::lock_guard<std::recursive_mutex> lock(mutex_);return binding_;}
};
extern Client live;
bool valid_pointer(u64 address);bool matrix_valid(const float* matrix);
}

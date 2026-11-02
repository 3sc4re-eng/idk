#include "entities.h"
#include "dma.h"
#include "hero_catalog.h"
#include "live_backend.h"
#include "offsets.h"
#include "config.h"
#include "input.h"
#include "collision.h"
#include "visibility_cache.h"
#include "target_geometry.h"
#include "native_target.h"
#include "profile_scope.h"
#include "world_units.h"
#include "game.h"
// ACE_NATIVE_JOINT_INTEGRATION_V1 (existing admission loop remains unchanged)
#include <algorithm>
#include <windows.h>
#include <thread>
#include <chrono>
Snapshots snap;std::mutex snap_mtx;
static std::atomic<bool> refresh_requested{false},rescan_requested{false};static std::mutex refresh_mutex;static std::string refresh_message="Game data has not been manually refreshed";
void request_entity_rescan(){snap.frame_valid=false;rescan_requested=true;std::lock_guard<std::mutex> lock(refresh_mutex);refresh_message=off.resolved?"Entity rescan requested":"Entity rescan queued until game attaches";plat_log("Recovery: %s",refresh_message.c_str());}
void request_recache(){snap.frame_valid=false;refresh_requested=true;std::lock_guard<std::mutex> lock(refresh_mutex);refresh_message=off.resolved?"Memory cache refresh requested":"Memory refresh queued until game attaches";plat_log("Recovery: %s",refresh_message.c_str());}
std::string recache_status(){std::lock_guard<std::mutex> lock(refresh_mutex);return refresh_message;}
static std::atomic<bool> reader_running{false};static std::thread reader_thread;
bool entity_reader_active(){return reader_running.load();}
bool consume_attach_recovery(){
    // Only the DMA owner calls this after joining the reader. Keep queued commands
    // available until the device is ready, even when attachment never succeeded.
    if(!mem.ready()||off.resolved||entity_reader_active())return false;
    bool refresh=refresh_requested.exchange(false),rescan=rescan_requested.exchange(false);if(!refresh&&!rescan)return false;
    plat_stage("attach","processing queued recovery");bool refreshed=!refresh||mem.refresh_caches();live_backend::live.recache();
    std::string message=refresh?(refreshed?"Memory caches refreshed; retrying game attachment":"Cache refresh unavailable; retrying game attachment"):"Retrying process and module discovery";
    {std::lock_guard<std::mutex> lock(refresh_mutex);refresh_message=message;}plat_log("Recovery: %s",message.c_str());return true;
}

const char* hero_name_by_id(u32 id){return current_hero_name(id);}
bool game_key_state(int key,bool& held){
    held=false;if(key<1||key>255)return false;
    if(!snap.backend_managed){held=(GetAsyncKeyState(key)&0x8000)!=0;return true;}
    if(key==VK_LBUTTON||key==VK_RBUTTON||key==VK_MBUTTON||key==VK_XBUTTON1||key==VK_XBUTTON2)return hw::physical_mouse_state(key,held);
    std::lock_guard<std::mutex> lock(snap_mtx);if(!snap.input_valid||now_ms()-snap.last_read_ms.load()>250)return false;held=snap.game_keys[key];return true;
}
bool game_key_down(int key){bool held=false;return game_key_state(key,held)&&held;}
// Historical helper name retained for callers; this is only an explicit diagnostic pause.
bool game_ui_pause_active(){return action_pause_policy::diagnostic_pause(cfg.B(action_pause_policy::diagnostic_key),snap.raw_ui_valid.load(),snap.raw_ui_flags.load());}
bool ace_menu_pause_active(){return action_pause_policy::ace_pause(cfg.B("input.disable_in_overlay"),snap.overlay_menu.load());}
action_pause_policy::Preferences configured_action_pause_preferences(){
    action_pause_policy::Preferences p;
    p.pause_in_ace_menu=cfg.B("input.disable_in_overlay");
    p.pause_when_dead=cfg.B("input.disable_when_dead");
    p.pause_in_scoreboard=cfg.B("input.disable_in_scoreboard");
    p.pause_when_spectator=cfg.B("input.disable_when_spectator");
    p.diagnostic_raw_ui=cfg.B(action_pause_policy::diagnostic_key);
    return p;
}
static action_pause_policy::Reason current_action_gate(){
    action_pause_policy::Observations s;
    const auto at=snap.last_read_ms.load(),now=now_ms();
    s.managed=snap.backend_managed;s.snapshot_valid=snap.frame_valid&&snap.action_valid;s.input_valid=snap.input_valid;
    s.fresh=at&&now>=at&&now-at<=250;s.in_match=snap.in_match;
    s.life_known=snap.local_alive_valid;s.dead=snap.dead;
    s.scoreboard=snap.in_scoreboard;s.spectator=snap.spectating;
    s.raw_ui_readable=snap.raw_ui_valid;s.raw_ui_flags=snap.raw_ui_flags;
    s.ace_menu_open=snap.overlay_menu;
    return action_pause_policy::evaluate(configured_action_pause_preferences(),s);
}
bool game_data_permits_output(){return current_action_gate()==action_pause_policy::Reason::Ready;}
std::string game_output_reason(){
    const auto reason=current_action_gate();
    if(!snap.backend_managed&&reason==action_pause_policy::Reason::Ready)return "standalone";
    return action_pause_policy::reason_text(reason);
}
std::string backend_status(){std::lock_guard<std::mutex> lock(snap_mtx);return snap.backend_status;}
std::string map_detection_status(){std::lock_guard<std::mutex> lock(snap_mtx);return snap.map_status;}
static void publish(live_backend::Frame&& f){
    // Never publish a completed old-binding acquisition into a replacement.
    if(f.native_binding_epoch&&f.native_binding_epoch!=mem.attachment_epoch()){
        live_backend::Frame empty;empty.time=f.time;empty.status="Attachment changed before publication";f=std::move(empty);
    }
    if(f.attached&&(!games::selected||!games::selected->cfg_prefix||f.game_id!=games::selected->cfg_prefix)){
        const auto sampled=f.time;f={};f.time=sampled;f.status="Backend game identity does not match the selected game";
    }
    world_units::set_scale(f.attached?f.world_units_per_metre:0);
    const bool visuals_ready=f.visuals_ready();
    std::string map=f.game_id=="deadlock"?(cfg.I("system.map")==1?"dl_midtown":(cfg.I("system.map")==2?"hero_testing":f.map_name)):f.map_name;{std::lock_guard<std::mutex> lock(snap_mtx);if(snap.map_name!=map)snap.frame_valid=false;}collision::select_map(collision::GameMapKey{f.game_id,map,f.game_revision});
    std::vector<collision::Obstacle> obstacles;for(const auto& entity:f.world)if(entity.world_obstacle)obstacles.push_back({entity.pos,entity.bounds_min,entity.bounds_max,entity.facing,entity.scale});collision::set_obstacles(std::move(obstacles));
    Vec3 camera;bool camera_valid=f.matrix_valid&&collision::camera_origin(f.view.data(),camera);
    static std::map<u32,std::pair<u64,u64>> visible_times;
    for(auto it=visible_times.begin();it!=visible_times.end();)if(f.time-it->second.second>500)it=visible_times.erase(it);else ++it;
    const auto collision_revision=collision::revision();
    auto ray_visibility=[&](Vec3 point){
        visibility_cache::Answer answer;if(!camera_valid||!visibility_cache::same(collision::revision(),collision_revision))return answer;
        const auto ray=collision::trace_any(camera,point,collision::Vision|collision::Bullet);
        if(visibility_cache::same(collision::revision(),collision_revision))answer={ray.known,ray.clear};
        return answer;
    };
    auto visibility=[&](std::vector<Entity>& targets,bool allied,bool npc){for(auto& e:targets){
        const bool team_known=npc||allied||e.visibility_known;e.team_visible=npc||allied?true:e.visible;e.bone_visible.fill(false);e.bone_visibility_known.fill(false);
        // Team FOW can reject a target. A positive FOW flag never proves local LOS.
        if(!camera_valid||!team_known||!e.team_visible){
            e.occlusion_valid=false;e.visible=false;e.visibility_known=team_known&&!e.team_visible;visible_times.erase(e.handle);continue;
        }
        bool any=false,on_screen=false,any_known=false;
        for(int i=0;i<7;i++){
            if(!native_target::supports_region(i))continue;
            Vec3 point;if(!native_target::point(e,i,point,f.native_binding_epoch,mem.attachment_epoch(),now_ms()))continue;
            float w=point.x*f.view[12]+point.y*f.view[13]+point.z*f.view[14]+f.view[15];
            if(std::isfinite(w)&&w>.01f){float x=(point.x*f.view[0]+point.y*f.view[1]+point.z*f.view[2]+f.view[3])/w,y=(point.x*f.view[4]+point.y*f.view[5]+point.z*f.view[6]+f.view[7])/w;on_screen|=std::isfinite(x)&&std::isfinite(y)&&std::fabs(x)<=1&&std::fabs(y)<=1;}
            // Offscreen is not an occluder. Helpers may need LOS behind the view.
            auto ray=ray_visibility(point);e.bone_visibility_known[i]=ray.known;any_known|=ray.known;e.bone_visible[i]=ray.known&&ray.clear;any|=e.bone_visible[i];
        }
        e.occlusion_valid=any_known;e.visible=e.team_visible&&any;e.visibility_known=team_known&&any_known;
        if(e.visible&&on_screen){auto& times=visible_times[e.handle];if(!times.first||f.time-times.second>250)times.first=f.time;times.second=f.time;e.visible_since=times.first;}else visible_times.erase(e.handle);
    }};visibility(f.enemies,false,false);visibility(f.friendlies,true,false);
    {profile_scope::ActiveScope hero_settings(f.local);if(native_target::supports_entity(EClass::NPC)&&(cfg.B("aim.target_minions")||cfg.B("aim.target_neutrals")))visibility(f.npcs,false,true);}
    if(!visibility_cache::same(collision::revision(),collision_revision)){
        auto invalidate=[&](std::vector<Entity>& values){for(auto& e:values){e.visible=e.visibility_known=e.occlusion_valid=false;e.bone_visible.fill(false);e.bone_visibility_known.fill(false);visible_times.erase(e.handle);}};
        invalidate(f.enemies);invalidate(f.friendlies);invalidate(f.npcs);
    }
    std::lock_guard<std::mutex> lock(snap_mtx);
    if(f.native_binding_epoch&&f.native_binding_epoch!=mem.attachment_epoch()){snap.native_binding_epoch=0;snap.frame_valid=false;snap.backend_status="Attachment changed before publication";return;}
    snap.game_id=f.game_id;snap.game_revision=f.game_revision;snap.units_per_metre=f.world_units_per_metre;snap.action_valid=f.actions_ready();snap.camera=camera;snap.camera_valid=camera_valid;snap.map_name=map;snap.map_status=f.map_status;snap.sensitivity=f.sensitivity;snap.fov_sensitivity=f.fov_sensitivity;snap.local=std::move(f.local);snap.enemies=std::move(f.enemies);snap.friendlies=std::move(f.friendlies);
    snap.npcs=std::move(f.npcs);snap.world=std::move(f.world);snap.projectiles=std::move(f.projectiles);
    std::copy(f.view.begin(),f.view.end(),snap.view);snap.native_binding_epoch=f.native_binding_epoch;snap.matrix_sample_ms=f.matrix_sample_ms;snap.matrix_change_ms=f.matrix_change_ms;snap.game_keys=f.keys;snap.input_valid=f.input_valid;snap.visibility_valid=f.visibility_valid;
    snap.game_time=f.game_time;snap.backend_status=f.status;snap.entity_count=f.entities;
    snap.discovery_completed_ms=f.discovery.completed_ms;snap.discovery_interval_ms=f.discovery.interval_ms;snap.discovery_pages_read=f.discovery.pages_read;snap.discovery_pages_unavailable=f.discovery.pages_unavailable;snap.discovery_accepted=f.discovery.accepted;snap.discovery_pawns=f.discovery.pawns;snap.discovery_controllers=f.discovery.controllers;
    snap.in_match=f.in_match;snap.local_alive_valid=snap.local.alive_valid;snap.dead=f.dead;snap.in_menu=f.menu;snap.raw_ui_flags=f.performance.raw_ui_flags;snap.raw_ui_valid=f.ui_valid;snap.raw_input_enabled=f.performance.raw_input_enabled;snap.raw_input_enabled_valid=f.performance.raw_input_enabled_valid;snap.in_scoreboard=f.scoreboard;snap.spectating=f.spectating;
    snap.last_read_ms=f.time;snap.frame_valid=visuals_ready; // Input validity gates actions separately, not visual data.
}
void entity_thread_start(){
    if(reader_running.exchange(true))return;
    world_units::set_scale(0);snap.action_valid=false;snap.read_hz=0;snap.read_hz_exact=0;snap.poll_ms=0;snap.publish_ms=0;snap.scan_ms=0;snap.discovery_completed_ms=0;snap.discovery_interval_ms=0;snap.discovery_pages_read=0;snap.discovery_pages_unavailable=0;snap.discovery_accepted=0;snap.discovery_pawns=0;snap.discovery_controllers=0;
    if(reader_thread.joinable())reader_thread.join();snap.backend_managed=true;
    plat_log("Reader settings: limit=%d Hz, discovery=%d ms, world=%d ms",cfg.I("reader.max_hz"),cfg.I("reader.discovery_ms"),cfg.I("reader.world_ms"));    plat_stage("reader","starting");reader_thread=std::thread([]{u64 rate_start=now_ms();unsigned rate_count=0;u64 previous_start=0,cadence_sum=0,cadence_count=0,cadence_min=UINT64_MAX,cadence_max=0;std::string previous_gate_report;u64 gate_reported_at=0;std::array<u64,5> stage_sums{},stage_maxima{};u64 completed_cycles=0;while(reader_running){Settings::ReadScope reader_settings(cfg);const auto iteration_started=std::chrono::steady_clock::now();auto start=now_ms();if(previous_start){const auto period=start-previous_start;snap.iteration_ms=period;cadence_sum+=period;++cadence_count;cadence_min=std::min(cadence_min,period);cadence_max=std::max(cadence_max,period);}previous_start=start;live_backend::Frame frame;
        if(rescan_requested.exchange(false)){plat_stage("reader","rescanning identities",false);live_backend::live.recache();}
        if(refresh_requested.exchange(false)){plat_stage("reader","refreshing memory caches",false);bool refreshed=mem.refresh_caches();live_backend::live.recache();std::lock_guard<std::mutex> lock(refresh_mutex);refresh_message=refreshed?"Memory caches refreshed; rebuilding entity data":"Memory refresh unavailable; rebuilding entity data";}
        plat_stage("reader","validating layout",false);static u64 checked=0;if(off.resolved&&now_ms()-checked>1000){checked=now_ms();if(!off.validate())off.invalidate("Live layout validation lost; waiting for game reattachment");}
        if(off.resolved){plat_stage("reader","reading game snapshot",false);frame=live_backend::live.poll(cfg.I("system.scoreboard_key"));}else{frame.time=now_ms();frame.status=off.status_text();}
        snap.poll_ms=now_ms()-start;
        static std::string previous_status;std::string state=frame.status+" | local="+std::to_string(frame.local.valid)+" matrix="+std::to_string(frame.matrix_valid)+" clock="+std::to_string(frame.clock_valid)+" keys="+std::to_string(frame.input_valid)+" visibility="+std::to_string(frame.visibility_valid);
        if(state!=previous_status){plat_log("Snapshot state: %s | identities=%u enemies=%u allies=%u world=%u",state.c_str(),frame.entities,(unsigned)frame.enemies.size(),(unsigned)frame.friendlies.size(),(unsigned)frame.world.size());previous_status=state;}
        plat_stage("reader","publishing snapshot and collision",false);if(frame.scan_ms)snap.scan_ms=frame.scan_ms;u64 publish_start=now_ms();publish(std::move(frame));snap.publish_ms=now_ms()-publish_start;rate_count++;u64 rate_elapsed=now_ms()-rate_start;if(rate_elapsed>=1000){snap.read_hz_exact=rate_count*1000.f/rate_elapsed;snap.read_hz=(int)snap.read_hz_exact.load();rate_start=now_ms();rate_count=0;}
        char gate_line[768];const auto ui=snap.raw_ui_flags.load(),input=snap.raw_input_enabled.load();
        snprintf(gate_line,sizeof(gate_line),"Action gate: reason=%s | ui_byte=0x%02X ui_readable=%d bit0=%u bit1=%u raw_signal=%d input_enable_byte=0x%02X input_readable=%d pause_raw_ui_diagnostic=%d ACE_menu_open=%d pause_ACE_menu=%d dead=%d life_known=%d pause_dead=%d scoreboard=%d pause_scoreboard=%d spectator=%d pause_spectator=%d",game_output_reason().c_str(),ui,snap.raw_ui_valid.load(),ui&1,(ui>>1)&1,action_pause_policy::raw_signal_active(snap.raw_ui_valid.load(),ui),input,snap.raw_input_enabled_valid.load(),cfg.B(action_pause_policy::diagnostic_key),snap.overlay_menu.load(),cfg.B("input.disable_in_overlay"),snap.dead.load(),snap.local_alive_valid.load(),cfg.B("input.disable_when_dead"),snap.in_scoreboard.load(),cfg.B("input.disable_in_scoreboard"),snap.spectating.load(),cfg.B("input.disable_when_spectator"));
        const auto gate_now=now_ms();if(previous_gate_report!=gate_line&&(!gate_reported_at||gate_now-gate_reported_at>=500)){plat_log("%s",gate_line);previous_gate_report=gate_line;gate_reported_at=gate_now;}
        static u64 last_timing_log=0;if(now_ms()-last_timing_log>=10000){last_timing_log=now_ms();
            if(completed_cycles){plat_log("Reader loop budget: cycles=%llu average_ms poll=%.2f publish=%.2f maintenance=%.2f wait=%.2f other=%.2f | maximum_ms poll=%llu publish=%llu maintenance=%llu wait=%llu other=%llu",(unsigned long long)completed_cycles,(double)stage_sums[0]/completed_cycles,(double)stage_sums[1]/completed_cycles,(double)stage_sums[2]/completed_cycles,(double)stage_sums[3]/completed_cycles,(double)stage_sums[4]/completed_cycles,(unsigned long long)stage_maxima[0],(unsigned long long)stage_maxima[1],(unsigned long long)stage_maxima[2],(unsigned long long)stage_maxima[3],(unsigned long long)stage_maxima[4]);stage_sums.fill(0);stage_maxima.fill(0);completed_cycles=0;}
            // Host request counters only. VMM may fetch more physical bytes per
            // request; this does not claim measured PCIe bandwidth/utilization.
            static u64 io_at=0,io_bytes=0,io_scalar=0,io_batches=0,io_requests=0,io_requested_bytes=0;
            const u64 at=now_ms(),bytes=mem.total_read_bytes.load(),scalar=mem.scalar_read_calls.load(),batches=mem.scatter_calls.load(),requests=mem.scatter_requests.load(),requested_bytes=mem.requested_read_bytes.load();
            if(io_at&&at>io_at&&bytes>=io_bytes&&scalar>=io_scalar&&batches>=io_batches&&requests>=io_requests&&requested_bytes>=io_requested_bytes){const double seconds=(at-io_at)*.001;
                plat_log("Reader I/O rates: valid_bytes/s=%.0f scalar_calls/s=%.1f scatter_calls/s=%.1f scatter_requests/s=%.1f requested_host_bytes/s=%.0f",(bytes-io_bytes)/seconds,(scalar-io_scalar)/seconds,(batches-io_batches)/seconds,(requests-io_requests)/seconds,(requested_bytes-io_requested_bytes)/seconds);
            }
            io_at=at;io_bytes=bytes;io_scalar=scalar;io_batches=batches;io_requests=requests;io_requested_bytes=requested_bytes;plat_log("Reader cadence: configured_cap=%d Hz period_avg=%.2f ms period_min=%llu ms period_max=%llu ms last_maintenance=%llu ms last_wait=%llu ms",std::clamp(cfg.I("reader.max_hz"),30,500),cadence_count?(double)cadence_sum/cadence_count:0.,(unsigned long long)(cadence_count?cadence_min:0),(unsigned long long)cadence_max,(unsigned long long)snap.maintenance_ms.load(),(unsigned long long)snap.wait_ms.load());cadence_sum=cadence_count=cadence_max=0;cadence_min=UINT64_MAX;plat_log("Reader timing: %.2f Hz, poll %llu ms, publish %llu ms, last scan %llu ms, data age %llu ms, matrix host age %lld ms, matrix unchanged %lld ms",(double)snap.read_hz_exact.load(),(unsigned long long)snap.poll_ms.load(),(unsigned long long)snap.publish_ms.load(),(unsigned long long)snap.scan_ms.load(),(unsigned long long)(now_ms()-snap.last_read_ms.load()),(snap.matrix_sample_ms.load()?(long long)(now_ms()-snap.matrix_sample_ms.load()):-1LL),(snap.matrix_change_ms.load()?(long long)(now_ms()-snap.matrix_change_ms.load()):-1LL));}
        const auto maintenance_ms=off.resolved&&reader_running?live_backend::live.maintenance():0;snap.maintenance_ms=maintenance_ms;if(maintenance_ms)snap.scan_ms=maintenance_ms;
        plat_stage("reader","waiting for next update",false);auto deadline=iteration_started+std::chrono::microseconds(1000000/std::clamp(cfg.I("reader.max_hz"),30,500));const auto wait_started=now_ms();if(std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_until(deadline);snap.wait_ms=now_ms()-wait_started;
        const std::array<u64,4> elapsed_stages{snap.poll_ms.load(),snap.publish_ms.load(),maintenance_ms,snap.wait_ms.load()};u64 accounted=0;
        for(std::size_t i=0;i<elapsed_stages.size();++i){stage_sums[i]+=elapsed_stages[i];stage_maxima[i]=std::max(stage_maxima[i],elapsed_stages[i]);accounted+=elapsed_stages[i];}
        const auto elapsed=now_ms()-start,other=elapsed>accounted?elapsed-accounted:0;stage_sums[4]+=other;stage_maxima[4]=std::max(stage_maxima[4],other);++completed_cycles;
    }});
}
void entity_thread_stop(){mem.invalidate_attachment_epoch();snap.native_binding_epoch=0;snap.frame_valid=false;bool was_running=reader_running.exchange(false);if(reader_thread.joinable())reader_thread.join();snap.read_hz=0;snap.read_hz_exact=0;snap.poll_ms=0;snap.publish_ms=0;snap.scan_ms=0;snap.discovery_completed_ms=0;snap.discovery_interval_ms=0;snap.discovery_pages_read=0;snap.discovery_pages_unavailable=0;snap.discovery_accepted=0;snap.discovery_pawns=0;snap.discovery_controllers=0;if(!was_running)return;live_backend::Frame empty;empty.time=now_ms();empty.status="Reader stopped";publish(std::move(empty));}
Vec2 world_to_screen(Vec3 point,u64 width,u64 height){
    float m[16];{std::lock_guard<std::mutex> guard(snap_mtx);std::copy(std::begin(snap.view),std::end(snap.view),m);}
    float w=point.x*m[12]+point.y*m[13]+point.z*m[14]+m[15];if(!std::isfinite(w)||w<.01f)return {-99999,-99999};
    float x=point.x*m[0]+point.y*m[1]+point.z*m[2]+m[3],y=point.x*m[4]+point.y*m[5]+point.z*m[6]+m[7];
    if(!std::isfinite(x)||!std::isfinite(y))return {-99999,-99999};return {(float)width*.5f*(1+x/w),(float)height*.5f*(1-y/w)};
}

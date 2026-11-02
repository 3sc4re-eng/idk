#include "live_backend.h"
#include "pe_header_check.h"
#include "map_identity.h"
#include "prediction_native.h"
#include "ballistics_demand.h"
#include "ability_read_demand.h"
#include "consumer_demand.h"
#include "native_joint_demand.h"
#include "native_target.h"
#include "world_units.h" // ACE_NATIVE_JOINT_INTEGRATION_V1
#include "deadlock_layout.h"
#include "verified_input_keymap.h"
#include "ability_catalog.h"
#include "dma.h"
#include "config.h"
#include "profile_scope.h"
#include "hero_catalog.h"
#include <cstring>
#include <cmath>
#include <set>
#include <algorithm>
namespace live_backend{
namespace L=dl_layout;
static_assert(engine_map_layout::timestamp==L::engine2_timestamp&&engine_map_layout::image_size==L::engine2_image_size);
Client live([](u64 a,void* b,u32 n){return mem.read(a,b,n);},[](std::vector<DmaRead>& requests){return mem.scatter(requests);});
bool valid_pointer(u64 p){return p>=0x10000&&p<0x00007fffffffffffULL;}
bool matrix_valid(const float* m){float total=0;for(int i=0;i<16;i++){if(!std::isfinite(m[i])||std::fabs(m[i])>1e8f)return false;total+=std::fabs(m[i]);}return total>.01f&&(std::fabs(m[12])+std::fabs(m[13])+std::fabs(m[14])+std::fabs(m[15]))>.00001f;}
bool Client::get(u64 address,void* data,u32 size){if(!data||!valid_pointer(address)||!size||size>8*1024*1024||address>0x00007fffffffffffULL-size)return false;if(cache_active_&&frame_cache_.planned(address,size)){if(frame_cache_.copy(address,data,size))return true;std::memset(data,0,size);return false;}++performance_.direct_reads;if(!read_(address,data,size)){std::memset(data,0,size);return false;}return true;}
void Client::reset(){std::lock_guard<std::recursive_mutex> lock(mutex_);binding_={};if(native_joint_bank_)native_joint_bank_->reset();world_display_requested_=false;world_label_observations_.clear();world_label_window_ms_=0;world_label_window_count_=0;map_monitor_.reset();map_layout_ready_=false;map_layout_error_="Map layout has not been validated";detected_map_.clear();map_resource_.clear();map_client_=0;map_spawn_=0;map_clock_=0;map_probe_stage_.clear();map_probe_report_ms_=0;discovery_engine_.reset();discovery_identity_revision_=identity_sync_updates_=identity_sync_skips_=0;definitions_.clear();frame_cache_.clear();cache_active_=false;helper_details_={};local_base_=0;auxiliary_cursor_=0;identities_.clear();cached_world_.clear();scan_time_=0;world_time_=0;discovery_={};scan_report_time_=0;scan_report_state_.clear();last_clock_change_=0;last_frame_=-1;last_view_.fill(0);last_view_change_=0;have_last_view_=false;classes_.clear();key_codes_.fill(0);keymap_ready_=false;keymap_retry_=0;}
void Client::recache(){std::lock_guard<std::recursive_mutex> lock(mutex_);if(native_joint_bank_)native_joint_bank_->reset();discovery_engine_.reset();discovery_identity_revision_=identity_sync_updates_=identity_sync_skips_=0;definitions_.clear();frame_cache_.clear();helper_details_={};map_monitor_.reset();detected_map_.clear();map_resource_.clear();map_client_=0;map_spawn_=0;identities_.clear();scan_time_=0;discovery_={};}
bool Client::bind(Binding binding,std::string& error,bool* metadata_failure){
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(metadata_failure)*metadata_failure=false;error.clear();bool any_build_mismatch=false;
    reset();auto check=[&](u64 base,u32 timestamp,u32 image_size,const char* name){
        plat_stage("attach",std::string("PE validation: ")+name);
        static_assert(sizeof(IMAGE_DOS_HEADER)==64&&sizeof(IMAGE_NT_HEADERS64)==264);
        const auto probe=pe_header_check::inspect(base,{timestamp,image_size},[&](u64 address,void* bytes,u32 size){return get(address,bytes,size);});
        plat_log("PE probe: PID=%lu %s",(unsigned long)mem.process_id(),pe_header_check::describe(name,probe).c_str());
        if(!probe.ready()){const bool mismatch=probe.stage==pe_header_check::Stage::BuildMismatch;if(error.empty()||(mismatch&&!any_build_mismatch))error=pe_header_check::summary(name,probe);any_build_mismatch|=mismatch;return false;}
        return true;
    };
    // Probe independently: client-header failure must not hide whether the
    // other mapped image headers are readable. Every module still must match.
    const bool client_ok=check(binding.client,L::client_timestamp,L::client_image_size,"client.dll");
    const bool engine_ok=check(binding.engine,L::engine2_timestamp,L::engine2_image_size,"engine2.dll");
    const bool input_ok=check(binding.input,L::inputsystem_timestamp,L::inputsystem_image_size,"inputsystem.dll");
    if(!client_ok||!engine_ok||!input_ok){
        if(!client_ok){
            unsigned char bytes[7]{};const bool address_ok=valid_pointer(binding.client)&&L::entities_instruction<L::client_image_size-7&&binding.client<0x00007fffffffffffULL-L::entities_instruction-7;
            const auto address=address_ok?binding.client+L::entities_instruction:0;
            const bool readable=address_ok&&get(address,bytes,sizeof(bytes));int32_t relative=0;std::memcpy(&relative,bytes+3,4);
            const auto target=(int64_t)L::entities_instruction+7+relative;
            const bool matches=readable&&bytes[0]==0x48&&bytes[1]==0x8b&&bytes[2]==0x1d&&target==L::entities_rva;
            plat_log("PE failure code diagnostic ONLY: PID=%lu address=0x%llX requested=7 complete=%d bytes=%s known_reference=%s; PE remains blocked",(unsigned long)mem.process_id(),(unsigned long long)address,readable,pe_header_check::hex(bytes,sizeof(bytes),readable).c_str(),readable?(matches?"match":"mismatch"):"unavailable");
        }
        // An actual build mismatch in ANY readable image is terminal, even if
        // a different module also suffered a metadata/read failure.
        if(metadata_failure)*metadata_failure=!any_build_mismatch;
        return false;
    }
    plat_log("PE identities: all three modules matched expected build");plat_stage("attach","validating global RIP references");
    const unsigned char entity_prefix[]={0x48,0x8b,0x1d},view_prefix[]={0x48,0x8d,0x0d},rules_prefix[]={0x48,0x89,0x1d},clock_prefix[]={0x48,0x8b,0x05};
    struct Reference{const char* name;u32 instruction,expected;const unsigned char* prefix;};
    const Reference references[]={{"globals",L::globals_instruction,L::globals_rva,clock_prefix},{"entities",L::entities_instruction,L::entities_rva,entity_prefix},{"view",L::view_instruction,L::view_rva,view_prefix},{"rules",L::rules_instruction,L::rules_rva,rules_prefix}};
    for(const auto& ref:references){
        unsigned char bytes[7]{};if(!get(binding.client+ref.instruction,bytes,sizeof(bytes))){error=std::string("Could not read client global reference: ")+ref.name;return false;}
        int32_t relative=0;std::memcpy(&relative,bytes+3,4);int64_t target=(int64_t)ref.instruction+7+relative;
        if(std::memcmp(bytes,ref.prefix,3)||target!=ref.expected){char detail[320];snprintf(detail,sizeof(detail),"Client global layout mismatch: %s instruction=%X prefix=%02X%02X%02X target=%llX expected=%X",ref.name,ref.instruction,bytes[0],bytes[1],bytes[2],(unsigned long long)target,ref.expected);error=detail;return false;}
    }
    plat_log("Global references: globals/entities/view/rules matched");plat_stage("attach","validating input and UI singletons");u64 input_vtable=0,ui_vtable=0;
    if(!get(binding.input+L::input_instance,input_vtable)||input_vtable!=binding.input+L::input_vtable||!get(binding.engine+L::engine_ui,ui_vtable)||ui_vtable!=binding.engine+L::engine_ui_vtable){error="Input/UI singleton is not initialized";return false;}
    plat_stage("attach","validating automatic map source");
    map_layout_ready_=engine_map::verify_layout([&](u64 a,void* bytes,u32 n){return get(a,bytes,n);},binding.engine,map_layout_error_);
    plat_log("Automatic map provider layout: %s%s%s",map_layout_ready_?"validated":"unavailable",map_layout_ready_?"":": ",map_layout_ready_?"":map_layout_error_.c_str());
    load_keymap(binding,true);
    plat_log("Input/UI singletons validated; key mapping %s",keymap_ready_?"ready":"unavailable (automatic actions blocked)");plat_stage("attach","layout ready");binding_=binding;for(auto& entry:L::classes)classes_[entry.vtable]=&entry;error="Matching revision 11193905; waiting for a match";return true;
}
void Client::load_keymap(Binding binding,bool announce){
    keymap_retry_=now_ms();std::array<int,256> live_keys{};
    const bool readable=get(binding.input+L::virtual_key_table,live_keys.data(),sizeof(live_keys));
    auto valid=[](const std::array<int,256>& keys){
        for(int code:keys)if(code<0||code>=512)return false;
        for(int vk='A';vk<='Z';vk++)if(keys[vk]!=vk-'A'+11)return false;
        return keys[VK_TAB]==68&&keys[VK_RETURN]==65&&keys[VK_ESCAPE]==71&&keys[VK_SHIFT]==80&&keys[VK_MENU]==82&&keys[VK_CONTROL]==84;
    };
    const char* source=nullptr;
    if(readable&&valid(live_keys)){key_codes_=live_keys;source="live virtual-key table";}
    // bind() already checked the actual PE identities. Never reuse this fixed
    // initializer across a different inputsystem build, even if other fields match.
    else if(L::inputsystem_timestamp==verified_input_keymap::timestamp&&L::inputsystem_image_size==verified_input_keymap::image_size){
        key_codes_=verified_input_keymap::virtual_keys;source="verified initializer fallback";
        plat_log("Virtual-key table %s at RVA 0x%X; using the exact-build fixed initializer (timestamp %08X, image %X)",readable?"not initialized/invalid":"unreadable",L::virtual_key_table,verified_input_keymap::timestamp,verified_input_keymap::image_size);
    }else{
        key_codes_.fill(0);keymap_ready_=false;
        if(announce)plat_log("Virtual-key mapping unavailable: live table %s; no matching verified fallback. Reader continues; automatic actions remain blocked",readable?"invalid":"unreadable");
        return;
    }
    // The DLL's VK table uses generic modifiers. Its reverse-map initializer
    // establishes right-side codes 81/83/85; normalize the side-specific VKs too.
    key_codes_[VK_LSHIFT]=80;key_codes_[VK_RSHIFT]=81;
    key_codes_[VK_LMENU]=82;key_codes_[VK_RMENU]=83;
    key_codes_[VK_LCONTROL]=84;key_codes_[VK_RCONTROL]=85;
    // Mouse ButtonCode values are read from the game bitmap, not local key state.
    key_codes_[VK_LBUTTON]=107;key_codes_[VK_RBUTTON]=108;key_codes_[VK_MBUTTON]=109;key_codes_[VK_XBUTTON1]=110;key_codes_[VK_XBUTTON2]=111;
    keymap_ready_=true;plat_log("Game key mapping ready: %s; Tab=%d, left/right Control=%d/%d",source,key_codes_[VK_TAB],key_codes_[VK_LCONTROL],key_codes_[VK_RCONTROL]);
}
bool Client::scan(){
    if(!discovery_engine_){
        discovery_engine_=std::make_unique<incremental_discovery::Engine>(
            [&](u64 a,void* b,u32 n){return fresh(a,b,n);},
            [&](std::vector<incremental_discovery::Request>& requests){
                std::vector<DmaRead> dma;dma.reserve(requests.size());for(auto& r:requests)dma.push_back({r.address,r.destination,r.size,0,false});
                bool ok=true;if(batch_)ok=batch_(dma);else for(auto& r:dma){r.valid=fresh(r.address,r.data,r.size);r.transferred=r.valid?r.size:0;}
                ++performance_.scatter_calls;performance_.scatter_requests+=dma.size();
                for(std::size_t i=0;i<requests.size();++i)requests[i].complete=ok&&dma[i].valid&&dma[i].transferred==dma[i].size;
                return ok;
            },
            [&](u32 rva,incremental_discovery::Class& out){auto found=classes_.find(rva);if(found==classes_.end())return false;const auto* value=static_cast<const L::Class*>(found->second);out={value->kind,value->name};return true;});
        incremental_discovery::Config config;config.entities_global=binding_.client+L::entities_rva;config.client_base=binding_.client;config.client_size=L::client_image_size;config.session_token=1;config.max_headers_per_step=128;discovery_engine_->reset(config);
    }
    discovery_engine_->set_cycle_interval((u64)std::clamp(cfg.I("reader.discovery_ms"),250,5000));
    const bool changed=discovery_engine_->step(now_ms());const auto& d=discovery_engine_->diagnostics();
    // step() reports I/O work, not membership change. Empty chunks may erase
    // identities even when it returns false. Copy only on the explicit revision.
    const auto identity_revision=discovery_engine_->identity_revision();
    if(incremental_discovery::identity_revision_changed(discovery_identity_revision_,identity_revision)){
        identities_.clear();for(const auto& pair:discovery_engine_->identities()){const auto& id=pair.second;identities_.emplace(pair.first,Identity{id.entity,id.handle,id.kind,id.name,id.record});}
        discovery_identity_revision_=identity_revision;++identity_sync_updates_;
    }else ++identity_sync_skips_;
    discovery_.attempted_ms=d.attempted_ms;discovery_.completed_ms=d.completed_ms;discovery_.interval_ms=d.interval_ms;discovery_.elapsed_ms=d.completed_ms>=d.attempted_ms?d.completed_ms-d.attempted_ms:0;
    discovery_.chunks=d.chunks;discovery_.invalid_chunks=d.invalid_chunks;discovery_.pages_read=d.pages_read;discovery_.pages_unavailable=d.pages_unavailable;discovery_.records_read=d.records_read;discovery_.empty_records=d.empty_records;discovery_.flags_rejected=d.flags_rejected;discovery_.invalid_addresses=d.invalid_addresses;discovery_.serial_mismatches=d.serial_mismatches;discovery_.candidates=d.candidates;discovery_.object_unreadable=d.object_unreadable;discovery_.vtable_outside_client=d.vtable_outside_client;discovery_.unknown_classes=d.unknown_classes;discovery_.known_classes=d.known_classes;discovery_.identity_mismatches=d.identity_mismatches;discovery_.accepted=d.accepted;discovery_.pawns=d.pawns;discovery_.controllers=d.controllers;discovery_.first_unavailable_page=d.first_unavailable_page;discovery_.first_unknown_vtable=d.first_unknown_vtable;discovery_.status=d.status;
    const auto state=d.status+" | "+std::to_string(d.accepted);
    if(now_ms()-scan_report_time_>=5000){scan_report_time_=now_ms();plat_log("Incremental discovery: %s | active=%d cycle=%llu step_headers=%u step_pages=%u accepted=%u pawns=%u controllers=%u candidates=%u unknown=%u page_holes=%u identity_revision=%llu identity_sync_updates=%llu identity_sync_skips=%llu",state.c_str(),d.active,(unsigned long long)d.cycle,d.step_headers,d.step_pages,d.accepted,d.pawns,d.controllers,d.candidates,d.unknown_classes,d.pages_unavailable,(unsigned long long)identity_revision,(unsigned long long)identity_sync_updates_,(unsigned long long)identity_sync_skips_);}
    scan_time_=d.completed_ms;return changed;
}

u64 Client::resolve(u32 handle)const{auto it=identities_.find(handle&0x7fff);return it!=identities_.end()&&it->second.handle==handle?it->second.entity:0;}
bool Client::entity(const Identity& id,Entity& result,const health_state::PlayerMaximum* maximum){
    result={};auto reject=[&](const char* reason){if(id.entity==local_base_)performance_.local_rejection=reason;return false;};
    u64 identity=0;u32 handle=0,identity_flags=0;if(!get(id.entity+L::CEntityInstance::m_pEntity,identity))return reject("local identity pointer unreadable");if(id.identity&&identity!=id.identity)return reject("local identity pointer changed");if(!get(identity+0x10,handle))return reject("local serial handle unreadable");if(handle!=id.handle)return reject("local serial handle mismatch");if(!get(identity+0x30,identity_flags))return reject("local identity flags unreadable");if(identity_flags&0x717)return reject("local identity flags rejected");
    // Consume individually validated cached slices, so a failed health slice
    // cannot turn an independently readable identity/scene pointer into zero.
    auto copy=[&](u32 offset,void* data,size_t size){return size<=UINT32_MAX&&get(id.entity+offset,data,(u32)size);};
    u64 scene=0;int32_t hp=0,max_hp=0;unsigned char team=0,life=0;
    const bool scene_read=copy(L::C_BaseEntity::m_pGameSceneNode,&scene,8);
    const bool current_read=copy(L::C_BaseEntity::m_iHealth,&hp,4),maximum_read=copy(L::C_BaseEntity::m_iMaxHealth,&max_hp,4),life_read=copy(L::C_BaseEntity::m_lifeState,&life,1);
    if(!scene_read||!copy(L::C_BaseEntity::m_iTeamNum,&team,1))return reject("local pose roots unavailable");
    const auto health=health_state::evaluate(hp,max_hp,life,id.kind==1,maximum,current_read,maximum_read,life_read);
    if(id.entity==local_base_){performance_.local_health=hp;performance_.local_base_max=max_hp;performance_.local_global_max=maximum?maximum->maximum:0;performance_.local_global_health=maximum?maximum->controller_health:0;performance_.local_life=life;performance_.local_global_readable=maximum&&maximum->readable;performance_.local_health_current_valid=health.current_valid;performance_.local_health_max_valid=health.maximum_valid;performance_.local_alive_valid=health.alive_valid;performance_.local_health_pair_valid=health.pair_valid;performance_.local_health_reason=health.reason;performance_.local_health_source=health.source;performance_.local_health_handle=id.handle;}
    if(!get(scene+L::CGameSceneNode::m_vecAbsOrigin,result.pos))return reject("local scene position unreadable");if(!std::isfinite(result.pos.x)||!std::isfinite(result.pos.y)||!std::isfinite(result.pos.z)||result.pos.len()>1e7f)return reject("local scene position invalid");
    bool transform_valid=get(scene+L::CGameSceneNode::m_angAbsRotation,result.facing)&&get(scene+L::CGameSceneNode::m_flAbsScale,result.scale)&&std::isfinite(result.facing.len())&&std::isfinite(result.scale)&&result.scale>=.01f&&result.scale<=100;
    u64 collision=0;copy(L::C_BaseEntity::m_pCollision,&collision,8);
    result.bounds_valid=transform_valid&&get(collision+0x40,result.bounds_min)&&get(collision+0x4c,result.bounds_max)&&std::isfinite(result.bounds_min.len())&&std::isfinite(result.bounds_max.len())&&result.bounds_max.x>result.bounds_min.x&&result.bounds_max.y>result.bounds_min.y&&result.bounds_max.z>result.bounds_min.z&&result.bounds_max.len()<10000;
    unsigned char dormant=0;if(!get(scene+L::CGameSceneNode::m_bDormant,dormant))return reject("local scene dormancy unreadable");if(dormant>1)return reject("local scene dormancy invalid");if(dormant&&id.entity!=local_base_)return false;
    if(!copy(L::C_BaseEntity::m_vecAbsVelocity,&result.vel,12))return reject("local velocity unreadable");if(!std::isfinite(result.vel.len())||result.vel.len()>100000)return reject("local velocity invalid");
    result.base=id.entity;result.idx=id.handle&0x7fff;result.handle=id.handle;result.team=team;result.hp=(float)hp;result.max_hp=(float)health.maximum;result.alive=health.alive;result.health_current_valid=health.current_valid;result.health_max_valid=health.maximum_valid;result.health_pair_valid=health.pair_valid;result.life_state_valid=health.life_valid;result.alive_valid=health.alive_valid;
    result.cls=id.kind==1?EClass::Player:(id.kind==5?EClass::NPC:(id.kind==6?EClass::Projectile:EClass::World));result.type_name=id.name;result.last_seen=now_ms();result.valid=true;
    const bool subclass_read=copy(L::C_BaseEntity::m_nSubclassID,&result.ability_token,4);
    if(id.kind==7&&world_display_requested_&&world_label_observations_.size()<64){
        // m_nSubclassID@904 is already in the 0x600-byte root acquisition.
        // The bool distinguishes a genuine zero token from unavailable bytes.
        if(result.last_seen-world_label_window_ms_>=1000){world_label_window_ms_=result.last_seen;world_label_window_count_=0;}
        if(world_label_window_count_<4){
            const std::string key=std::string(id.name)+"|"+(subclass_read?std::to_string(result.ability_token):"unreadable");
            if(world_label_observations_.insert(key).second){
                ++world_label_window_count_;
                plat_log("World label evidence: class=%s handle=%08X index=%u entity=0x%llX subclass_read=%d subclass=%08X source=C_BaseEntity::m_nSubclassID@%u extra_dma_reads=0; subtype not yet verified",id.name,(unsigned)id.handle,(unsigned)result.idx,(unsigned long long)id.entity,subclass_read?1:0,(unsigned)result.ability_token,(unsigned)L::C_BaseEntity::m_nSubclassID);
            }
        }
    }
    u32 owner=0;copy(L::C_BaseEntity::m_hOwnerEntity,&owner,4);result.owner=resolve(owner);
    u64 modifiers=0;copy(L::C_BaseEntity::m_pModifierProp,&modifiers,8);
    if((id.kind==1||id.kind==5)&&valid_pointer(modifiers)){
        std::array<u32,36> bits{};if(get(modifiers+L::CModifierProperty::m_bvEnabledStateMask,bits.data(),sizeof(bits))){
            auto state=[&](unsigned bit){return(((bits[bit/32]|bits[24+bit/32])&~bits[12+bit/32])&(1u<<(bit%32)))!=0;};
            result.disarmed=state(14);result.active_reload_allowed=state(177);result.bullet_invuln=state(96)||state(134);result.stealthed=state(34)||state(35);result.invuln=state(21)||state(22)||state(23)||state(29);result.charging=state(32);result.ability_blocked=state(17)||state(19)||state(20)||state(31);result.modifiers_valid=true;
            struct Vector{int count,pad;u64 data;int max,pad2;};Vector v{};
            if(id.entity==local_base_&&helper_details_.automatic&&get(modifiers+L::CModifierProperty::m_vecModifiers,v)&&v.count>=0&&v.count<=512&&v.max>=v.count&&v.max<=4096){
                std::vector<u64> entries(v.count);if(entries.empty()||get(v.data,entries.data(),(u32)entries.size()*8))for(auto address:entries){
                    std::array<unsigned char,0x80> raw{};if(!get(address,raw.data(),(u32)raw.size())||raw[0x70]!=0||raw[0x71]!=0||raw[0x73]!=1||(raw[0x60]&16))continue;
                    u32 token=0,caster=0;std::memcpy(&token,raw.data()+0x5c,4);std::memcpy(&caster,raw.data()+0x38,4);u64 source=resolve(caster);unsigned char caster_team=0;
                    auto* data=ability_data::find(token);if(data&&data->enemy&&source&&get(source+L::C_BaseEntity::m_iTeamNum,caster_team)&&caster_team!=team)result.removable_debuffs.push_back(token);
                }
            }
        }
    }
    if(id.kind==6){result.alive=true;if(!get(id.entity+L::C_CitadelProjectile::m_abilityID,result.ability_token))result.valid=false;auto* definition=ability_data::find(result.ability_token);result.collision_radius=definition?definition->projectile_radius:0;}
    if(id.kind==9){unsigned char type=0,flags=0;result.world_obstacle=result.bounds_valid&&get(collision+0x5b,type)&&get(collision+0x5a,flags)&&type>0&&type<=6&&!(flags&4)&&std::isfinite(result.scale)&&result.scale>.001f&&result.scale<100;}
    if(id.kind==7||id.kind==9)result.alive=true;
    return result.valid; // Health warnings never invalidate a verified pose.
}
void Client::abilities(Entity& entity,float time,bool ultimate_trained,bool ballistics){
    bool needs_native_ballistics=false;
    if(ballistics){
        // Resolve the SAME hero/global override chain used by Aim and Trigger.
        // The diagnostic display is global and remains a consumer on its own.
        profile_scope::ActiveScope hero_settings(entity);
        needs_native_ballistics=ballistics_demand::required({
            cfg.B("aim.enabled"),cfg.B("aim.prediction"),
            cfg.B("trigger.enabled"),cfg.B("trigger.prediction"),
            cfg.B("trigger.magnet"),cfg.B("trigger.magnet_prediction"),
            cfg.B("dbg.ballistics")});
    }
    u64 modifiers=0,globals=0;int tick=-1;if(ballistics){get(entity.base+L::C_BaseEntity::m_pModifierProp,modifiers);get(binding_.client+L::globals_rva,globals);get(globals+0x44,tick);u32 selected=0,casting=0;get(entity.base+L::C_CitadelPlayerPawn::m_CCitadelAbilityComponent+0xc8,selected);get(entity.base+L::C_CitadelPlayerPawn::m_CCitadelAbilityComponent+0xd0,casting);entity.selected_ability=resolve(casting)?casting:(resolve(selected)?selected:0);}

    struct Vector{int32_t count,pad;u64 data;int32_t max,pad2;};Vector vector{};
    if(!get(entity.base+L::C_CitadelPlayerPawn::m_CCitadelAbilityComponent+L::CCitadelAbilityComponent::m_vecAbilities,vector)||vector.count<0||vector.count>128||(vector.count&&!valid_pointer(vector.data)))return;
    std::vector<u32> handles(vector.count);if(vector.count&&!get(vector.data,handles.data(),(u32)handles.size()*4))return;
    for(auto handle:handles){
        u64 base=resolve(handle);if(!base)continue;auto known=identities_.find(handle&0x7fff);if(known==identities_.end()||known->second.kind!=4)continue;u64 identity=0;u32 current_handle=0,flags=0;if(!get(base+L::CEntityInstance::m_pEntity,identity)||identity!=known->second.identity||!get(identity+0x10,current_handle)||current_handle!=handle||!get(identity+0x30,flags)||(flags&0x717))continue;
        std::array<unsigned char,0x7e0> raw{};if(!get(base,raw.data(),(u32)raw.size()))continue;
        auto at=[&](auto& out,u32 offset){if(offset+sizeof(out)<=raw.size())std::memcpy(&out,raw.data()+offset,sizeof(out));};
        Ability a;a.base=base;a.handle=handle;a.type=known->second.name;at(a.token,L::C_BaseEntity::m_nSubclassID);at(a.slot,L::C_CitadelBaseAbility::m_eAbilitySlot);
        const auto* data=ability_data::find(a.token);if(!data&&a.slot>=0&&a.slot<4&&a.type.find("Item")==std::string::npos){data=ability_data::signature(entity.hero,a.slot);if(data)a.token=data->token;}
        at(a.cooldown_end,L::C_CitadelBaseAbility::m_flCooldownEnd);at(a.cast_start,L::C_CitadelBaseAbility::m_flCastDelayStartTime);at(a.channel_start,L::C_CitadelBaseAbility::m_flChannelStartTime);at(a.completed,L::C_CitadelBaseAbility::m_flCastCompletedTime);at(a.charges,L::C_CitadelBaseAbility::m_iRemainingCharges);
        unsigned char channel=0,casting=0,cooling=0;at(channel,L::C_CitadelBaseAbility::m_bChanneling);at(casting,L::C_CitadelBaseAbility::m_bInCastDelay);at(cooling,L::C_CitadelBaseAbility::m_bIsCoolingDownInternal);a.channeling=channel!=0;a.casting=casting!=0;
        u32 upgrades=0;at(upgrades,L::C_CitadelBaseAbility::m_nUpgradeInfo);float post=0,stopped=0;at(post,L::C_CitadelBaseAbility::m_flPostCastDelayEndTime);at(stopped,L::C_CitadelBaseAbility::m_flStoppedCooldownRemaining);
        a.valid=data&&std::isfinite(a.cooldown_end)&&std::isfinite(post)&&std::isfinite(stopped)&&a.charges>=0&&a.charges<100&&channel<=1&&casting<=1&&cooling<=1;
        a.upgrade_bits=upgrades&0xffff;bool trained=data&&(data->start_trained||(a.upgrade_bits&1)!=0||(a.slot==3&&ultimate_trained));
        a.trained=trained;
        if(helper_details_.automatic&&a.token==754480263u){ // Mystic Expansion: networked imbued ability tokens
            Vector imbued{};if(get(base+L::C_CitadelBaseAbility::m_vecImbuedAbilities,imbued)&&imbued.count>=0&&imbued.count<=4&&imbued.max>=imbued.count&&imbued.max<=16){
                a.imbued_tokens.resize(imbued.count);a.imbued_valid=a.imbued_tokens.empty()||get(imbued.data,a.imbued_tokens.data(),(u32)a.imbued_tokens.size()*4);
                if(!a.imbued_valid)a.imbued_tokens.clear();
            }
        }
        a.ready=a.valid&&trained&&entity.modifiers_valid&&!a.casting&&!a.channeling&&!entity.ability_blocked&&post<=time&&stopped<=0&&((data->charges>0&&a.charges>0)||(!cooling&&a.cooldown_end<=time));
        if(a.type.find("PrimaryWeapon")!=std::string::npos){
            unsigned char reloading=0,allowed=0;u64 vdata=0;float fraction=0,width=0;
            entity.weapon_valid=get(base+0x185c,reloading)&&reloading<=1&&get(base+0x1868,allowed)&&allowed<=1;
            entity.reloading=!entity.weapon_valid||reloading;entity.active_reload_allowed=entity.active_reload_allowed&&allowed;
            // Core reload/weapon gates above remain current every frame.
            // The timing window is only consumed by active automatic reload.
            if(helper_details_.active_reload){
                get(base+0x1870,entity.reload_begin);get(base+0x1874,entity.reload_end);get(base+0x1878,entity.reload_pause);
                if(get(base+0x390,vdata)&&get(vdata+0x10e8,fraction)&&get(vdata+0x10ec,width)&&std::isfinite(fraction)&&std::isfinite(width)){entity.reload_fraction=fraction;entity.reload_width=width;}
            }
        }
        if(needs_native_ballistics&&(a.type.find("PrimaryWeapon")!=std::string::npos||handle==entity.selected_ability)){
            u64 vdata=0;if(get(base+0x390,vdata))a.shot=shot(handle,vdata,modifiers,tick);
            if(data&&data->item&&data->single&&a.shot.valid)a.shot.kind=prediction::Kind::UnitTarget;
            if(a.type.find("PrimaryWeapon")!=std::string::npos){int mode=0;if(!get(base+0x1890,mode)||mode!=0){a.shot.valid=false;a.shot.reason="Special fire mode needs its own verified ballistics";}float queued=0;if(get(base+0x16e8,queued)&&queued>time&&queued<time+5)a.shot.delay=queued-time;}
            else if(data&&a.casting)a.shot.delay+=std::max(0.f,data->cast-std::max(0.f,time-a.cast_start));
        }
        a.shot.source=base;a.shot.token=a.token;a.shot.name=data?data->title:"Primary weapon";
        if(a.type.find("PrimaryWeapon")!=std::string::npos)entity.primary_shot=a.shot;
        if(helper_details_.active_reload&&a.type=="CCitadel_Item_ActiveReload"){unsigned char failed=1;if(!get(base+0x16e1,failed)||failed>1)a.valid=a.ready=false;else a.active_reload_failed=failed;}
        if(helper_details_.defensive&&a.type=="CCitadel_Ability_Lash_Ultimate"){
            Vector targets{};if(get(base+L::CCitadelBaseLockonAbility::m_vecLockonTargets,targets)&&targets.count>=0&&targets.count<=64){
                std::vector<unsigned char> entries(targets.count*L::LockonTarget_t::size);if(entries.empty()||get(targets.data,entries.data(),(u32)entries.size()))for(int i=0;i<targets.count;i++){
                    u32 target=0;std::memcpy(&target,entries.data()+i*L::LockonTarget_t::size+L::LockonTarget_t::m_hTarget,4);if(resolve(target))a.targets.push_back(target);
                }
            }get(base+L::CCitadel_Ability_Lash_Ultimate::m_flNextStateTime,a.impact_time);
        }
        if(helper_details_.defensive&&a.type=="CCitadel_Ability_Bull_Leap"){
            bool position=get(base+dl_layout::CCitadel_Ability_Bull_Leap::m_vecCrashPosition,a.impact_position),timing=get(base+dl_layout::CCitadel_Ability_Bull_Leap::m_flNextStateTime+dl_layout::CCitadelAutoScaledTime::m_flTime,a.impact_time);
            unsigned char state=0;bool phase=get(base+dl_layout::CCitadel_Ability_Bull_Leap::m_eLeapState,state);a.has_impact_position=position&&timing&&phase&&(state==3||state==5)&&std::isfinite(a.impact_position.len())&&a.impact_position.len()<1e7f&&std::isfinite(a.impact_time)&&a.impact_time>time;
        }
        if(helper_details_.defensive&&a.type=="CCitadel_Ability_LashDownStrike"){
            bool position=get(base+dl_layout::CCitadel_Ability_LashDownStrike::m_vDamagePos,a.impact_position),timing=get(base+dl_layout::CCitadel_Ability_LashDownStrike::m_ImpactTime,a.impact_time);
            bool crashing=false;bool phase=get(base+dl_layout::CCitadel_Ability_LashDownStrike::m_bIsCrashingDown,crashing);a.has_impact_position=position&&timing&&phase&&crashing&&std::isfinite(a.impact_position.len())&&a.impact_position.len()<1e7f&&std::isfinite(a.impact_time)&&a.impact_time>time;
        }
        if(a.type=="CCitadel_Ability_VampireBat_BatBlink"){
            float recast=0;bool blinking=false;get(base+0x1b10,recast);get(base+0x1b0c,blinking);if(recast>time||blinking)a.ready=false;
        }
        if(a.type=="CCitadel_Ability_Fencer_Riposte"){
            int state=0;if(!get(base+0x1700,state)||state!=0)a.ready=false;
        }
        if(a.type=="CCitadel_Ability_Bull_Charge"&&entity.charging)a.channeling=true;
        entity.casting|=a.casting||a.channeling;entity.abilities.push_back(std::move(a));
    }
    entity.current_shot=entity.selected_ability?prediction::Shot{}:entity.primary_shot;if(entity.selected_ability)for(const auto& a:entity.abilities)if(a.handle==entity.selected_ability){entity.current_shot=a.shot;break;}
    entity.abilities_valid=true;
}
namespace {struct RemoteVector {int count=0,pad=0;u64 data=0;int max=0,pad2=0;};bool bounded_vector(const RemoteVector& v,int max){return v.count>=0&&v.count<=max&&v.max>=v.count&&v.max<=std::max(max,4096)&&(!v.count||valid_pointer(v.data));}}
void Client::flush(unsigned phase){const auto began=now_ms();const auto before=frame_cache_.counters;frame_cache_.flush(batch_,[&](u64 a,void* b,u32 n){return fresh(a,b,n);});const auto& after=frame_cache_.counters;performance_.scatter_calls+=after.batches-before.batches;performance_.scatter_requests+=after.requests-before.requests;performance_.failed_ranges+=after.failed-before.failed;if(phase<performance_.phase_ms.size())performance_.phase_ms[phase]+=now_ms()-began;}
void Client::prefetch_roots(const std::vector<const Identity*>& ids){
    for(const auto* id:ids){queue(id->entity,0x600);if(valid_pointer(id->identity))queue(id->identity,0x34);
        if(id->kind==1){queue(id->entity+L::C_CitadelPlayerPawn::m_CCitadelAbilityComponent+L::CCitadelAbilityComponent::m_vecAbilities,sizeof(RemoteVector));queue(id->entity+L::C_CitadelPlayerPawn::m_CCitadelAbilityComponent+0xc8,12);}
        if(id->kind==2){queue(id->entity+L::CBasePlayerController::m_bIsLocalPlayerController,1);queue(id->entity+L::CCitadelPlayerController::m_hHeroPawn,4);queue(id->entity+L::CBasePlayerController::m_steamID,8);queue(id->entity+L::CCitadelPlayerController::m_PlayerDataGlobal,144);}
        if(id->kind==3)queue(id->entity+L::C_CitadelTeam::m_vecFOWEntities,sizeof(RemoteVector));
        if(id->kind==6)queue(id->entity+L::C_CitadelProjectile::m_abilityID,4);
    }
}
void Client::prefetch_dependencies(const std::vector<const Identity*>& ids){
    for(const auto* id:ids){if(id->kind==2||id->kind==3)continue;u64 scene=0,collision=0,modifiers=0;
        if(get(id->entity+L::C_BaseEntity::m_pGameSceneNode,scene))queue(scene,L::CGameSceneNode::size);
        if(get(id->entity+L::C_BaseEntity::m_pCollision,collision))queue(collision+0x40,0x1c);
        if(id->kind==6){u32 owner_handle=0;if(get(id->entity+L::C_BaseEntity::m_hOwnerEntity,owner_handle)){auto owner=identities_.find(owner_handle&0x7fff);if(owner!=identities_.end()&&owner->second.handle==owner_handle&&owner->second.kind==4){queue(owner->second.entity,0x600);queue(owner->second.identity,0x34);}}}
        if((id->kind==1||id->kind==5)&&get(id->entity+L::C_BaseEntity::m_pModifierProp,modifiers)){queue(modifiers+L::CModifierProperty::m_bvEnabledStateMask,144);if(id->entity==local_base_)queue(modifiers+L::CModifierProperty::m_vecModifiers,sizeof(RemoteVector));}
    }
    if(local_base_){queue(local_base_+L::C_BasePlayerPawn::m_flMouseSensitivity,4);queue(local_base_+L::C_BasePlayerPawn::m_flFOVSensitivityAdjust,4);queue(local_base_+L::C_BasePlayerPawn::m_pObserverServices,8);}
}
void Client::prefetch_abilities(const std::vector<const Identity*>& players){
    for(const auto* player:players){RemoteVector v{};if(get(player->entity+L::C_CitadelPlayerPawn::m_CCitadelAbilityComponent+L::CCitadelAbilityComponent::m_vecAbilities,v)&&bounded_vector(v,128)&&v.count)queue(v.data,u32(v.count)*4);}
    if(local_base_){u64 mods=0,observer=0;if(helper_details_.automatic&&get(local_base_+L::C_BaseEntity::m_pModifierProp,mods)){RemoteVector v{};if(get(mods+L::CModifierProperty::m_vecModifiers,v)&&bounded_vector(v,512)&&v.count)queue(v.data,u32(v.count)*8);}if(get(local_base_+L::C_BasePlayerPawn::m_pObserverServices,observer))queue(observer+L::CPlayer_ObserverServices::m_iObserverMode,1);}
}
void Client::prefetch_ability_details(const std::vector<const Identity*>& players){
    for(const auto* player:players){RemoteVector v{};if(!get(player->entity+L::C_CitadelPlayerPawn::m_CCitadelAbilityComponent+L::CCitadelAbilityComponent::m_vecAbilities,v)||!bounded_vector(v,128))continue;std::vector<u32> handles(v.count);if(v.count&&!get(v.data,handles.data(),u32(handles.size())*4))continue;
        for(auto handle:handles){auto found=identities_.find(handle&0x7fff);if(found==identities_.end()||found->second.handle!=handle||found->second.kind!=4)continue;const auto& id=found->second;const auto base=id.entity;queue(base,0x7e0);queue(id.identity,0x34);
            if(id.name.find("PrimaryWeapon")!=std::string::npos){queue(base+0x16e8,4);queue(base+0x185c,0x38);}
            if(helper_details_.active_reload&&id.name=="CCitadel_Item_ActiveReload")queue(base+0x16e1,1);
            if(helper_details_.defensive&&id.name=="CCitadel_Ability_Lash_Ultimate"){queue(base+L::CCitadelBaseLockonAbility::m_vecLockonTargets,sizeof(RemoteVector));queue(base+L::CCitadel_Ability_Lash_Ultimate::m_flNextStateTime,4);}
            if(helper_details_.defensive&&id.name=="CCitadel_Ability_Bull_Leap"){queue(base+L::CCitadel_Ability_Bull_Leap::m_vecCrashPosition,12);queue(base+L::CCitadel_Ability_Bull_Leap::m_flNextStateTime+L::CCitadelAutoScaledTime::m_flTime,4);queue(base+L::CCitadel_Ability_Bull_Leap::m_eLeapState,1);}
            if(helper_details_.defensive&&id.name=="CCitadel_Ability_LashDownStrike"){queue(base+L::CCitadel_Ability_LashDownStrike::m_vDamagePos,12);queue(base+L::CCitadel_Ability_LashDownStrike::m_ImpactTime,4);queue(base+L::CCitadel_Ability_LashDownStrike::m_bIsCrashingDown,1);}
            if(id.name=="CCitadel_Ability_VampireBat_BatBlink")queue(base+0x1b0c,8);
            if(id.name=="CCitadel_Ability_Fencer_Riposte")queue(base+0x1700,4);
        }
    }
    if(local_base_&&helper_details_.automatic){u64 mods=0;if(get(local_base_+L::C_BaseEntity::m_pModifierProp,mods)){RemoteVector v{};if(get(mods+L::CModifierProperty::m_vecModifiers,v)&&bounded_vector(v,512)){std::vector<u64> records(v.count);if(!v.count||get(v.data,records.data(),u32(records.size())*8))for(auto p:records)queue(p,0x80);}}}
}

prediction::Shot Client::shot(u32 handle,u64 vdata,u64 modifiers,int tick){
    if(definitions_.size()>64)definitions_.clear();auto& definition=definitions_[handle];
    if(definition.vdata!=vdata||now_ms()-definition.loaded_ms>=1000){definition={};definition.vdata=vdata;definition.loaded_ms=now_ms();}
    u64 gravity_record=0;return prediction_native::read([&](u64 address,void* destination,u32 size){
        // Only immutable weapon-definition tree/record bytes are eligible.
        // Modifier epochs, dirty flags, sequence-word rechecks, mode switches,
        // and live gravity ALWAYS call the fresh reader; never the frame cache.
        const bool mutable_read=(valid_pointer(modifiers)&&address>=modifiers&&address-modifiers<L::CModifierProperty::size)||address==binding_.client+prediction_native::cache_enabled||address==binding_.client+prediction_native::cache_default_mode||(address>=binding_.client+prediction_native::cache_modes&&address<binding_.client+prediction_native::cache_modes+512)||address==binding_.client+prediction_native::gravity_reference+8||(gravity_record&&address==gravity_record+0x58);
        const bool fixed=!mutable_read&&((address==binding_.client+prediction_native::primary_symbol&&size==8)||(address>=vdata+0x18c&&address+size<=vdata+0x19c)||(definition.capacity&&definition.capacity<=128&&valid_pointer(definition.nodes)&&address>=definition.nodes&&address+size<=definition.nodes+u64(definition.capacity)*0x8e8));
        const auto key=std::make_pair(address,size);if(fixed){auto found=definition.blocks.find(key);if(found!=definition.blocks.end()){std::memcpy(destination,found->second.data(),size);return true;}}
        if(!fresh(address,destination,size))return false;
        if(address==binding_.client+prediction_native::gravity_reference+8&&size==8)std::memcpy(&gravity_record,destination,8);
        if(address==vdata+0x18c&&size==4){std::memcpy(&definition.capacity,destination,4);definition.capacity&=0x7fffffff;}
        if(address==vdata+0x190&&size==8)std::memcpy(&definition.nodes,destination,8);
        if(fixed&&definition.blocks.size()<256){auto& bytes=definition.blocks[key];bytes.resize(size);std::memcpy(bytes.data(),destination,size);}return true;
    },binding_.client,vdata,modifiers,tick,false);
}

u64 Client::maintenance(){std::lock_guard<std::recursive_mutex> lock(mutex_);if(!binding_.client)return 0;const auto start=now_ms();scan();return now_ms()-start;}
void Client::detect_map(Frame& f){
    if(f.clock_valid&&map_clock_>0&&f.game_time+1<map_clock_){map_monitor_.reset();cached_world_.clear();world_time_=0;}
    if(f.clock_valid)map_clock_=f.game_time;
    if(!map_layout_ready_){f.map_name.clear();f.map_status="Automatic map unavailable: "+map_layout_error_;return;}
    const auto result=map_monitor_.update(
        [&](u64 a,void* bytes,u32 n){return get(a,bytes,n);},
        [&](u64 a,void* bytes,u32 n){return fresh(a,bytes,n);},binding_.engine,now_ms());
    const auto next=result.ready()?result.map:std::string{};
    if(next!=detected_map_||result.header.client!=map_client_||result.header.spawn!=map_spawn_){cached_world_.clear();world_time_=0;}
    detected_map_=next;map_resource_=result.raw_full;map_client_=result.header.client;map_spawn_=result.header.spawn;
    f.map_name=next;f.map_status=result.ready()?"Automatic map: "+next:"Automatic map: "+result.detail;
    const auto stage=std::to_string(static_cast<int>(result.stage))+"|"+next+"|"+std::to_string(result.header.state)+"|"+std::to_string(map_client_)+"|"+std::to_string(map_spawn_);
    const auto now=now_ms();
    if(stage!=map_probe_stage_||now-map_probe_report_ms_>=10000){map_probe_stage_=stage;map_probe_report_ms_=now;
        plat_log("Automatic map identity: source=engine2 network client stage=%s map=%s client=0x%llX service_vtable_rva=0x%llX client_vtable_rva=0x%llX signon=%d spawn=%u full=%s short=%s name_host_age=%lld ms",engine_map::stage_text(result.stage),next.empty()?"<unavailable>":next.c_str(),(unsigned long long)result.header.client,(unsigned long long)(result.header.service_vtable>=binding_.engine?result.header.service_vtable-binding_.engine:0),(unsigned long long)(result.header.client_vtable>=binding_.engine?result.header.client_vtable-binding_.engine:0),result.header.state,result.header.spawn,result.raw_full.empty()?"<unavailable>":result.raw_full.c_str(),result.raw_short.empty()?"<unavailable>":result.raw_short.c_str(),result.name_sampled_ms&&now>=result.name_sampled_ms?(long long)(now-result.name_sampled_ms):-1LL);
    }
}
void Client::sample_native_joints(Frame& f){
    if(!native_joint_bank_)native_joint_bank_=std::make_unique<native_joint_backend::Bank>(mem);
    std::vector<native_joint_backend::Work> work;
    profile_scope::ActiveScope hero_settings(f.local);
    // Activation/toggle/panic facts originate in real Aim/Trigger ticks. Current
    // frame state can additionally suppress demand, and never authorizes input.
    action_pause_policy::Observations observed;
    const auto observed_at=now_ms();
    observed.managed=true;observed.snapshot_valid=f.visuals_ready();observed.input_valid=f.input_valid;
    observed.fresh=f.time&&observed_at>=f.time&&observed_at-f.time<=250;observed.in_match=f.in_match;
    observed.life_known=f.local.alive_valid;observed.dead=f.dead;
    observed.scoreboard=f.scoreboard;observed.spectator=f.spectating;
    // Frame.performance is copied only by finish(), after this method. Use the
    // current poll's raw observation rather than the preceding public snapshot.
    observed.raw_ui_readable=f.ui_valid;observed.raw_ui_flags=performance_.raw_ui_flags;
    observed.ace_menu_open=snap.overlay_menu.load();
    const bool action_ready=f.attached&&f.ui_valid&&f.clock_valid&&f.input_valid&&f.in_match&&
        f.local.valid&&f.local.weapon_valid&&!f.local.disarmed&&!f.local.reloading&&
        action_pause_policy::evaluate(configured_action_pause_preferences(),observed)==action_pause_policy::Reason::Ready;
    const auto actions=action_ready?native_joint_demand::action_requests(f.native_binding_epoch,now_ms()):std::array<native_joint_demand::Request,3>{};
    auto append=[&](std::vector<Entity>& values){for(auto& e:values){
        e.native_joints={};const auto found=identities_.find(e.handle&0x7fff);
        if(found==identities_.end()||found->second.handle!=e.handle||found->second.entity!=e.base||found->second.kind!=1)continue;
        unsigned requested=0;const bool player=e.cls==EClass::Player;
        const auto distance=f.local.pos.dist(e.pos);
        if(player&&e.valid&&std::isfinite(distance)){
            {Settings::VisualScope side(e.team==f.local.team);
                requested|=native_joint_demand::visual_head(cfg.B("visuals.enabled")&&cfg.B("cls.players")&&cfg.B("esp.enabled"),
                    player,e.valid,world_units::units((float)cfg.I("esp.draw_distance")),distance);}
            if(e.alive&&e.modifiers_valid&&!e.stealthed&&!e.invuln&&!e.bullet_invuln&&
               (!cfg.B("target.team_check")||e.team!=f.local.team))requested|=native_joint_demand::at_distance(actions,distance);
        }
        const auto& id=found->second;
        // Each joint is an independent acceptance row, batched together. A
        // missing Neck must not erase a valid Head marker on the same actor.
        if(!requested)work.push_back({&e,{id.entity,id.identity,id.handle},0});
        for(unsigned bit:{unsigned(native_joints::head),unsigned(native_joints::neck)})if(requested&bit)
            work.push_back({&e,{id.entity,id.identity,id.handle},static_cast<native_joints::Demand>(bit)});
    }};
    append(f.enemies);append(f.friendlies);
    std::sort(work.begin(),work.end(),[](const auto& a,const auto& b){return a.target.expected_handle<b.target.expected_handle;});
    f.native_joint_performance=native_joint_bank_->sample(binding_.client,f.native_binding_epoch,work);
    const auto now=now_ms();if(f.native_joint_performance.demanded&&now-native_joint_log_>=5000){native_joint_log_=now;
        const auto& s=f.native_joint_performance;
        plat_log("Native cached joints: demanded_rows=%u attempts=%u accepted_rows=%u warmups=%u warm_rows=%u cold_rows=%u plan_discards=%u budget_skips=%u backoff_skips=%u requested_host_bytes=%llu requests=%llu fresh_batches=%llu elapsed_ms=%llu first_failure=%s; native_age=unknown",s.demanded,s.attempted,s.accepted,s.warmups,s.warm_rows,s.cold_rows,s.plan_discards,s.budget_skipped,s.backoff_skipped,(unsigned long long)s.bytes,(unsigned long long)s.requests,(unsigned long long)s.fresh_calls,(unsigned long long)s.elapsed_ms,native_joint_backend::failure_text(s.first_failure));
    }
}

Frame Client::poll(int scoreboard_key){
    Settings::ReadScope reader_settings(cfg,true);
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Frame f;f.game_id=games::DEADLOCK.cfg_prefix;f.game_revision=L::revision;f.world_units_per_metre=world_units::per_metre;f.actions_supported=true;f.time=now_ms();f.native_binding_epoch=mem.attachment_epoch();if(!binding_.client){f.status="No matching game layout";return f;}f.attached=true;
    frame_cache_.clear();performance_={};cache_active_=true;local_base_=0;helper_details_={};bool frame_acquired=false;
    auto finish=[&](){
        // Late camera sample avoids retaining the beginning-of-poll matrix
        // through dependent entity/ability reads. Frame.time still identifies
        // the oldest entity acquisition; never freshen it to conceal a stall.
        std::array<float,16> latest_view{};const auto matrix_started=now_ms();
        f.matrix_valid=fresh(binding_.client+L::view_rva,latest_view.data(),sizeof(latest_view))&&matrix_valid(latest_view.data());
        if(f.matrix_valid){
            f.view=latest_view;f.matrix_sample_ms=matrix_started;
            if(!have_last_view_||std::memcmp(last_view_.data(),latest_view.data(),sizeof(latest_view))){last_view_change_=matrix_started;last_view_=latest_view;have_last_view_=true;}
            f.matrix_change_ms=last_view_change_;
        }
        // This status describes the final camera sample, not the earlier
        // batched matrix used only to admit this poll's joint observations.
        if(frame_acquired)f.status=!performance_.raw_state_valid?"Game state unavailable":(!f.input_valid?"Game input unavailable":(!f.ui_valid?"Game UI state unavailable":(!f.matrix_valid?"View matrix unavailable":(!f.visibility_valid?"Team visibility unavailable":(f.in_match?"Live batched data / team visibility (FOW)":"Match inactive or paused")))));
        cache_active_=false;performance_.cache_hits=frame_cache_.counters.hits;performance_.phase_ms[7]=now_ms()-f.time;f.read_failures=(unsigned)performance_.failed_ranges;f.performance=performance_;f.discovery=discovery_;f.entities=(unsigned)identities_.size();if(now_ms()-performance_log_>=5000){performance_log_=now_ms();plat_log("Reader batches: global=%llu references=%llu vectors=%llu ability_headers=%llu ability_details=%llu auxiliary=%llu ability_parse_ballistics=%llu total=%llu ms | scatter=%llu requests=%llu direct=%llu hits=%llu failed_ranges=%llu local_reject=%s ability_players=%u skipped_ability_players=%u",(unsigned long long)performance_.phase_ms[0],(unsigned long long)performance_.phase_ms[1],(unsigned long long)performance_.phase_ms[2],(unsigned long long)performance_.phase_ms[3],(unsigned long long)performance_.phase_ms[4],(unsigned long long)performance_.phase_ms[5],(unsigned long long)performance_.phase_ms[6],(unsigned long long)performance_.phase_ms[7],(unsigned long long)performance_.scatter_calls,(unsigned long long)performance_.scatter_requests,(unsigned long long)performance_.direct_reads,(unsigned long long)performance_.cache_hits,(unsigned long long)performance_.failed_ranges,performance_.local_rejection.empty()?"none":performance_.local_rejection.c_str(),performance_.ability_players,performance_.ability_players_skipped);plat_log("Player health source: handle=%08X pawn_current=%d base_max=%d controller_max=%d controller_current=%d life=%u global_readable=%d pair_valid=%d source=%s current_valid=%d max_valid=%d alive_valid=%d health_note=%s",performance_.local_health_handle,performance_.local_health,performance_.local_base_max,performance_.local_global_max,performance_.local_global_health,performance_.local_life,performance_.local_global_readable,performance_.local_health_pair_valid,health_state::source_name(performance_.local_health_source),performance_.local_health_current_valid,performance_.local_health_max_valid,performance_.local_alive_valid,health_state::reason_name(performance_.local_health_reason));plat_log("Raw output gates: ui_flags=0x%02X ui_valid=%d game_state=%d state_valid=%d server_paused=%d pause_valid=%d in_match=%d ui_mask03_active=%d menu_visibility=unverified",performance_.raw_ui_flags,performance_.raw_ui_valid,performance_.raw_game_state,performance_.raw_state_valid,performance_.raw_paused,performance_.raw_pause_valid,f.in_match,f.menu);}return f;};
    // Cold start only. Once populated, bounded discovery maintenance belongs
    // after publish, not inside the critical frame's sampling window.
    if(identities_.empty()){auto before=now_ms();scan();f.scan_ms=now_ms()-before;}
    std::vector<const Identity*> roots,players,auxiliary;for(const auto& pair:identities_){const auto& id=pair.second;if(id.kind==1)players.push_back(&id);if(id.kind>=1&&id.kind<=3)roots.push_back(&id);}
    // Dynamic NPC/projectile work is bounded independently of player state.
    std::vector<const Identity*> candidates;profile_subject::Subject previous_subject;{std::lock_guard<std::mutex> guard(snap_mtx);previous_subject=snap.local.settings_subject;}
    bool npc_needed=false;{profile_scope::ActiveScope scope(previous_subject);npc_needed=(cfg.B("visuals.enabled")&&cfg.B("minions.enabled"))||(native_target::supports_entity(EClass::NPC)&&(cfg.B("aim.enabled")||cfg.B("trigger.enabled"))&&(cfg.B("aim.target_minions")||cfg.B("aim.target_neutrals")));}
    const bool world_display=cfg.B("visuals.enabled")&&cfg.B("world.enabled")&&cfg.B("cls.world");
    const bool projectiles_needed=(cfg.B("visuals.enabled")&&cfg.B("world.enabled")&&cfg.B("cls.projectiles"))||cfg.B("defense.counterspell.enabled")||cfg.B("defense.11.enabled")||cfg.B("defense.63.enabled")||cfg.B("defense.77.enabled");
    for(const auto& pair:identities_)if((pair.second.kind==5&&npc_needed)||(pair.second.kind==6&&projectiles_needed))candidates.push_back(&pair.second);
    std::sort(candidates.begin(),candidates.end(),[](const Identity* a,const Identity* b){return a->handle<b->handle;});if(!candidates.empty()){const auto count=std::min<std::size_t>(64,candidates.size());for(std::size_t i=0;i<count;++i)auxiliary.push_back(candidates[(auxiliary_cursor_+i)%candidates.size()]);auxiliary_cursor_=(auxiliary_cursor_+count)%candidates.size();}
    const bool refresh_world=world_display!=world_display_requested_||!world_time_||f.time-world_time_>(u64)std::clamp(cfg.I("reader.world_ms"),250,5000);
    if(refresh_world){world_time_=f.time;world_display_requested_=world_display;}
    if(refresh_world)for(const auto& pair:identities_)if((pair.second.kind==7&&world_display)||pair.second.kind==9)auxiliary.push_back(&pair.second);
    roots.insert(roots.end(),auxiliary.begin(),auxiliary.end());prefetch_roots(roots);
    // Independent engine roots share the existing first uncached batch.
    if(map_layout_ready_){queue(binding_.engine+engine_map_layout::service_instance_rva,8);queue(binding_.engine+engine_map_layout::active_client_global_rva,8);}
    queue(binding_.engine+L::engine_ui+L::engine_ui_flags,1);queue(binding_.input+L::input_instance+0x48,1);queue(binding_.input+L::input_instance+L::input_buttons,64);queue(binding_.client+L::globals_rva,8);queue(binding_.client+L::rules_rva,8);queue(binding_.client+L::view_rva,(u32)sizeof(f.view));flush(0);
    // Validate an actual current-poll matrix before native action-demand
    // evaluation. finish() still reacquires the camera after ALL dependent
    // work and supplies the only published matrix timestamps.
    f.matrix_valid=get(binding_.client+L::view_rva,f.view.data(),(u32)sizeof(f.view))&&matrix_valid(f.view.data());
    unsigned char flags=0,enabled=0;std::array<u32,16> buttons{};f.ui_valid=get(binding_.engine+L::engine_ui+L::engine_ui_flags,flags);performance_.raw_ui_flags=flags;performance_.raw_ui_valid=f.ui_valid;f.menu=f.ui_valid&&(flags&3)!=0; // Unverified UI signal; never claims Escape visibility.
    if(!keymap_ready_&&f.time-keymap_retry_>=5000)load_keymap(binding_,false);
    const bool enabled_read=get(binding_.input+L::input_instance+0x48,enabled);
    performance_.raw_input_enabled=enabled;performance_.raw_input_enabled_valid=enabled_read;
    f.input_valid=keymap_ready_&&enabled_read&&enabled==1&&get(binding_.input+L::input_instance+L::input_buttons,buttons.data(),sizeof(buttons));
    if(f.input_valid){for(int vk=1;vk<256;vk++){int code=key_codes_[vk];if(code>0&&code<512)f.keys[vk]=(buttons[code/32]&(1u<<(code%32)))!=0;}f.keys[VK_SHIFT]=f.keys[VK_LSHIFT]||f.keys[VK_RSHIFT];f.keys[VK_CONTROL]=f.keys[VK_LCONTROL]||f.keys[VK_RCONTROL];f.keys[VK_MENU]=f.keys[VK_LMENU]||f.keys[VK_RMENU];}
    f.scoreboard=scoreboard_key>0&&scoreboard_key<256&&f.keys[scoreboard_key];
    u64 globals=0,rules=0;if(get(binding_.client+L::globals_rva,globals))queue(globals,0x48);if(get(binding_.client+L::rules_rva,rules)){queue(rules+L::C_CitadelGameRules::m_eGameState,4);queue(rules+L::C_CitadelGameRules::m_bServerPaused,1);}
    auto current_identity=[&](const Identity& id){u64 record=0;u32 serial=0,identity_flags=0;return get(id.entity+L::CEntityInstance::m_pEntity,record)&&record==id.identity&&get(record+0x10,serial)&&serial==id.handle&&get(record+0x30,identity_flags)&&!(identity_flags&0x717);};
    const Identity* local_controller=nullptr;std::unordered_map<u32,const Identity*> pawn_controllers;
    for(const auto& pair:identities_)if(pair.second.kind==2){const auto& controller=pair.second;if(!current_identity(controller))continue;unsigned char is_local=0;u32 handle=0;if(get(controller.entity+L::CCitadelPlayerController::m_hHeroPawn,handle))pawn_controllers.emplace(handle,&controller);if(get(controller.entity+L::CBasePlayerController::m_bIsLocalPlayerController,is_local)&&is_local==1){if(local_controller){performance_.local_rejection="ambiguous local controller";f.status=performance_.local_rejection;return finish();}local_controller=&controller;if(handle)pawn_controllers[handle]=&controller;}}
    if(local_controller){u32 handle=0;if(get(local_controller->entity+L::CCitadelPlayerController::m_hHeroPawn,handle))local_base_=resolve(handle);}
    if(map_layout_ready_){u64 service=0,client=0;
        if(get(binding_.engine+engine_map_layout::service_instance_rva,service)&&service==binding_.engine+engine_map_layout::service_expected_vtable_rva&&get(binding_.engine+engine_map_layout::active_client_global_rva,client)&&engine_map::span(client,0x248)){
            queue(client,8);queue(client+engine_map_layout::full_map_pointer_offset,0x38);
        }
    }
    prefetch_dependencies(roots);flush(1);
    int frame=-1,state=0;unsigned char paused=1;f.clock_valid=valid_pointer(globals)&&get(globals+0x30,f.game_time)&&get(globals+4,frame)&&frame>=0&&std::isfinite(f.game_time)&&f.game_time>=0&&f.game_time<172800;
    if(f.clock_valid){if(last_frame_!=frame){last_frame_=frame;last_clock_change_=f.time;}if(f.time-last_clock_change_>500)f.clock_valid=false;}
    const bool state_valid=valid_pointer(rules)&&get(rules+L::C_CitadelGameRules::m_eGameState,state);const bool pause_valid=state_valid&&get(rules+L::C_CitadelGameRules::m_bServerPaused,paused)&&paused<=1;
    performance_.raw_game_state=state;performance_.raw_state_valid=state_valid;performance_.raw_paused=paused;performance_.raw_pause_valid=pause_valid;
    detect_map(f);
    if(identities_.empty()){performance_.local_rejection="entity discovery pending";f.status="Waiting for entity system: "+discovery_.status;return finish();}
    if(!local_controller){performance_.local_rejection="local controller flag unavailable";f.status="Waiting for local controller";return finish();}
    if(!local_base_){performance_.local_rejection="hero handle absent or serial mismatch";f.status="Waiting for local hero pawn";return finish();}
    // Vector phase groups visibility and ability-handle arrays across ALL players.
    u32 local_team=0;const bool local_team_known=get(local_base_+L::C_BaseEntity::m_iTeamNum,(unsigned char&)local_team);
    struct PublicUltimate {float start=0,end=0,stopped=0;bool trained_read=false,trained=false,valid=false;};
    std::unordered_map<u32,PublicUltimate> public_ultimates;
    for(const auto& pair:pawn_controllers){
        const auto pawn=identities_.find(pair.first&0x7fff);if(pawn==identities_.end()||pawn->second.handle!=pair.first||pawn->second.kind!=1)continue;
        const auto data=pair.second->entity+L::CCitadelPlayerController::m_PlayerDataGlobal;auto& value=public_ultimates[pair.first];unsigned char trained=0;
        value.trained_read=get(data+L::PlayerDataGlobal_t::m_bUltimateTrained,trained)&&trained<=1;value.trained=trained==1;
        const bool readable=value.trained_read&&get(data+L::PlayerDataGlobal_t::m_flUltimateCooldownStart,value.start)&&get(data+L::PlayerDataGlobal_t::m_flUltimateCooldownEnd,value.end)&&get(data+L::PlayerDataGlobal_t::m_flUltimateStoppedCooldownRemaining,value.stopped);
        value.valid=readable&&std::isfinite(value.start)&&std::isfinite(value.end)&&std::isfinite(value.stopped)&&value.start>=0&&value.end>=0&&value.stopped>=0&&value.start<172800&&value.end<172800&&value.stopped<172800;
    }
    bool enemy_ultimate=false,team_ultimate=false;
    ability_read_demand::Consumers consumers;consumers.visuals=cfg.B("visuals.enabled");
    {Settings::VisualScope enemy(false);consumers.enemy_esp=cfg.B("esp.enabled");consumers.enemy_icons=cfg.B("esp.abilities");enemy_ultimate=cfg.B("esp.ultimate");}
    {Settings::VisualScope team(true);consumers.team_esp=cfg.B("esp.enabled");consumers.team_icons=cfg.B("esp.abilities");team_ultimate=cfg.B("esp.ultimate");}
    u32 actual_hero=0;if(local_controller)get(local_controller->entity+L::CCitadelPlayerController::m_PlayerDataGlobal+L::PlayerDataGlobal_t::m_nHeroID,actual_hero);
    const auto demand_now=now_ms();
    const bool automatic_running=consumer_demand::automatic_running(demand_now),defensive_running=consumer_demand::defensive_running(demand_now);
    // Acquisition demand only; these fields never authorize an action. Reset
    // every poll so disabling/retiring a dispatcher cannot replay old details.
    helper_details_.automatic=automatic_running&&consumer_demand::automatic(cfg,actual_hero);
    helper_details_.active_reload=helper_details_.automatic&&cfg.B("auto.active_reload");
    helper_details_.defensive=defensive_running&&consumer_demand::defensive(cfg,actual_hero);
    const bool action_details=helper_details_.automatic||helper_details_.defensive;
    consumers.enemy_actions=consumers.team_actions=action_details;
    std::vector<const Identity*> ability_players;std::unordered_set<u32> ability_pawns;
    for(const auto* player:players){unsigned char team=0;const bool team_known=local_team_known&&get(player->entity+L::C_BaseEntity::m_iTeamNum,team);
        const auto public_state=public_ultimates.find(player->handle);const bool public_valid=public_state!=public_ultimates.end()&&public_state->second.valid;
        const bool ultimate_fallback=(team==local_team?team_ultimate:enemy_ultimate)&&!public_valid;
        if(ability_read_demand::required(consumers,player->entity==local_base_,team_known,team==local_team,ultimate_fallback)){ability_players.push_back(player);ability_pawns.insert(player->handle);}
    }
    performance_.ability_players=(unsigned)ability_players.size();performance_.ability_players_skipped=(unsigned)(players.size()-ability_players.size());
    // Only ability details are omitted. Player poses, modifier safety masks,
    // identity checks, FOW and public ultimate state still update every poll.
    prefetch_abilities(ability_players);std::vector<RemoteVector> visibility_vectors;
    for(const auto& pair:identities_)if(pair.second.kind==3){if(!current_identity(pair.second))continue;unsigned char team=0;RemoteVector v{};if(get(pair.second.entity+L::C_BaseEntity::m_iTeamNum,team)&&team==local_team&&get(pair.second.entity+L::C_CitadelTeam::m_vecFOWEntities,v)&&v.count>=0&&v.count<=32768&&v.max>=v.count&&v.max<=65536&&(!v.count||valid_pointer(v.data))){visibility_vectors.push_back(v);if(v.count)queue(v.data,u32(v.count)*L::STeamFOWEntity::size);}}
    flush(2);std::unordered_map<u32,bool> visibility;
    for(const auto& v:visibility_vectors){std::vector<unsigned char> raw(v.count*L::STeamFOWEntity::size);if(!raw.empty()&&!get(v.data,raw.data(),u32(raw.size())))continue;f.visibility_valid=true;for(int i=0;i<v.count;i++){u32 index=0;unsigned char visible=raw[i*L::STeamFOWEntity::size+L::STeamFOWEntity::m_bVisibleOnMap];std::memcpy(&index,raw.data()+i*L::STeamFOWEntity::size+L::STeamFOWEntity::m_nEntIndex,4);if(index<32768&&visible<=1)visibility[index]=visible!=0;}}
    prefetch_ability_details(ability_players);flush(3);
    // Only live helper consumers require this final dependent scatter. With
    // all helper demand off, phase 3 has already executed every queued span;
    // flush(4) therefore has zero requests and issues no ExecuteRead call.
    if(helper_details_.automatic||helper_details_.defensive)for(const auto& pair:identities_)if(pair.second.kind==4&&frame_cache_.contains(pair.second.entity,0x7e0)){const auto base=pair.second.entity;const auto& name=pair.second.name;
        if(helper_details_.active_reload&&name.find("PrimaryWeapon")!=std::string::npos){u64 data=0;if(get(base+0x390,data))queue(data+0x10e8,8);}
        if(helper_details_.defensive&&name=="CCitadel_Ability_Lash_Ultimate"){RemoteVector v{};if(get(base+L::CCitadelBaseLockonAbility::m_vecLockonTargets,v)&&bounded_vector(v,64)&&v.count)queue(v.data,u32(v.count)*L::LockonTarget_t::size);}
        if(helper_details_.automatic){u32 token=0;if(get(base+L::C_BaseEntity::m_nSubclassID,token)&&token==754480263u){RemoteVector imbued{};if(get(base+L::C_CitadelBaseAbility::m_vecImbuedAbilities,imbued)&&bounded_vector(imbued,4)&&imbued.max<=16&&imbued.count)queue(imbued.data,u32(imbued.count)*4);}}
    }flush(4);
    std::unordered_map<u32,bool> trained_by_pawn;
    for(const auto& pair:pawn_controllers){
        const auto pawn=identities_.find(pair.first&0x7fff);
        if(pawn==identities_.end()||pawn->second.handle!=pair.first||pawn->second.kind!=1)continue;
        const auto& controller=*pair.second;
        const auto data=controller.entity+L::CCitadelPlayerController::m_PlayerDataGlobal;
        // Both health values and public ultimate data are covered by phase 0.
        // The controller was linked by its current serial-validated hero handle.
        health_state::PlayerMaximum maximum;
        maximum.readable=get(data+L::PlayerDataGlobal_t::m_iHealthMax,maximum.maximum);maximum.controller_readable=get(data+L::PlayerDataGlobal_t::m_iHealth,maximum.controller_health);
        Entity e;if(!entity(pawn->second,e,&maximum))continue;e.host_sample_ms=f.time;
        const bool hero_read=get(data+L::PlayerDataGlobal_t::m_nHeroID,e.hero);
        if(hero_read)if(const auto* hero=current_hero(e.hero))e.settings_subject={profile_subject::Kind::Hero,games::DEADLOCK.cfg_prefix,hero->name,{},true};
        int souls=0;get(data+L::PlayerDataGlobal_t::m_iGoldNetWorth,souls);e.souls=std::max(0,souls);
        get(controller.entity+L::CBasePlayerController::m_steamID,e.steam_id);
        const auto public_state=public_ultimates.find(e.handle);
        if(public_state!=public_ultimates.end()){const auto& value=public_state->second;
            e.public_ultimate_trained=value.trained_read&&value.trained;e.public_ultimate_valid=value.valid;
            e.public_ultimate_cooldown_start=value.start;e.public_ultimate_cooldown_end=value.end;e.public_ultimate_stopped_remaining=value.stopped;
            e.public_ultimate_sample_ms=value.valid?f.time:0;trained_by_pawn[e.handle]=value.trained_read&&value.trained;
        }
        e.visible=e.team==local_team||(f.visibility_valid&&visibility[e.idx]);e.visibility_known=f.visibility_valid||e.team==local_team;
        if(e.base==local_base_)f.local=std::move(e);else if(e.team==local_team)f.friendlies.push_back(std::move(e));else f.enemies.push_back(std::move(e));
    }
    if(!f.local.base||!f.clock_valid){if(performance_.local_rejection.empty())performance_.local_rejection=f.clock_valid?"local entity not accepted":"game clock unavailable/frozen";f.status=performance_.local_rejection;return finish();}
    get(f.local.base+L::C_BasePlayerPawn::m_flMouseSensitivity,f.sensitivity);get(f.local.base+L::C_BasePlayerPawn::m_flFOVSensitivityAdjust,f.fov_sensitivity);if(!std::isfinite(f.sensitivity)||f.sensitivity<=0||f.sensitivity>100)f.sensitivity=0;if(!std::isfinite(f.fov_sensitivity)||f.fov_sensitivity<=0||f.fov_sensitivity>10)f.fov_sensitivity=1;
    const auto abilities_started=now_ms();abilities(f.local,f.game_time,trained_by_pawn[f.local.handle],true);for(auto& e:f.enemies)if(ability_pawns.count(e.handle))abilities(e,f.game_time,trained_by_pawn[e.handle]);for(auto& e:f.friendlies)if(ability_pawns.count(e.handle))abilities(e,f.game_time,trained_by_pawn[e.handle]);performance_.phase_ms[6]=now_ms()-abilities_started;
    std::unordered_map<u64,const Identity*> by_address;for(const auto& pair:identities_)by_address.emplace(pair.second.entity,&pair.second);
    const auto aux_start=now_ms();std::vector<Entity> fresh_world;for(const auto* id:auxiliary){Entity e;if(!entity(*id,e))continue;++performance_.auxiliary_sampled;if(e.cls==EClass::Projectile){if(e.owner){auto source=by_address.find(e.owner);if(source!=by_address.end()&&source->second->kind==4&&current_identity(*source->second)){u32 owner=0;if(get(e.owner+L::C_BaseEntity::m_hOwnerEntity,owner))e.owner=resolve(owner);}}f.projectiles.push_back(std::move(e));}else if(e.cls==EClass::NPC)f.npcs.push_back(std::move(e));else fresh_world.push_back(std::move(e));}performance_.phase_ms[5]=now_ms()-aux_start;
    if(refresh_world){cached_world_=std::move(fresh_world);world_time_=f.time;}f.world=cached_world_;u64 observer=0;unsigned char mode=0;if(get(f.local.base+L::C_BasePlayerPawn::m_pObserverServices,observer)&&valid_pointer(observer)&&get(observer+L::CPlayer_ObserverServices::m_iObserverMode,mode))f.spectating=mode!=0;
    f.in_match=state_valid&&pause_valid&&(state==6||state==7)&&!paused;// Match state is a separate mandatory gate, not a menu-visibility observation.
    f.dead=f.local.alive_valid&&!f.local.alive;frame_acquired=true;
    sample_native_joints(f);
    return finish();
}

}

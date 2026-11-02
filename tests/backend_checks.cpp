#include "consumer_demand.h"
#include "live_backend.h"
#include "aim_model.h"
#include "ability_range.h"
#include "autouse.h"
#include "collision.h"
#include "deadlock_layout.h"
#include "verified_input_keymap.h"
static_assert(verified_input_keymap::virtual_keys[VK_TAB]==68);
static_assert(verified_input_keymap::virtual_keys['A']==11&&verified_input_keymap::virtual_keys['Z']==36);
#include "ability_catalog.h"
#include "defense.h"
#include "config.h"
#include "km_protocol.h"
#include <map>
#include <array>
#include <cassert>
#include <cstring>
#include <cstdio>
namespace L=dl_layout;
struct Memory{
    std::map<u64,std::array<unsigned char,4096>> pages;
    void zero(u64 address,size_t size){for(u64 p=address&~4095ULL;p<address+size;p+=4096)pages[p]={};}
    void put(u64 address,const void* data,size_t size){const auto* bytes=(const unsigned char*)data;for(size_t i=0;i<size;i++)pages[(address+i)&~4095ULL][(address+i)&4095]=bytes[i];}
    template<class T>void put(u64 address,T value){put(address,&value,sizeof(value));}
    bool read(u64 address,void* data,u32 size){auto* out=(unsigned char*)data;for(u32 i=0;i<size;i++){auto found=pages.find((address+i)&~4095ULL);if(found==pages.end())return false;out[i]=found->second[(address+i)&4095];}return true;}
};
static const L::Class& type(const char* name,int kind){for(auto& c:L::classes)if(std::strcmp(c.name,name)==0)return c;for(auto& c:L::classes)if(c.kind==kind)return c;std::abort();}
static const ability_data::Entry& named(const char* key){for(auto& e:ability_data::entries)if(std::strcmp(e.key,key)==0)return e;std::abort();}
struct Fixture{
    Memory memory;live_backend::Binding binding{0x10000000,0x20000000,0x30000000};
    u64 system=0x40000000,chunk=0x40010000,second=0x40030000,rules=0x41000000,key_state=0;
    static u32 handle(u32 i){return (1u<<15)|i;}static u64 object(u32 i){return 0x50000000ULL+i*0x10000;}
    void pe(u64 base,u32 timestamp,u32 size){memory.zero(base,4096);IMAGE_DOS_HEADER dos{};dos.e_magic=IMAGE_DOS_SIGNATURE;dos.e_lfanew=0x80;memory.put(base,dos);IMAGE_NT_HEADERS64 nt{};nt.Signature=IMAGE_NT_SIGNATURE;nt.FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;nt.FileHeader.TimeDateStamp=timestamp;nt.OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;nt.OptionalHeader.SizeOfImage=size;memory.put(base+0x80,nt);}
    void instruction(u32 source,u32 target,const std::array<unsigned char,3>& prefix){unsigned char bytes[7]{};std::memcpy(bytes,prefix.data(),3);int32_t displacement=(int32_t)target-(int32_t)source-7;std::memcpy(bytes+3,&displacement,4);memory.put(binding.client+source,bytes,7);}
    void add(u32 index,const char* name,int kind){u64 address=object(index),identity=(index<512?chunk:second)+(index%512)*0x70;memory.zero(address,0x4000);memory.put(address,binding.client+type(name,kind).vtable);memory.put(address+L::CEntityInstance::m_pEntity,identity);memory.put(identity,address);memory.put(identity+0x10,handle(index));}
    void pawn(u32 index,unsigned char team,Vec3 position){
        add(index,"C_CitadelPlayerPawn",1);auto address=object(index);u64 scene=address+0x8000,modifiers=address+0xa000;memory.zero(scene,0x1000);memory.zero(modifiers,0x5000);memory.put(scene+L::CGameSceneNode::m_flAbsScale,1.f);
        memory.put(address+L::C_BaseEntity::m_pGameSceneNode,scene);memory.put(scene+L::CGameSceneNode::m_vecAbsOrigin,position);memory.put(address+L::C_BaseEntity::m_iHealth,int32_t(100));memory.put(address+L::C_BaseEntity::m_iMaxHealth,int32_t(100));memory.put(address+L::C_BaseEntity::m_iTeamNum,team);memory.put(address+L::C_BaseEntity::m_flSimulationTime,100.f);memory.put(address+L::C_BaseEntity::m_pModifierProp,modifiers);
        u64 collision=address+0x9000;memory.zero(collision,4096);memory.put(address+L::C_BaseEntity::m_pCollision,collision);memory.put(collision+0x40,Vec3{-16,-16,0});memory.put(collision+0x4c,Vec3{16,16,72});
    }
    void controller(u32 index,u32 pawn_index,u32 hero,bool local){add(index,"CCitadelPlayerController",2);auto address=object(index);memory.put(address+L::CBasePlayerController::m_bIsLocalPlayerController,(unsigned char)local);memory.put(address+L::CCitadelPlayerController::m_hHeroPawn,handle(pawn_index));auto data=address+L::CCitadelPlayerController::m_PlayerDataGlobal;memory.put(data+L::PlayerDataGlobal_t::m_nHeroID,hero);memory.put(data+L::PlayerDataGlobal_t::m_iHealthMax,int32_t(100));memory.put(data+L::PlayerDataGlobal_t::m_iHealth,int32_t(100));memory.put(data+L::PlayerDataGlobal_t::m_bUltimateTrained,(unsigned char)1);}
    void ability(u32 index,const char* key,int16_t slot,const char* type_name="C_CitadelBaseAbility"){
        add(index,type_name,4);auto address=object(index);memory.put(address+L::C_BaseEntity::m_nSubclassID,named(key).token);memory.put(address+L::C_CitadelBaseAbility::m_eAbilitySlot,slot);memory.put(address+L::C_CitadelBaseAbility::m_nUpgradeInfo,u32(1));
    }
    void abilities(u32 pawn_index,std::initializer_list<u32> entries,u64 data){
        struct Vector{int count,pad;u64 data;int max,pad2;}v{(int)entries.size(),0,data,(int)entries.size(),0};memory.zero(data,4096);unsigned i=0;for(auto e:entries)memory.put(data+4*i++,handle(e));memory.put(object(pawn_index)+L::C_CitadelPlayerPawn::m_CCitadelAbilityComponent+L::CCitadelAbilityComponent::m_vecAbilities,v);
    }
    Fixture(u64 chunk_bias=0){
        chunk+=chunk_bias;second+=chunk_bias;
        pe(binding.client,L::client_timestamp,L::client_image_size);pe(binding.engine,L::engine2_timestamp,L::engine2_image_size);pe(binding.input,L::inputsystem_timestamp,L::inputsystem_image_size);
        instruction(L::globals_instruction,L::globals_rva,{0x48,0x8b,0x05});memory.zero(0x42000000,4096);memory.put(binding.client+L::globals_rva,u64(0x42000000));memory.put(0x42000030,100.f);memory.put(0x42000004,int(100));
        instruction(L::entities_instruction,L::entities_rva,{0x48,0x8b,0x1d});instruction(L::view_instruction,L::view_rva,{0x48,0x8d,0x0d});instruction(L::rules_instruction,L::rules_rva,{0x48,0x89,0x1d});
        memory.zero(binding.input+L::input_instance,0x3000);memory.put(binding.input+L::input_instance,binding.input+L::input_vtable);memory.put(binding.input+L::input_instance+0x48,(unsigned char)1);
        memory.zero(binding.engine+L::engine_ui,0x100);memory.put(binding.engine+L::engine_ui,binding.engine+L::engine_ui_vtable);memory.zero(binding.input+L::virtual_key_table,1024);
        memory.put(binding.input+L::virtual_key_table,verified_input_keymap::virtual_keys.data(),sizeof(verified_input_keymap::virtual_keys));
        memory.put(binding.input+L::input_instance+L::input_buttons+(110/32)*4,u32(1u<<(110%32)));
        memory.zero(system,0x1000);memory.zero(chunk,512*0x70);memory.zero(second,512*0x70);memory.put(binding.client+L::entities_rva,system);memory.put(system+0x10,chunk);memory.put(system+0x18,second);
        memory.zero(rules,0xb000);memory.put(binding.client+L::rules_rva,rules);memory.put(rules+L::C_CitadelGameRules::m_eGameState,int(7));
        std::array<float,16> matrix{};matrix[0]=matrix[6]=.01f;matrix[15]=1;memory.put(binding.client+L::view_rva,matrix);
        pawn(2,1,{0,0,0});controller(1,2,11,true);pawn(600,2,{20,0,0});controller(3,600,31,false);
        add(5,"C_CitadelTeam",3);memory.put(object(5)+L::C_BaseEntity::m_iTeamNum,(unsigned char)1);
        struct Vector{int count,pad;u64 data;int max,pad2;}fow{1,0,0x82000000,1,0};memory.zero(fow.data,L::STeamFOWEntity::size);memory.put(fow.data+L::STeamFOWEntity::m_nEntIndex,u32(600));memory.put(fow.data+L::STeamFOWEntity::m_bVisibleOnMap,(unsigned char)1);memory.put(object(5)+L::C_CitadelTeam::m_vecFOWEntities,fow);
        ability(6,"citadel_ability_void_sphere",1);ability(7,"citadel_ability_melee_parry",22);ability(8,"upgrade_counterspell",4,"CCitadel_Item");abilities(2,{6,7,8},0x83000000);
        ability(9,"citadel_ability_lash_ultimate",3,"CCitadel_Ability_Lash_Ultimate");abilities(600,{9},0x83010000);
        memory.put(object(9)+L::C_CitadelBaseAbility::m_bChanneling,(unsigned char)1);memory.put(object(9)+L::C_CitadelBaseAbility::m_flChannelStartTime,99.f);memory.put(object(9)+L::CCitadel_Ability_Lash_Ultimate::m_flNextStateTime,100.08f);
        Vector targets{1,0,0x84000000,1,0};memory.zero(targets.data,L::LockonTarget_t::size);memory.put(targets.data+L::LockonTarget_t::m_hTarget,handle(2));memory.put(object(9)+L::CCitadelBaseLockonAbility::m_vecLockonTargets,targets);
    }
};

static void discovery_sparse_checks(){
    cfg=Settings{};cfg.register_all();std::string error;
    // Reproduce the observed +8 chunk alignment with only the leading identity
    // pages resident. The local/controller records and distant pawn still exist.
    Fixture sparse(8);
    for(u64 page=(sparse.chunk&~4095ULL)+8192;page<sparse.chunk+512*0x70;page+=4096)sparse.memory.pages.erase(page);
    for(u64 page=(sparse.second&~4095ULL)+12288;page<sparse.second+512*0x70;page+=4096)sparse.memory.pages.erase(page);
    unsigned largest_identity_read=0;
    live_backend::Client partial([&](u64 a,void* b,u32 n){
        if((a>=sparse.chunk&&a<sparse.chunk+512*0x70)||(a>=sparse.second&&a<sparse.second+512*0x70))largest_identity_read=std::max(largest_identity_read,n);
        return sparse.memory.read(a,b,n);
    });
    assert(partial.bind(sparse.binding,error));auto frame=partial.poll();
    assert(frame.local.valid&&frame.enemies.size()==1&&frame.entities==9);
    assert(frame.discovery.pages_unavailable>0&&frame.discovery.accepted==9&&largest_identity_read<=4096);

    // An entirely unreadable chunk cannot abort discovery of a different chunk.
    Fixture absent;
    for(u64 page=absent.chunk;page<absent.chunk+512*0x70;page+=4096)absent.memory.pages.erase(page);
    live_backend::Client other([&](u64 a,void* b,u32 n){return absent.memory.read(a,b,n);});
    assert(other.bind(absent.binding,error));frame=other.poll();
    assert(frame.entities==1&&frame.discovery.accepted==1&&frame.discovery.pages_unavailable==14&&!frame.local.valid);

    // A single unrelated entity read failure is skipped, not a scan failure.
    Fixture optional;
    live_backend::Client unavailable_object([&](u64 a,void* b,u32 n){return a!=Fixture::object(9)&&optional.memory.read(a,b,n);});
    assert(unavailable_object.bind(optional.binding,error));frame=unavailable_object.poll();
    assert(frame.local.valid&&frame.enemies.size()==1&&frame.discovery.object_unreadable==1&&frame.discovery.accepted==8);

    // Empty results obey the discovery interval. Explicit recache retries now.
    Fixture empty;std::array<u64,64> no_chunks{};empty.memory.put(empty.system+0x10,no_chunks.data(),sizeof(no_chunks));unsigned scans=0;
    live_backend::Client waiting([&](u64 a,void* b,u32 n){if(a==empty.system+0x10)++scans;return empty.memory.read(a,b,n);});
    assert(waiting.bind(empty.binding,error));assert(waiting.poll().entities==0);waiting.poll();waiting.poll();assert(scans==1);
    waiting.recache();waiting.poll();assert(scans==2);
}

static void control_and_collision_checks(){
    for(int mode=0;mode<2;mode++){
        std::vector<float> positions;
        for(int hz:{60,144,240}){
            aim_model::Movement movement;float position=0;Vec2 previous;
            for(int n=0;n<hz;n++){
                float error=15-position,dt=1.f/hz;auto step=aim_model::controlled_motion({error,0},movement,7,1,dt,1,mode,1,120,720,200,true,.4f,2);
                assert(std::isfinite(step.x)&&step.x>=-1e-5f&&step.x<=error+.0001f);assert(movement.velocity.len()<=120.001f);
                if(error>.05f)assert((movement.velocity-previous).len()<=720*dt+.01f);previous=movement.velocity;position+=step.x;
            }assert(position>13&&position<=15.001f);positions.push_back(position);
        }assert(*std::max_element(positions.begin(),positions.end())-*std::min_element(positions.begin(),positions.end())<.35f);
    }
    aim_model::Movement gentle;float moved=0;for(int n=0;n<144;n++)moved+=aim_model::controlled_motion({2-moved,0},gentle,20,1,1.f/144,.05f,1,1,30,120,1000,true,.1f,4).x;assert(moved>0&&moved<.4f);
    auto stopped=aim_model::controlled_motion({10,0},gentle,7,0,.01f,0,1,1,180,1000,200,true,.4f,2);assert(stopped.len()==0&&gentle.velocity.len()==0);
    collision::Scene scene;scene.build({{{0,-10,-10},{0,10,-10},{0,0,10},0,0,collision::Vision},{{4,0,0},{4,0,3},{},1,1,collision::Bullet}},"fixture");
    auto blocked=scene.trace({-5,0,0},{5,0,0},collision::Vision);assert(blocked.known&&!blocked.clear&&std::fabs(blocked.hit.x)<.01f);
    assert(scene.trace({-5,8,8},{5,8,8},collision::Vision).clear);assert(scene.trace({-5,0,0},{2,0,0},collision::Bullet).clear);
    assert(!scene.trace({2,0,1},{6,0,1},collision::Bullet).clear);std::string error;
    assert(scene.save("collision_fixture.acecol",11193905,error));collision::Scene restored;assert(restored.load("collision_fixture.acecol","fixture",error));assert(!restored.load("collision_fixture.acecol","wrong_map",error));
    {std::fstream file("collision_fixture.acecol",std::ios::binary|std::ios::in|std::ios::out);file.seekg(-1,std::ios::end);char byte=0;file.read(&byte,1);byte^=0x5a;file.seekp(-1,std::ios::end);file.write(&byte,1);}assert(!restored.load("collision_fixture.acecol","fixture",error));std::remove("collision_fixture.acecol");
    assert(restored.load("../../assets/collision/hero_testing.acecol","hero_testing",error)&&restored.size()>90000);
    assert(restored.load("../../assets/collision/dl_midtown.acecol","dl_midtown",error)&&restored.size()>2800000);
    float matrix[16]={1,0,0,-10,0,0,1,-50,0,1,0,0,0,1,0,100};Vec3 camera;assert(collision::camera_origin(matrix,camera)&&camera.dist({10,-100,50})<.01f);
    assert(autouse::reload_window(100,102,0,.5f,.4f,101));assert(!autouse::reload_window(100,102,0,.5f,.4f,100.8f));assert(!autouse::reload_window(100,102,0,.5f,.4f,101.2f));assert(!autouse::reload_window(100,102,0,0,.4f,101));
    cfg=Settings{};cfg.register_all();Entity local;local.base=1;local.handle=1;local.hp=50;local.max_hp=100;local.health_current_valid=local.health_max_valid=local.health_pair_valid=local.life_state_valid=local.alive_valid=true;local.alive=local.modifiers_valid=local.abilities_valid=local.weapon_valid=true;local.reloading=false;
    auto add=[&](const char* key,const char* type=""){const auto& definition=named(key);Ability a;a.token=definition.token;a.base=100+a.token;a.valid=a.ready=true;a.type=type;local.abilities={a};auto p=autouse::prefix(a.token);if(cfg.find((p+"enabled").c_str())){cfg.find((p+"enabled").c_str())->b=true;cfg.find((p+"key").c_str())->i='C';}return a.token;};
    Entity enemy;enemy.base=2;enemy.handle=2;enemy.hp=enemy.max_hp=100;enemy.alive=enemy.visible=true;enemy.pos={100,0,0};Entity other=enemy;other.base=3;other.handle=3;other.pos.y=50;
    auto cold=add("upgrade_cold_front");assert(autouse::choose(local,{enemy,other},{},100,false).token==cold);local.health_pair_valid=false;assert(!autouse::choose(local,{enemy,other},{},100,false).token);local.health_pair_valid=true;other.visible=false;assert(!autouse::choose(local,{enemy,other},{},100,false).token);other.visible=true;other.pos.x=1000;assert(!autouse::choose(local,{enemy,other},{},100,false).token);
    auto burst=add("upgrade_surging_power");assert(autouse::choose(local,{enemy},{},100,false).token==burst);enemy.bullet_invuln=true;assert(!autouse::choose(local,{enemy},{},100,false).token);enemy.bullet_invuln=false;local.disarmed=true;assert(!autouse::choose(local,{enemy},{},100,false).token);local.disarmed=false;
    auto dispel=add("upgrade_reduce_debuff_duration");u32 hex=named("upgrade_greater_withering_whip").token;cfg.find(("cleanse.watch."+std::to_string(hex)).c_str())->b=true;assert(!autouse::choose(local,{enemy},{},100,false).token);local.removable_debuffs={hex};assert(autouse::choose(local,{enemy},{},100,false).token==dispel);local.removable_debuffs.clear();
    local.hero=13;auto smoke=add("ability_smoke_bomb");local.removable_debuffs={hex};local.abilities[0].upgrade_bits=7;assert(!autouse::choose(local,{enemy},{},100,false).token);local.abilities[0].upgrade_bits=15;assert(autouse::choose(local,{enemy},{},100,false).token==smoke);local.removable_debuffs.clear();
    local.hero=18;local.team=1;auto combo=add("ability_ult_combo");enemy.team=2;enemy.hero=13;enemy.pos={0,0,0};
    {std::lock_guard<std::mutex> lock(snap_mtx);std::fill(std::begin(snap.view),std::end(snap.view),0);snap.view[15]=1;}
    Ability greater;greater.token=1193964439u;Ability mystic;mystic.token=754480263u;mystic.imbued_valid=true;mystic.imbued_tokens={combo};
    assert(std::fabs(ability_range::known_lower_bound(local,combo,4)-4)<.001f);
    local.abilities.push_back(greater);assert(std::fabs(ability_range::known_lower_bound(local,combo,4)-5.2f)<.001f);
    local.abilities.pop_back();local.abilities.push_back(mystic);assert(std::fabs(ability_range::known_lower_bound(local,combo,4)-4.8f)<.001f);
    local.abilities.back().imbued_tokens.clear();assert(std::fabs(ability_range::known_lower_bound(local,combo,4)-4)<.001f);
    local.abilities.pop_back();auto cp=autouse::prefix(combo);Entity ally;ally.alive=true;ally.team=1;ally.pos={30,0,0};
    assert(!autouse::choose(local,{enemy},{},100,false,1920,1080).token);
    assert(autouse::choose(local,{enemy},{ally},100,false,1920,1080).token==combo);
    ally.pos={1000,0,0};assert(!autouse::choose(local,{enemy},{ally},100,false,1920,1080).token);
    ally.pos={30,0,0};enemy.pos={world_units::units(5),0,0};assert(!autouse::choose(local,{enemy},{ally},100,false,1920,1080).token);
    local.abilities.push_back(greater);assert(autouse::choose(local,{enemy},{ally},100,false,1920,1080).token==combo);local.abilities.pop_back();
    enemy.pos={0,0,0};enemy.visible=false;assert(!autouse::choose(local,{enemy},{ally},100,false,1920,1080).token);enemy.visible=true;
    cfg.find((cp+"friends").c_str())->i=2;assert(!autouse::choose(local,{enemy},{ally},100,false,1920,1080).token);
    auto active_reload=add("upgrade_active_reload","CCitadel_Item_ActiveReload");cfg.find("auto.active_reload")->b=true;local.reloading=local.active_reload_allowed=true;local.reload_begin=100;local.reload_end=102;local.reload_fraction=.5f;local.reload_width=.4f;
    assert(autouse::choose(local,{}, {},101,false).token==active_reload);local.abilities[0].active_reload_failed=true;assert(!autouse::choose(local,{}, {},101,false).token);local.abilities.clear();assert(!autouse::choose(local,{}, {},101,false).token);
    std::puts("PASS: damped/smoothed motion at 60/144/240 Hz, speed/acceleration/ramp/strength, weak assist, triangle/capsule collision, channels, corrupt and wrong-map rejection, packaged map BVHs, camera derivation, real item/cleanse/upgrade/reload conditions");
}


static void counterspell_filter_checks(){
    cfg=Settings{};cfg.register_all();cfg.find("defense.counterspell.enabled")->b=true;
    defense::World world;world.usable=true;world.time=100;world.local.base=1;world.local.handle=1;world.local.hero=6;world.local.alive=world.local.modifiers_valid=true;
    world.local.bounds_valid=true;world.local.bounds_min={-16,-16,0};world.local.bounds_max={16,16,72};
    for(auto key:{"upgrade_counterspell","citadel_ability_melee_parry"}){Ability a;a.token=named(key).token;a.valid=a.ready=true;world.local.abilities.push_back(a);}
    for(auto key:{"citadel_ability_bull_heal","ability_nurse_fear","ability_frank_painaura","citadel_ability_storm_cloud","ability_fencer_ultimate","ability_baba_ultimate_2","ability_punkgoat_ult","citadel_ability_lash_down_strike"}){
        const auto& incoming=named(key);assert(!defense::counterspell_observable(incoming));cfg.find(defense::selection("defense.counterspell.",incoming.token).c_str())->b=true;
        defense::Threat t;t.id=10;t.token=incoming.token;t.eta=.05f;t.victims={world.local.handle};assert(defense::choose(world,{t}).type==defense::Decision::None);
    }
    const auto& hex=named("upgrade_containment");assert(defense::counterspell_observable(hex));cfg.find(defense::selection("defense.counterspell.",hex.token).c_str())->b=true;
    defense::Threat direct;direct.id=11;direct.token=hex.token;direct.eta=.05f;direct.victims={world.local.handle};assert(defense::choose(world,{direct}).type==defense::Decision::Counterspell);
    Entity enemy;enemy.base=20;enemy.handle=20;enemy.alive=true;enemy.pos={500,0,0};Ability leap;leap.token=named("citadel_ability_bull_leap").token;leap.valid=true;leap.has_impact_position=true;leap.impact_time=100.08f;leap.impact_position={0,0,0};enemy.abilities={leap};world.enemies={enemy};
    // The native Leap timestamp is the float at +8 in CCitadelAutoScaledTime,
    // not the wrapper prefix. A failed timestamp read cannot mark impact valid.
    static_assert(L::CCitadelAutoScaledTime::m_flTime==8);Fixture native;
    native.ability(10,"citadel_ability_bull_leap",3,"CCitadel_Ability_Bull_Leap");native.abilities(600,{10},0x83010000);
    const auto base=Fixture::object(10);native.memory.put(base+L::CCitadel_Ability_Bull_Leap::m_vecCrashPosition,Vec3{0,0,0});native.memory.put(base+L::CCitadel_Ability_Bull_Leap::m_eLeapState,int(3));
    native.memory.put(base+L::CCitadel_Ability_Bull_Leap::m_flNextStateTime,u64(0x10000000));native.memory.put(base+L::CCitadel_Ability_Bull_Leap::m_flNextStateTime+8,100.08f);
    bool timestamp_available=true;live_backend::Client native_client([&](u64 a,void* b,u32 n){return (timestamp_available||a!=base+L::CCitadel_Ability_Bull_Leap::m_flNextStateTime+8)&&native.memory.read(a,b,n);});std::string bind_error;
    assert(native_client.bind(native.binding,bind_error));consumer_demand::note_defensive_tick();auto native_frame=native_client.poll();assert(native_frame.enemies.size()==1&&native_frame.enemies[0].abilities.size()==1&&native_frame.enemies[0].abilities[0].has_impact_position&&std::fabs(native_frame.enemies[0].abilities[0].impact_time-100.08f)<.001f);
    timestamp_available=false;consumer_demand::note_defensive_tick();native_frame=native_client.poll();assert(native_frame.enemies[0].abilities.size()==1&&!native_frame.enemies[0].abilities[0].has_impact_position);
    auto threats=defense::detect(world);assert(threats.size()==1&&threats.front().token==leap.token);
    world.enemies[0].abilities[0].has_impact_position=false;assert(defense::detect(world).empty());
    world.enemies[0].abilities[0]=leap;world.enemies[0].abilities[0].impact_time=99;assert(defense::detect(world).empty());
    world.enemies[0].abilities[0]=leap;world.enemies[0].abilities[0].impact_position={10000,0,0};assert(defense::detect(world).empty());
}

static void health_separation_checks(){
    cfg=Settings{};cfg.register_all();Fixture fixture;std::string error;
    const auto local_data=Fixture::object(1)+L::CCitadelPlayerController::m_PlayerDataGlobal;
    const auto enemy_data=Fixture::object(3)+L::CCitadelPlayerController::m_PlayerDataGlobal;
    fixture.memory.put(Fixture::object(2)+L::C_BaseEntity::m_iHealth,int32_t(2449));
    fixture.memory.put(local_data+L::PlayerDataGlobal_t::m_iHealthMax,int32_t(0));
    fixture.memory.put(Fixture::object(600)+L::C_BaseEntity::m_iHealth,int32_t(2000));
    fixture.memory.put(enemy_data+L::PlayerDataGlobal_t::m_iHealthMax,int32_t(500));
    live_backend::Client client([&](u64 a,void* b,u32 n){return fixture.memory.read(a,b,n);});assert(client.bind(fixture.binding,error));
    for(int i=0;i<8;++i)client.maintenance();auto frame=client.poll();
    assert(frame.visuals_ready()&&frame.local.valid&&frame.local.alive&&frame.local.alive_valid&&!frame.dead);
    assert(frame.local.health_current_valid&&!frame.local.health_max_valid&&!frame.local.health_pair_valid&&frame.local.max_hp==0);
    assert(frame.enemies.size()==1&&frame.enemies[0].valid&&frame.enemies[0].alive&&!frame.enemies[0].health_pair_valid);
    fixture.memory.put(Fixture::object(2)+L::C_BaseEntity::m_iHealth,int32_t(-1));frame=client.poll();
    assert(frame.visuals_ready()&&frame.local.valid&&!frame.local.health_current_valid&&!frame.local.alive_valid&&!frame.dead);
    assert(frame.matrix_valid&&frame.enemies.size()==1); // pose/view data survives unknown current HP too
}

int main(){health_separation_checks();discovery_sparse_checks();control_and_collision_checks();
    cfg.register_all();Fixture f;live_backend::Client client([&](u64 a,void* b,u32 size){return f.memory.read(a,b,size);});std::string message;assert(client.bind(f.binding,message));
    cfg.find("esp.abilities")->b=false;cfg.find("esp.ultimate")->b=false;
    consumer_demand::automatic_until.store(0);consumer_demand::defensive_until.store(0);
    auto idle=client.poll();assert(idle.enemies.size()==1&&idle.enemies[0].abilities.empty());
    cfg.find("defense.11.enabled")->b=true;consumer_demand::note_defensive_tick();
    auto frame=client.poll();assert(frame.local.valid&&frame.local.hero==11&&frame.local.hp==100&&frame.local.team==1&&frame.local.modifiers_valid);assert(frame.in_match&&!frame.dead&&!frame.menu&&frame.input_valid&&frame.matrix_valid&&frame.visibility_valid);assert(frame.keys[VK_XBUTTON1]);assert(frame.enemies.size()==1&&frame.enemies[0].idx==600&&frame.enemies[0].visible);assert(frame.local.abilities.size()==3&&frame.local.abilities[0].ready);assert(frame.enemies[0].abilities.size()==1&&!frame.enemies[0].abilities[0].targets.empty()&&frame.enemies[0].abilities[0].targets.front()==Fixture::handle(2));
    f.memory.put(f.binding.engine+L::engine_ui+L::engine_ui_flags,(unsigned char)2);assert(client.poll().menu);f.memory.put(f.binding.engine+L::engine_ui+L::engine_ui_flags,(unsigned char)0);
    auto buttons=f.binding.input+L::input_instance+L::input_buttons;f.memory.put(buttons+(68/32)*4,u32(1u<<(68%32)));assert(client.poll().scoreboard);f.memory.put(buttons+(68/32)*4,u32(0));
    f.memory.put(f.rules+L::C_CitadelGameRules::m_bServerPaused,(unsigned char)1);assert(!client.poll().in_match);f.memory.put(f.rules+L::C_CitadelGameRules::m_bServerPaused,(unsigned char)0);
    f.memory.put(Fixture::object(6)+L::C_CitadelBaseAbility::m_bIsCoolingDownInternal,(unsigned char)1);assert(!client.poll().local.abilities[0].ready);f.memory.put(Fixture::object(6)+L::C_CitadelBaseAbility::m_bIsCoolingDownInternal,(unsigned char)0);
    defense::World world;world.local=frame.local;world.enemies=frame.enemies;world.time=frame.game_time;world.usable=true;
    auto threats=defense::detect(world);assert(threats.size()==1&&threats[0].aoe&&threats[0].victims.front()==world.local.handle);
    std::string prefix=defense::prefix(11);cfg.find((prefix+"enabled").c_str())->b=true;cfg.find(defense::selection(prefix,threats[0].token).c_str())->b=true;
    auto decision=defense::choose(world,threats);assert(decision.type==defense::Decision::Dodge&&decision.key=='2');
    Entity ally=world.local;ally.base=0x999000;ally.handle=Fixture::handle(20);ally.pos.x=20;world.friends={ally};threats[0].victims.push_back(ally.handle);cfg.find((prefix+"protect_team").c_str())->b=true;cfg.find((prefix+"alt_mode").c_str())->i=1;cfg.find((prefix+"alt_key").c_str())->i='G';decision=defense::choose(world,threats);assert(decision.type==defense::Decision::GroupDodge&&decision.key=='G'&&decision.modifier==0);
    threats[0].aoe=false;decision=defense::choose(world,threats);assert(decision.type==defense::Decision::Dodge);cfg.find((prefix+"enabled").c_str())->b=false;
    cfg.find("defense.counterspell.enabled")->b=true;cfg.find(defense::selection("defense.counterspell.",threats[0].token).c_str())->b=true;decision=defense::choose(world,threats);assert(decision.type==defense::Decision::Counterspell&&decision.key=='F');
    world.local.abilities.erase(std::remove_if(world.local.abilities.begin(),world.local.abilities.end(),[](auto& a){return a.token==named("upgrade_counterspell").token;}),world.local.abilities.end());assert(defense::choose(world,threats).type==defense::Decision::None);
    world.friends.clear();Entity projectile;projectile.base=0x990000;projectile.handle=Fixture::handle(40);projectile.owner=world.enemies[0].base;projectile.pos={100,0,36};projectile.vel={-2000,0,0};projectile.ability_token=named("upgrade_focus_lens").token;projectile.collision_radius=15;world.projectiles={projectile};auto detected=defense::detect(world);assert(std::any_of(detected.begin(),detected.end(),[](auto& t){return t.projectile&&std::fabs(t.eta-.0345f)<.001f;}));
    world.projectiles[0].pos.z=78;detected=defense::detect(world);assert(std::any_of(detected.begin(),detected.end(),[](auto& t){return t.projectile;}));world.projectiles[0].pos.z=160;detected=defense::detect(world);assert(std::none_of(detected.begin(),detected.end(),[](auto& t){return t.projectile;}));world.projectiles[0].pos.z=36;
    world.projectiles[0].vel={2000,0,0};detected=defense::detect(world);assert(std::none_of(detected.begin(),detected.end(),[](auto& t){return t.projectile;}));
    // A recycled ability identity cannot be resolved from its previous handle.
    f.memory.put(f.chunk+6*0x70+0x10,u32((2u<<15)|6));auto recycled=client.poll();assert(std::none_of(recycled.local.abilities.begin(),recycled.local.abilities.end(),[](auto& a){return a.token==named("citadel_ability_void_sphere").token;}));
    f.memory.put(f.binding.client+0x80+offsetof(IMAGE_NT_HEADERS64,FileHeader)+offsetof(IMAGE_FILE_HEADER,TimeDateStamp),L::client_timestamp+1);assert(!client.bind(f.binding,message)&&!client.poll().attached);
    // Regression: an unreadable runtime-only key table must use the exact-build
    // initializer without confusing mouse/keyboard bitmap validity or scoreboard.
    Fixture key_fixture;bool missing_table=true,missing_bitmap=false;
    live_backend::Client key_client([&](u64 address,void* data,u32 size){
        if(missing_table&&address==key_fixture.binding.input+L::virtual_key_table)return false;
        if(missing_bitmap&&address==key_fixture.binding.input+L::input_instance+L::input_buttons)return false;
        return key_fixture.memory.read(address,data,size);
    });
    assert(key_client.bind(key_fixture.binding,message));auto key_frame=key_client.poll();
    assert(key_frame.input_valid&&key_frame.visuals_ready()&&key_frame.keys[VK_XBUTTON1]);
    const auto key_buttons=key_fixture.binding.input+L::input_instance+L::input_buttons;
    key_fixture.memory.put(key_buttons+(85/32)*4,u32(1u<<(85%32)));key_frame=key_client.poll();
    assert(key_frame.keys[VK_RCONTROL]&&key_frame.keys[VK_CONTROL]&&!key_frame.keys[VK_LCONTROL]);
    key_fixture.memory.put(key_buttons+(85/32)*4,u32(0));
    key_fixture.memory.put(key_buttons+(68/32)*4,u32(1u<<(68%32)));assert(key_client.poll().scoreboard);
    key_fixture.memory.put(key_buttons+(68/32)*4,u32(1u<<(67%32)));assert(!key_client.poll().scoreboard);
    missing_bitmap=true;key_frame=key_client.poll();assert(!key_frame.input_valid&&key_frame.visuals_ready()&&!key_frame.keys[VK_XBUTTON1]);
    missing_bitmap=false;missing_table=false;key_fixture.memory.zero(key_fixture.binding.input+L::virtual_key_table,1024);
    assert(key_client.bind(key_fixture.binding,message)&&key_client.poll().input_valid);
    key_fixture.memory.put(key_fixture.binding.input+0x80+offsetof(IMAGE_NT_HEADERS64,FileHeader)+offsetof(IMAGE_FILE_HEADER,TimeDateStamp),L::inputsystem_timestamp+1);
    assert(!key_client.bind(key_fixture.binding,message)); // Fallback must never bypass build matching.
    // A failed reference read is not evidence of a changed game layout.
    Fixture reference_fixture;bool unavailable_reference=true;
    live_backend::Client reference_client([&](u64 address,void* data,u32 size){if(unavailable_reference&&address==reference_fixture.binding.client+L::globals_instruction)return false;return reference_fixture.memory.read(address,data,size);});
    assert(!reference_client.bind(reference_fixture.binding,message)&&message.find("Could not read client global reference")!=std::string::npos&&!reference_client.poll().attached);
    unavailable_reference=false;reference_fixture.memory.put(reference_fixture.binding.client+L::globals_instruction,(unsigned char)0x90);
    assert(!reference_client.bind(reference_fixture.binding,message)&&message.find("global layout mismatch")!=std::string::npos&&!reference_client.poll().attached);
    assert(km_protocol::hid('1')==30&&km_protocol::hid('2')==31&&km_protocol::hid('0')==39&&km_protocol::hid(VK_LMENU)==226);assert(km_protocol::press('2',35)=="km.press(31,35,0)\r\n");assert(km_protocol::chord(VK_LMENU,'2',true)=="km.multidown(226,31)\r\n");
    counterspell_filter_checks();
    std::puts("PASS: exact-build PE/RIP validation; chunked entities/serial handles; integer health and scene position; game-PC keys/UI/scoreboard/pause; FOW visibility; live ability readiness/ownership; target-specific dodge; AoE teammate cast; Counterspell; approaching/receding projectiles; recycled handles; Makcu key/chord protocol");
}

// Compile-only host byte fixtures. No main and no executable is linked/run.
#include "native_joint_backend.h"
#include "native_target.h"
#include "native_joint_demand.h"
#include "native_cache_compile_fixtures.cpp"

// Independent component boundaries are compiler-evaluated. Aim's world range
// cannot remove Trigger's Head/Neck request when Head ESP is disabled.
constexpr std::array<native_joint_demand::Request,2> separate_ranges{{{native_joints::head,80},{native_joints::neck,200}}};
static_assert(native_joint_demand::at_distance(separate_ranges,150)==native_joints::neck);
static_assert(native_joint_demand::at_distance(separate_ranges,80)==(native_joints::head|native_joints::neck));
static_assert(native_joint_demand::at_distance(separate_ranges,201)==0);
static_assert(native_joint_demand::effective_distance(0,80)==80);
static_assert(native_joint_demand::effective_distance(200,80)==200);
static_assert(native_joint_demand::effective_distance(-1,80)==0);
constexpr std::array<native_joint_demand::Request,2> trigger_only{{{0,80},{native_joints::head,native_joint_demand::effective_distance(200,80)}}};
static_assert(native_joint_demand::at_distance(trigger_only,150)==native_joints::head);
constexpr std::array<native_joint_demand::Request,3> legacy_ranges{{{native_joints::head,80},{native_joints::neck,native_joint_demand::effective_distance(0,80)},{native_joints::neck,native_joint_demand::effective_distance(0,80)}}};
static_assert(native_joint_demand::at_distance(legacy_ranges,150)==0);
constexpr std::array<native_joint_demand::Request,3> magnet_override{{{native_joints::head,80},{native_joints::head,80},{native_joints::neck,native_joint_demand::effective_distance(200,80)}}};
static_assert(native_joint_demand::at_distance(magnet_override,150)==native_joints::neck);
static_assert(native_joint_demand::at_distance(trigger_only,-1)==0);
static_assert(native_joint_demand::visual_head(true,true,true,200,150)==native_joints::head);
static_assert(native_joint_demand::visual_head(false,true,true,200,150)==0);
static_assert(native_joint_demand::visual_head(true,true,true,100,150)==0);

[[maybe_unused]] void native_consumer_boundaries(){
    Entity e;e.valid=true;e.host_sample_ms=1000;e.pos={1000,2000,3000};e.scale=4;
    e.native_joints.binding_epoch=7;e.native_joints.started_ms=1000;e.native_joints.completed_ms=1001;
    e.native_joints.valid_mask=3;e.native_joints.world[0]={10,20,30};e.native_joints.world[1]={11,22,33};
    ::Vec3 point;assert(native_target::point(e,0,point,7,7,1100));assert(point.x==10&&point.y==20&&point.z==30);
    assert(!native_target::point(e,0,point,7,8,1100));assert(!native_target::point(e,0,point,7,0,1100));
    assert(!native_target::point(e,2,point,7,7,1100));assert(!native_target::point(e,0,point,7,7,1251));
    assert(!native_target::point(e,0,point,7,7,1000));
    e.native_joints.valid_mask=1;assert(!native_target::point(e,1,point,7,7,1100));assert(native_target::point(e,0,point,7,7,1100));assert(e.valid);
    e.host_sample_ms=800;assert(!native_target::point(e,0,point,7,7,1100));e.host_sample_ms=1000;
    native_joint_demand::Lease lease;lease.publish(native_joints::head,7,1000);
    assert(lease.read(7,1050)==native_joints::head);assert(lease.read(8,1050)==0);assert(lease.read(7,1101)==0);assert(lease.read(7,900)==0);
    lease.publish(0,7,1050);assert(lease.read(7,1050)==0);
    lease.publish(native_joints::head,7,1000,80);const auto request=lease.request(7,1050);
    assert(request.mask==native_joints::head&&request.maximum_distance_units==80);
    // Invulnerability blocks action selection, not the enabled player marker.
    e.invuln=true;e.stealthed=true;
    const auto visual=native_joint_demand::visual_head(true,e.valid,true,200,150);
    const auto action=(!e.invuln&&!e.stealthed)?native_joint_demand::at_distance(trigger_only,150):0;
    assert(visual==native_joints::head&&action==0);
    ::Reader source;assert(!source.dma_read_ready());assert(!source.publish_attachment_epoch());assert(source.attachment_epoch()==0);
}

struct MultiActorFixture{
    SyntheticSource memory;std::vector<Entity> entities;std::vector<native_joint_backend::Work> work;
    explicit MultiActorFixture(unsigned count,bool both=false):entities(count){
        Fixture seed;memory=std::move(seed.memory);
        for(unsigned i=0;i<count;++i){const u64 shift=u64(i+1)*0x1000000;
            const auto entity=Fixture::entity+shift,id=Fixture::identity+shift,scene=Fixture::scene+shift,poses=Fixture::poses+shift,bitmap=Fixture::bitmap+shift;
            const u32 handle=Fixture::handle+i;
            memory.put(entity+d::entity_identity_offset,id);memory.put(entity+d::entity_scene_offset,scene);
            memory.put(id+d::identity_handle_offset,handle);memory.put<u32>(id+d::identity_flags_offset,0);
            memory.put(scene,Fixture::module+d::scene_vtable_rva);memory.put(scene+d::scene_owner_offset,entity);memory.put(scene+d::scene_lock_offset,d::Lock{});
            memory.put(scene+d::model_record_offset,Fixture::record);memory.put(scene+d::pose_pointer_offset,poses);memory.put(scene+d::bitmap_pointer_offset,bitmap);
            memory.put<std::int32_t>(scene+d::pose_count_offset,3);memory.put<u32>(scene+d::computed_mask_offset,0x100);memory.put<u64>(scene+d::condition_pointer_offset,0);memory.put<u32>(scene+d::condition_mask_offset,0);
            memory.put<u32>(bitmap,5);memory.put(poses,d::NativePose{10+float(i),20,30,1,0,0,0,1});memory.put(poses+64,d::NativePose{40+float(i),50,60,1,0,0,0,1});
            auto& e=entities[i];e.base=entity;e.handle=handle;e.valid=true;e.host_sample_ms=now_ms();e.pos={1000,2000,3000};
            work.push_back({&e,{entity,id,handle},native_joints::head});
            if(both)work.push_back({&e,{entity,id,handle},native_joints::neck});
        }
    }
};

[[maybe_unused]] void shared_stage_fixtures(){
    {
        MultiActorFixture f(7);native_joint_backend::Bank bank(f.memory);
        for(unsigned i=0;i<7;++i)bank.sample(Fixture::module,1,f.work);
        const auto stats=bank.sample(Fixture::module,1,f.work);
        assert(stats.accepted==7&&stats.attempted==7&&stats.warmups==0&&stats.fresh_calls==4);
        for(unsigned i=0;i<7;++i){const auto& e=f.entities[i];assert(e.native_joints.valid_mask==1);assert(e.native_joints.world[0].x==40+float(i));assert(e.native_joints.binding_epoch==1);assert(e.native_joints.started_ms<=e.native_joints.completed_ms);}
        // A partial selected read rejects its owner while six peers can succeed.
        f.memory.partial_address=Fixture::poses+0x1000000+64;
        const auto partial=bank.sample(Fixture::module,1,f.work);assert(partial.accepted==6);assert(f.entities[0].native_joints.valid_mask==0);
        f.memory.partial_address=0;
        // No consumer demand performs no I/O and publishes no retained pose.
        for(auto& w:f.work)w.demand=0;const auto calls=f.memory.calls;const auto idle=bank.sample(Fixture::module,1,f.work);
        assert(idle.fresh_calls==0&&f.memory.calls==calls);for(const auto& e:f.entities)assert(e.native_joints.valid_mask==0);
    }
    {
        MultiActorFixture f(1,true);native_joint_backend::Bank bank(f.memory);
        bank.sample(Fixture::module,1,f.work);bank.sample(Fixture::module,1,f.work);
        assert(bank.sample(Fixture::module,1,f.work).accepted==2);
        // Neck readiness must not suppress a separately validated Head marker.
        f.memory.put<u32>(Fixture::bitmap+0x1000000,4);
        const auto stats=bank.sample(Fixture::module,1,f.work);assert(stats.accepted==1);assert(f.entities[0].native_joints.valid_mask==1);
    }
    {
        MultiActorFixture f(65);native_joint_backend::Bank bank(f.memory);
        const auto stats=bank.sample(Fixture::module,1,f.work);assert(stats.attempted<=64&&stats.warmups<=1&&stats.bytes<=256*1024&&stats.requests<=4096);
    }
    {
        MultiActorFixture f(2);native_joint_backend::Bank bank(f.memory);
        bank.sample(Fixture::module,1,f.work);bank.sample(Fixture::module,1,f.work);
        unsigned reads=0;f.memory.before_read=[&](SyntheticSource& source,std::span<native_joints::ReadRequest> requests){for(const auto& r:requests)if(r.address==Fixture::poses+0x1000000+64&&++reads==2)source.put(r.address,d::NativePose{41,50,60,1,0,0,0,1});};
        const auto stats=bank.sample(Fixture::module,1,f.work);assert(stats.accepted==1&&f.entities[0].native_joints.valid_mask==0&&f.entities[1].native_joints.valid_mask==1);
    }
}

native_joint_quarantine::Plan fixture_plan(const MultiActorFixture& f){
    native_joint_quarantine::Plan p;p.target=f.work[0].target;p.module=Fixture::module;p.epoch=1;
    const u64 shift=0x1000000;auto& e=p.expected;
    e.identity=Fixture::identity+shift;e.scene=Fixture::scene+shift;e.record=Fixture::record;e.model=Fixture::model;
    e.poses=Fixture::poses+shift;e.bitmap=Fixture::bitmap+shift;e.names=Fixture::names;e.flags=Fixture::flags;e.pose_count=e.model_count=3;
    p.selected.index=2;p.selected.name=Fixture::head_name;p.selected.flags=0x100;p.selected.exact_name="head";p.selected.matches=1;return p;
}

[[maybe_unused]] void quarantined_parent_rejection_cases(){
    using namespace native_joint_quarantine;
    {
        MultiActorFixture f(1);auto plan=fixture_plan(f);Row row;row.plan=&plan;const auto calls=f.memory.calls;
        Runner::sample(f.memory,std::span<Row>(&row,1));assert(row.failure==native_joints::Failure::None);assert(f.memory.calls-calls==4);
    }
    // All old leaf bytes remain readable, named, finite, computed and stable.
    // Changing any live dependency must still reject that old proof and require
    // cold discovery; successful OldLeaf reads are never promotion evidence.
    for(unsigned changed=0;changed<5;++changed){
        MultiActorFixture f(1);auto plan=fixture_plan(f);const auto scene=plan.expected.scene;
        switch(changed){
        case 0:f.memory.put<u64>(f.work[0].target.entity+d::entity_scene_offset,0x50000000);break;
        case 1:f.memory.put<u64>(scene+d::model_record_offset,0x50000000);break;
        case 2:f.memory.put<u64>(scene+d::pose_pointer_offset,0x50000000);break;
        case 3:f.memory.put<u64>(Fixture::model+d::names_pointer_offset,0x50000000);break;
        case 4:f.memory.put<u64>(Fixture::names+16,0x50000000);break;
        }
        d::NativePose old_pose{};auto old_read=d::request(plan.expected.poses+64,old_pose);assert(f.memory.read_fresh(std::span<native_joints::ReadRequest>(&old_read,1)));assert(old_read.valid&&old_pose.x==40);
        Row row;row.plan=&plan;Runner::sample(f.memory,std::span<Row>(&row,1));
        assert(row.failure!=native_joints::Failure::None&&row.needs_cold);assert(row.completed_at==native_joints::HostClock::time_point{});
    }
    {
        MultiActorFixture f(1);auto plan=fixture_plan(f);auto& e=plan.expected;e.condition_pointer=0x300000;e.condition_mask=0x100;e.condition_object=Fixture::condition_object;
        f.memory.put(e.scene+d::condition_pointer_offset,e.condition_pointer);f.memory.put(e.scene+d::condition_mask_offset,e.condition_mask);
        f.memory.put(Fixture::module+d::condition_object_pointer_rva,e.condition_object);f.memory.put<std::uint8_t>(e.condition_object+0x58,1);f.memory.put<std::int32_t>(Fixture::module+d::condition_dword_rva,0);
        // The irrelevant callee byte is absent: current native short circuit
        // does not require it, so a per-request failure cannot poison the row.
        f.memory.erase_span(Fixture::module+d::condition_byte_rva,1);Row valid;valid.plan=&plan;Runner::sample(f.memory,std::span<Row>(&valid,1));assert(valid.failure==native_joints::Failure::None);
        // Old G+0x58 remains readable and would permit mask0x100, but the current
        // global parent pointer is new. Old branch evidence cannot approve it.
        f.memory.put<u64>(Fixture::module+d::condition_object_pointer_rva,0x50000000);Row changed;changed.plan=&plan;Runner::sample(f.memory,std::span<Row>(&changed,1));assert(changed.needs_cold&&changed.failure!=native_joints::Failure::None);
    }
    {
        MultiActorFixture f(1);auto plan=fixture_plan(f);unsigned observed=0;
        f.memory.before_read=[&](SyntheticSource& source,std::span<native_joints::ReadRequest> requests){for(const auto& r:requests)if(r.address==plan.target.entity+d::entity_scene_offset&&++observed==2)source.put<u64>(plan.expected.scene+d::pose_pointer_offset,0x50000000);};
        Row row;row.plan=&plan;Runner::sample(f.memory,std::span<Row>(&row,1));assert(row.first.leaf.pose.x==40&&row.second.leaf.pose.x==40);assert(row.needs_cold&&row.failure!=native_joints::Failure::None);
    }
}

constexpr bool native_capability_boundary(){
    for(int region=-2;region<9;++region)
        if(native_target::supports_region(region)!=(region==0||region==1))return false;
    return native_target::supports_entity(EClass::Player)&&
        !native_target::supports_entity(EClass::NPC)&&!native_target::supports_entity(EClass::World)&&
        !native_target::supports_entity(EClass::Projectile)&&!native_target::supports_entity(EClass::Deployable);
}
static_assert(native_capability_boundary());

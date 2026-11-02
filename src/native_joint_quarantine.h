#pragma once
#include "native_joint_reader.h"
#include <optional>

namespace native_joint_quarantine {
namespace wire=native_joints::detail;
using Failure=native_joints::Failure;
using Address=native_joints::Address;
// Address dependencies only. No computed mask, lock, bitmap or pose values are
// retained here as evidence of present readiness or present transform content.
struct Dependencies {
    Address identity=0,scene=0,record=0,model=0,poses=0,bitmap=0,names=0,flags=0,condition_pointer=0,condition_object=0;
    std::int32_t pose_count=0,model_count=0;std::uint32_t condition_mask=0;
    bool operator==(const Dependencies&)const=default;
};
inline Dependencies dependencies(const wire::Snapshot& s){return {s.identity,s.scene,s.model_record,s.model,s.poses,s.bitmap,s.names,s.flags,s.condition_pointer,s.condition_object,s.pose_count,s.model_count,s.condition_mask};}
struct Plan {
    native_joints::Target target{};Address module=0;std::uint64_t epoch=0;
    Dependencies expected{};wire::Descriptor selected{};
};
struct Observation {
    wire::Snapshot state{};wire::SelectedRead leaf{};
    bool base=true,name_entry=true,leaves=true,object_byte=true,dword=true,condition_byte=true;
};
struct Row {
    const Plan* plan=nullptr;Observation first{},second{};
    Failure failure=Failure::None;bool needs_cold=false;
    native_joints::HostClock::time_point started_at{},completed_at{};
};
inline bool same_dependencies(const wire::Snapshot& s,const Dependencies& p){
    return s.identity==p.identity&&s.scene==p.scene&&s.model_record==p.record&&s.model==p.model&&
        s.poses==p.poses&&s.bitmap==p.bitmap&&s.pose_count==p.pose_count&&s.model_count==p.model_count&&
        s.names==p.names&&s.flags==p.flags&&s.condition_pointer==p.condition_pointer&&s.condition_mask==p.condition_mask&&
        s.condition_object==p.condition_object;
}
inline void fail(Row& r,Failure f,bool cold=false){if(r.failure==Failure::None){r.failure=f;r.needs_cold=cold;}}
inline void validate_leaves(Row& row,Observation& o,const wire::Snapshot& proof){
    if(!o.name_entry||!o.leaves){fail(row,Failure::ReadFailed);return;}
    const auto& d=row.plan->selected;auto& x=o.leaf;
    if(d.matches!=1||d.index<0||d.index>=proof.model_count||d.index>=proof.pose_count||!(d.flags&0x100)){fail(row,Failure::InvalidName,true);return;}
    if(x.name!=d.name||x.flags!=d.flags){fail(row,Failure::MetadataChanged,true);return;}
    if(!(x.bitmap_word&(1U<<(std::uint32_t(d.index)&31)))){fail(row,Failure::CacheNotReady);return;}
    if(!wire::finite_pose(x.pose)){fail(row,Failure::NonFiniteTransform);return;}
    if(!wire::parse_name(x.name_bytes,x.name_string)){fail(row,Failure::InvalidName,true);return;}
    if(x.name_string!=d.exact_name){fail(row,Failure::MetadataChanged,true);return;}
}
inline void validate_parents(Row& row,Observation& o){
    const auto& p=*row.plan;auto& s=o.state;
    if(!o.base){fail(row,Failure::ReadFailed,true);return;}
    if(s.identity!=p.target.expected_identity||s.handle!=p.target.expected_handle||(s.identity_flags&wire::rejected_identity_flags)){fail(row,Failure::IdentityMismatch,true);return;}
    if(s.vtable!=p.module+wire::scene_vtable_rva||s.owner!=p.target.entity){fail(row,Failure::InvalidScene,true);return;}
    // Parent hierarchy fields use QUARANTINED expected addresses. All current
    // dependencies must match before leaf work and again after it completes.
    // Changed parents reject old leaves even when those old leaves look valid.
    if(!same_dependencies(s,p.expected)){fail(row,wire::key(s)==wire::MetadataKey{p.expected.identity,p.expected.scene,p.expected.record,p.expected.model,p.expected.names,p.expected.flags,p.target.expected_handle,p.expected.model_count}?Failure::Inconsistent:Failure::MetadataChanged,true);return;}
    if(!wire::valid_count(s.pose_count,1024)||!wire::valid_count(s.model_count,1024)||
       !wire::valid_span(s.scene,wire::computed_mask_offset+4)||!wire::valid_span(s.model_record,8)||!wire::valid_span(s.model,wire::flags_pointer_offset+8)||
       !wire::valid_span(s.poses,std::uint64_t(s.pose_count)*32)||(s.poses&15)||!wire::valid_span(s.bitmap,((std::uint64_t(s.pose_count)+31)/32)*4)||
       !wire::valid_span(s.names,std::uint64_t(s.model_count)*8)||!wire::valid_span(s.flags,std::uint64_t(s.model_count)*4)){fail(row,Failure::InvalidModel,true);return;}
    s.actor_context_captured=s.metadata_captured=true;
    const bool branch=s.condition_pointer&&(s.condition_mask&0x100);
    if(branch){
        if(!o.object_byte){fail(row,Failure::ReadFailed);return;}
        if(s.condition_object_byte){if(!o.dword){fail(row,Failure::ReadFailed);return;}if(s.condition_dword==-1){if(!o.condition_byte){fail(row,Failure::ReadFailed);return;}}else s.condition_byte=0;}
        else{s.condition_dword=0;s.condition_byte=0;}
    }else{s.condition_object=0;s.condition_object_byte=0;s.condition_dword=0;s.condition_byte=0;}
    if(!s.lock.quiet()){fail(row,Failure::Locked);return;}
    if(!wire::mask_ready(s.computed_mask,s.request())){fail(row,Failure::CacheNotReady);return;}
}

// The owning Bank must first verify the supported loaded module and build each
// plan from its fully accepted cold path in the same host attachment epoch.
class Runner {
    enum class Kind{Base,NameEntry,Leaves,ObjectByte,Dword,ConditionByte};
    struct Tagged{std::size_t owner;Kind kind;native_joints::ReadRequest read;};
    static void incomplete(Observation& o,Kind k){switch(k){case Kind::Base:o.base=false;break;case Kind::NameEntry:o.name_entry=false;break;case Kind::Leaves:o.leaves=false;break;case Kind::ObjectByte:o.object_byte=false;break;case Kind::Dword:o.dword=false;break;case Kind::ConditionByte:o.condition_byte=false;break;}}
    static void acquire(native_joints::FreshSource& source,std::span<Row> rows,bool second,bool parents){
        std::vector<Tagged> queued;
        std::vector<std::size_t> leader(rows.size());for(std::size_t i=0;i<rows.size();++i)leader[i]=i;
        auto same_actor=[](const Plan& a,const Plan& b){return a.module==b.module&&a.epoch==b.epoch&&a.target.entity==b.target.entity&&a.target.expected_identity==b.target.expected_identity&&a.target.expected_handle==b.target.expected_handle;};
        auto push=[&](std::size_t i,Kind k,native_joints::ReadRequest r){if(!r.destination||!wire::valid_span(r.address,r.size)){fail(rows[i],Failure::InvalidAddress,true);return;}queued.push_back({i,k,r});};
        auto add=[&]<class T>(std::size_t i,Kind k,Address address,T& value){push(i,k,wire::request(address,value));};
        for(std::size_t i=0;i<rows.size();++i){auto& row=rows[i];if(row.failure!=Failure::None)continue;const auto& p=*row.plan;const auto& e=p.expected;auto& o=second?row.second:row.first;auto& s=o.state;auto& x=o.leaf;const auto index=std::uint32_t(p.selected.index);
            if(parents){
            bool follower=false;
            for(std::size_t j=0;j<i;++j)if(rows[j].failure==Failure::None&&same_actor(p,*rows[j].plan)){
                if(!(e==rows[j].plan->expected)){
                    // Conflicting quarantined plans for one actor cannot borrow
                    // a convenient proof. Retire all and rediscover cold.
                    for(auto& other:rows)if(other.plan&&same_actor(p,*other.plan))fail(other,Failure::Inconsistent,true);
                }else{leader[i]=leader[j];follower=true;}break;
            }
            if(row.failure!=Failure::None||follower)continue;
            add(i,Kind::Base,p.target.entity+wire::entity_identity_offset,s.identity);add(i,Kind::Base,p.target.entity+wire::entity_scene_offset,s.scene);
            add(i,Kind::Base,e.identity+wire::identity_handle_offset,s.handle);add(i,Kind::Base,e.identity+wire::identity_flags_offset,s.identity_flags);
            add(i,Kind::Base,e.scene,s.vtable);add(i,Kind::Base,e.scene+wire::scene_owner_offset,s.owner);add(i,Kind::Base,e.scene+wire::scene_lock_offset,s.lock);
            add(i,Kind::Base,e.scene+wire::model_record_offset,s.model_record);add(i,Kind::Base,e.scene+wire::pose_pointer_offset,s.poses);add(i,Kind::Base,e.scene+wire::bitmap_pointer_offset,s.bitmap);
            add(i,Kind::Base,e.scene+wire::pose_count_offset,s.pose_count);add(i,Kind::Base,e.scene+wire::computed_mask_offset,s.computed_mask);add(i,Kind::Base,e.scene+wire::condition_pointer_offset,s.condition_pointer);add(i,Kind::Base,e.scene+wire::condition_mask_offset,s.condition_mask);
            add(i,Kind::Base,e.record,s.model);add(i,Kind::Base,e.model+wire::model_count_offset,s.model_count);add(i,Kind::Base,e.model+wire::names_pointer_offset,s.names);add(i,Kind::Base,e.model+wire::flags_pointer_offset,s.flags);
            if(e.condition_pointer&&(e.condition_mask&0x100)){
                add(i,Kind::Base,p.module+wire::condition_object_pointer_rva,s.condition_object);add(i,Kind::ObjectByte,e.condition_object+0x58,s.condition_object_byte);
                // Fixed module-address scalar leaves are fetched speculatively.
                // Validation requires only the current native branch's leaves;
                // unused failed reads are ignored and normalized before A/B.
                add(i,Kind::Dword,p.module+wire::condition_dword_rva,s.condition_dword);add(i,Kind::ConditionByte,p.module+wire::condition_byte_rva,s.condition_byte);
            }
            }
            if(!parents){
            add(i,Kind::NameEntry,e.names+std::uint64_t(index)*8,x.name);add(i,Kind::Leaves,e.flags+std::uint64_t(index)*4,x.flags);add(i,Kind::Leaves,e.bitmap+std::uint64_t(index>>5)*4,x.bitmap_word);add(i,Kind::Leaves,e.poses+std::uint64_t(index)*32,x.pose);
            // Fetch every expected semantic byte AND its terminating NUL. Tail
            // allocation bytes are not part of the name association proof.
            push(i,Kind::Leaves,wire::bytes(p.selected.name,x.name_bytes.data(),p.selected.exact_name.size()+1));
            }
        }
        std::vector<native_joints::ReadRequest> requests;std::vector<std::size_t> tags;
        for(std::size_t i=0;i<queued.size();++i)if(rows[queued[i].owner].failure==Failure::None){requests.push_back(queued[i].read);tags.push_back(i);}
        // Parent proof is shared per physical actor; leaf failures remain per
        // joint. Bursts above512 are real separately counted fresh operations.
        for(std::size_t begin=0;begin<requests.size();begin+=512){const auto n=std::min<std::size_t>(512,requests.size()-begin);auto part=std::span<native_joints::ReadRequest>(requests).subspan(begin,n);const auto epoch=source.binding_epoch();const bool capable=source.supports_independent_uncached_reads();
            if(!capable||!epoch){for(auto& r:rows)fail(r,Failure::BindingChanged,true);break;}
            const bool executed=source.read_fresh(part),same=source.binding_epoch()==epoch;
            for(std::size_t j=0;j<n;++j){const auto& tag=queued[tags[begin+j]];auto& row=rows[tag.owner];if(!same||row.plan->epoch!=epoch){fail(row,Failure::BindingChanged,true);continue;}if(!executed||!part[j].valid||part[j].transferred!=part[j].size)incomplete(second?row.second:row.first,tag.kind);}
        }
        if(parents)for(std::size_t i=0;i<rows.size();++i)if(rows[i].failure==Failure::None&&leader[i]!=i){auto& own=second?rows[i].second:rows[i].first;const auto& origin=second?rows[leader[i]].second:rows[leader[i]].first;
            if(rows[leader[i]].failure!=Failure::None){fail(rows[i],rows[leader[i]].failure,rows[leader[i]].needs_cold);continue;}
            // Fresh bytes from this same operation, not a prior frame cache.
            own.state=origin.state;own.base=origin.base;own.object_byte=origin.object_byte;own.dword=origin.dword;own.condition_byte=origin.condition_byte;
        }
        for(auto& row:rows)if(row.failure==Failure::None){if(parents)validate_parents(row,second?row.second:row.first);else validate_leaves(row,second?row.second:row.first,row.first.state);}
    }
public:
    static void sample(native_joints::FreshSource& source,std::span<Row> rows){
        if(rows.size()>128){for(auto& r:rows)fail(r,Failure::BudgetExceeded);return;}
        const auto started=native_joints::HostClock::now();
        for(auto& r:rows){r.first={};r.second={};r.failure=Failure::None;r.needs_cold=false;r.started_at=started;r.completed_at={};if(!r.plan||!r.plan->epoch||r.plan->selected.matches!=1||r.plan->selected.exact_name.empty()||r.plan->selected.exact_name.size()>=wire::name_capacity||r.plan->selected.index<0||!wire::valid_count(r.plan->expected.pose_count,1024)||!wire::valid_count(r.plan->expected.model_count,1024)||r.plan->selected.index>=r.plan->expected.pose_count||r.plan->selected.index>=r.plan->expected.model_count)fail(r,Failure::InvalidName,true);}
        // Strict host operation chronology, not assumed request ordering inside
        // an unordered scatter: A fully completes before either pose read, and
        // B starts after both independently fresh pose/leaf acquisitions finish.
        acquire(source,rows,false,true);
        acquire(source,rows,false,false);
        acquire(source,rows,true,false);
        acquire(source,rows,true,true);
        const auto completed=native_joints::HostClock::now();
        for(auto& r:rows)if(r.failure==Failure::None){const auto& x=r.first.leaf;const auto& y=r.second.leaf;
            if(!wire::same_snapshot(r.first.state,r.second.state)){const bool metadata_changed=!(wire::key(r.first.state)==wire::key(r.second.state));fail(r,metadata_changed?Failure::MetadataChanged:Failure::Inconsistent,metadata_changed);}
            else if(x.name!=y.name||x.flags!=y.flags||x.name_string!=y.name_string)fail(r,Failure::MetadataChanged,true);
            else if(x.bitmap_word!=y.bitmap_word||std::memcmp(&x.pose,&y.pose,sizeof(x.pose)))fail(r,Failure::Inconsistent);
            if(r.failure==Failure::None&&source.binding_epoch()!=r.plan->epoch)fail(r,Failure::BindingChanged,true);
            if(r.failure==Failure::None)r.completed_at=completed;
        }
    }
};
} // namespace native_joint_quarantine

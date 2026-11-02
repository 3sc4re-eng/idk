#pragma once
#include "native_joint_reader.h"
#include "native_joint_quarantine.h"
#include "entities.h"
#include "dma.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace native_joint_backend {
// Reads the actual Reader route fact. No caller-provided mode checkbox exists.
class Source final:public native_joints::FreshSource{
    ::Reader& reader_;
public:
    explicit Source(::Reader& reader):reader_(reader){}
    bool supports_independent_uncached_reads()const noexcept override{return reader_.dma_read_ready();}
    u64 binding_epoch()const noexcept override{return reader_.dma_read_ready()?reader_.attachment_epoch():0;}
    bool read_fresh(std::span<native_joints::ReadRequest> requests)override{
        for(auto& r:requests){r.valid=false;r.transferred=0;}
        const auto epoch=binding_epoch();if(!epoch)return false;
        std::vector<DmaRead> reads;reads.reserve(requests.size());
        for(const auto& r:requests)reads.push_back({r.address,r.destination,r.size,0,false});
        const bool executed=reader_.scatter(reads)&&binding_epoch()==epoch;
        for(size_t i=0;i<reads.size();++i){requests[i].transferred=reads[i].transferred;requests[i].valid=executed&&reads[i].valid&&reads[i].transferred==reads[i].size;}
        return executed;
    }
};
inline const char* failure_text(native_joints::Failure failure){
    static constexpr const char* names[]={"none","no demand","unsupported demand","independent-read contract unavailable","not bound","game version guard mismatch","binding changed","invalid address","read failed","acquisition budget exceeded","identity mismatch","invalid scene","invalid model","invalid count","invalid name","joint name missing","joint name ambiguous","joint not in hitbox set","pose cache locked","pose cache not ready","metadata changed","pose changed during acquisition","non-finite transform"};
    const auto index=static_cast<unsigned>(failure);return index<std::size(names)?names[index]:"unknown failure";
}
struct Stats{native_joints::Failure first_failure=native_joints::Failure::None;u64 bytes=0,requests=0,elapsed_ms=0,fresh_calls=0;unsigned demanded=0,attempted=0,accepted=0,warmups=0,budget_skipped=0,backoff_skipped=0,warm_rows=0,cold_rows=0,plan_discards=0;};
class BudgetSource final:public native_joints::FreshSource{
    native_joints::FreshSource& source_;u64 remaining_bytes_=0,remaining_requests_=0;bool exhausted_=false;
public:
    Stats stats;
    explicit BudgetSource(native_joints::FreshSource& source):source_(source){}
    bool budget_exhausted()const noexcept{return exhausted_;}
    void begin(){remaining_bytes_=256*1024;remaining_requests_=4096;exhausted_=false;stats={};}
    bool supports_independent_uncached_reads()const noexcept override{return source_.supports_independent_uncached_reads();}
    u64 binding_epoch()const noexcept override{return source_.binding_epoch();}
    bool read_fresh(std::span<native_joints::ReadRequest> requests)override{
        u64 bytes=0;for(const auto& r:requests)bytes+=r.size;
        if(bytes>remaining_bytes_||requests.size()>remaining_requests_){exhausted_=true;return false;}
        remaining_bytes_-=bytes;remaining_requests_-=requests.size();stats.bytes+=bytes;stats.requests+=requests.size();++stats.fresh_calls;
        return source_.read_fresh(requests);
    }
};
struct Work{Entity* entity=nullptr;native_joints::Target target{};native_joints::Demand demand=0;};
namespace wire=native_joints::detail;
using Failure=native_joints::Failure;
// Shared dependency stages; no thread workers and no prior frame/stage replay.
class Bank{
    struct Entry{u64 entity=0,identity=0,next_retry=0,last_selected=0;u32 handle=0;unsigned failures=0;bool metadata=false;wire::MetadataKey key{};std::array<wire::Descriptor,2> descriptors{};std::optional<native_joint_quarantine::Plan> plan;};
    struct Actor{Work* work=nullptr;Entry* entry=nullptr;Failure failure=Failure::None;wire::Snapshot a{},b{};std::array<wire::SelectedRead,2> first{},second{};u64 started_ms=0,completed_ms=0;bool warm=false;};
    struct Tagged{size_t actor=0;native_joints::ReadRequest read;};
    std::unique_ptr<Source> owned_source_;BudgetSource source_;native_joints::Reader verifier_;std::unordered_map<u64,Entry> entries_;
    u64 epoch_=0,verify_retry_=0,selection_turn_=0;size_t cursor_=0;bool verified_=false,version_rejected_=false;
    static bool live(const Actor& a){return a.failure==Failure::None;}
    static void fail(Actor& a,Failure f){if(live(a))a.failure=f;}
    static bool negative_metadata(Failure f){return f==Failure::MissingName||f==Failure::AmbiguousName||f==Failure::InvalidName||f==Failure::NotHitbox||f==Failure::InvalidScene||f==Failure::InvalidModel||f==Failure::InvalidCount;}
    void add(std::vector<Tagged>& q,std::vector<Actor>& a,size_t i,native_joints::ReadRequest r){if(!live(a[i]))return;if(!r.destination||!wire::valid_span(r.address,r.size)){fail(a[i],Failure::InvalidAddress);return;}q.push_back({i,r});}
    template<class T>void add(std::vector<Tagged>& q,std::vector<Actor>& a,size_t i,u64 at,T& v){add(q,a,i,wire::request(at,v));}
    void run(std::vector<Tagged>& q,std::vector<Actor>& actors){
        std::vector<native_joints::ReadRequest> requests;std::vector<size_t> owner;
        for(auto& t:q)if(live(actors[t.actor])){requests.push_back(t.read);owner.push_back(t.actor);}
        for(size_t begin=0;begin<requests.size();begin+=256){const auto n=std::min<size_t>(256,requests.size()-begin);auto part=std::span<native_joints::ReadRequest>(requests).subspan(begin,n);const auto epoch=source_.binding_epoch();
            if(std::none_of(owner.begin()+begin,owner.begin()+begin+n,[&](size_t i){return live(actors[i]);}))continue;
            if(!epoch||epoch!=epoch_){for(size_t j=0;j<n;++j)fail(actors[owner[begin+j]],Failure::BindingChanged);continue;}
            const bool executed=source_.read_fresh(part),same=source_.binding_epoch()==epoch;
            for(size_t j=0;j<n;++j)if(!same)fail(actors[owner[begin+j]],Failure::BindingChanged);else if(!executed||!part[j].valid||part[j].transferred!=part[j].size)fail(actors[owner[begin+j]],source_.budget_exhausted()?Failure::BudgetExceeded:Failure::ReadFailed);
        }q.clear();
    }
    static void observe(Actor& a,const wire::Snapshot& s){auto& e=*a.entry;if(!e.metadata)return;if(s.actor_context_captured&&(e.key.identity!=s.identity||e.key.scene!=s.scene||e.key.handle!=s.handle))e.metadata=false;if(e.metadata&&s.metadata_captured&&!(e.key==wire::key(s)))e.metadata=false;if(!e.metadata)e.plan.reset();}
    static u64 millis(native_joints::HostClock::time_point value){return static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch()).count());}
    void warm(std::vector<Actor>& actors){
        std::vector<native_joint_quarantine::Row> rows;std::vector<size_t> owner;
        for(size_t i=0;i<actors.size();++i)if(actors[i].warm){native_joint_quarantine::Row row;row.plan=&*actors[i].entry->plan;rows.push_back(std::move(row));owner.push_back(i);}
        native_joint_quarantine::Runner::sample(source_,rows);
        for(size_t k=0;k<rows.size();++k){auto& a=actors[owner[k]];const auto& row=rows[k];a.failure=row.failure;a.a=row.first.state;a.b=row.second.state;a.started_ms=millis(row.started_at);a.completed_ms=millis(row.completed_at);
            const auto j=a.work->demand==native_joints::head?0:1;a.first[j]=row.first.leaf;a.second[j]=row.second.leaf;if(row.needs_cold){a.entry->plan.reset();++source_.stats.plan_discards;}
        }
    }
    void capture(std::vector<Actor>& actors,bool after,u64 module){
        std::vector<Tagged> q;auto state=[&](Actor& a)->wire::Snapshot&{return after?a.b:a.a;};
        // Stage 1: entity identity and scene roots across all admitted actors.
        for(size_t i=0;i<actors.size();++i){auto& a=actors[i];if(a.warm||!live(a))continue;auto& s=state(a);const auto& t=a.work->target;if(!wire::valid_span(t.entity,wire::entity_scene_offset+8)||!wire::valid_span(t.expected_identity,wire::identity_flags_offset+4)){fail(a,Failure::InvalidAddress);continue;}add(q,actors,i,t.entity+wire::entity_identity_offset,s.identity);add(q,actors,i,t.entity+wire::entity_scene_offset,s.scene);}run(q,actors);
        // Stage 2: identity/type, cache roots/mask/lock and condition fields.
        for(size_t i=0;i<actors.size();++i){auto& a=actors[i];if(a.warm||!live(a))continue;auto& s=state(a);if(s.identity!=a.work->target.expected_identity){fail(a,Failure::IdentityMismatch);continue;}if(!wire::valid_span(s.scene,wire::computed_mask_offset+4)){fail(a,Failure::InvalidScene);continue;}
            add(q,actors,i,s.identity+wire::identity_handle_offset,s.handle);add(q,actors,i,s.identity+wire::identity_flags_offset,s.identity_flags);
            add(q,actors,i,s.scene,s.vtable);add(q,actors,i,s.scene+wire::scene_owner_offset,s.owner);add(q,actors,i,s.scene+wire::scene_lock_offset,s.lock);
            add(q,actors,i,s.scene+wire::model_record_offset,s.model_record);add(q,actors,i,s.scene+wire::pose_pointer_offset,s.poses);add(q,actors,i,s.scene+wire::bitmap_pointer_offset,s.bitmap);add(q,actors,i,s.scene+wire::pose_count_offset,s.pose_count);add(q,actors,i,s.scene+wire::computed_mask_offset,s.computed_mask);add(q,actors,i,s.scene+wire::condition_pointer_offset,s.condition_pointer);add(q,actors,i,s.scene+wire::condition_mask_offset,s.condition_mask);
        }run(q,actors);
        // Stage 3: model object and optional global object in one acquisition.
        for(size_t i=0;i<actors.size();++i){auto& a=actors[i];if(a.warm||!live(a))continue;auto& s=state(a);
            if(s.handle!=a.work->target.expected_handle||(s.identity_flags&wire::rejected_identity_flags)){fail(a,Failure::IdentityMismatch);continue;}if(s.vtable!=module+wire::scene_vtable_rva||s.owner!=a.work->target.entity){fail(a,Failure::InvalidScene);continue;}s.actor_context_captured=true;observe(a,s);
            if(!s.lock.quiet()){fail(a,Failure::Locked);continue;}if(!wire::valid_count(s.pose_count,1024)){fail(a,Failure::InvalidCount);continue;}
            if(!wire::valid_span(s.model_record,8)||!wire::valid_span(s.poses,u64(s.pose_count)*32)||(s.poses&15)||!wire::valid_span(s.bitmap,((u64(s.pose_count)+31)/32)*4)){fail(a,Failure::InvalidModel);continue;}
            add(q,actors,i,s.model_record,s.model);if(s.condition_pointer&&(s.condition_mask&0x100))add(q,actors,i,module+wire::condition_object_pointer_rva,s.condition_object);
        }run(q,actors);
        // Stage 4: model metadata and optional global object byte together.
        for(size_t i=0;i<actors.size();++i){auto& a=actors[i];if(a.warm||!live(a))continue;auto& s=state(a);if(!wire::valid_span(s.model,wire::flags_pointer_offset+8)){fail(a,Failure::InvalidModel);continue;}
            add(q,actors,i,s.model+wire::model_count_offset,s.model_count);add(q,actors,i,s.model+wire::names_pointer_offset,s.names);add(q,actors,i,s.model+wire::flags_pointer_offset,s.flags);
            if(s.condition_pointer&&(s.condition_mask&0x100)){if(!wire::valid_span(s.condition_object,0x59)){fail(a,Failure::InvalidModel);continue;}add(q,actors,i,s.condition_object+0x58,s.condition_object_byte);}
        }run(q,actors);
        // Stages 5/6: only dependencies required by the native short circuit.
        for(size_t i=0;i<actors.size();++i){auto& a=actors[i];if(a.warm||!live(a))continue;auto& s=state(a);if(!wire::valid_count(s.model_count,1024)){fail(a,Failure::InvalidCount);continue;}if(!wire::valid_span(s.names,u64(s.model_count)*8)||!wire::valid_span(s.flags,u64(s.model_count)*4)){fail(a,Failure::InvalidModel);continue;}s.metadata_captured=true;observe(a,s);if(s.condition_pointer&&(s.condition_mask&0x100)&&s.condition_object_byte)add(q,actors,i,module+wire::condition_dword_rva,s.condition_dword);}run(q,actors);
        for(size_t i=0;i<actors.size();++i){auto& a=actors[i];auto& s=state(a);if(!a.warm&&live(a)&&s.condition_pointer&&(s.condition_mask&0x100)&&s.condition_object_byte&&s.condition_dword==-1)add(q,actors,i,module+wire::condition_byte_rva,s.condition_byte);}run(q,actors);
        for(auto& a:actors)if(!a.warm&&live(a)){const auto& s=state(a);if(!wire::mask_ready(s.computed_mask,s.request()))fail(a,Failure::CacheNotReady);}
    }
    void metadata(std::vector<Actor>& actors){
        size_t chosen=actors.size();for(size_t i=0;i<actors.size();++i)if(!actors[i].warm&&live(actors[i])&&!actors[i].entry->metadata){if(chosen==actors.size())chosen=i;else{fail(actors[i],Failure::BudgetExceeded);++source_.stats.budget_skipped;}}
        if(chosen==actors.size())return;auto& a=actors[chosen];auto& e=*a.entry;const auto& s=a.a;++source_.stats.warmups;
        const auto n=static_cast<size_t>(s.model_count);std::vector<u64> pointers(n);std::vector<u32> flags(n);std::vector<std::array<char,wire::name_capacity>> names(n);std::vector<Tagged> q;
        add(q,actors,chosen,wire::bytes(s.names,pointers.data(),n*8));add(q,actors,chosen,wire::bytes(s.flags,flags.data(),n*4));run(q,actors);
        for(size_t j=0;j<n;++j)add(q,actors,chosen,wire::bytes(pointers[j],names[j].data(),names[j].size()));run(q,actors);
        if(!live(a))return;e.descriptors={};constexpr std::array<std::string_view,2> expected={"head","neck_0"};
        for(size_t j=0;j<n;++j){std::string name;if(!wire::parse_name(names[j],name)){fail(a,Failure::InvalidName);return;}for(size_t k=0;k<2;++k)if(name==expected[k]){auto& d=e.descriptors[k];++d.matches;d.index=(int)j;d.name=pointers[j];d.flags=flags[j];d.exact_name=name;}}
        e.key=wire::key(s);e.metadata=true;
    }
    void selected(std::vector<Actor>& actors,bool second){
        std::vector<Tagged> q;
        for(size_t i=0;i<actors.size();++i){auto& a=actors[i];if(a.warm||!live(a))continue;const auto& s=a.a;auto& result=second?a.second:a.first;
            for(unsigned j=0;j<2;++j)if(a.work->demand&(1u<<j)){const auto& d=a.entry->descriptors[j];if(d.matches!=1){fail(a,d.matches?Failure::AmbiguousName:Failure::MissingName);break;}if(!(d.flags&0x100)){fail(a,Failure::NotHitbox);break;}if(d.index<0||d.index>=s.model_count||d.index>=s.pose_count){fail(a,Failure::InvalidCount);break;}
                auto& r=result[j];const auto index=static_cast<u32>(d.index);add(q,actors,i,s.names+u64(index)*8,r.name);add(q,actors,i,s.flags+u64(index)*4,r.flags);add(q,actors,i,s.bitmap+u64(index>>5)*4,r.bitmap_word);add(q,actors,i,s.poses+u64(index)*32,r.pose);
                // The expected string can share this acquisition with its fresh
                // table entry. A changed entry rejects all of this actor's data.
                add(q,actors,i,wire::bytes(d.name,r.name_bytes.data(),r.name_bytes.size()));
            }
        }run(q,actors);
        for(auto& a:actors)if(!a.warm&&live(a)){auto& result=second?a.second:a.first;for(unsigned j=0;j<2;++j)if(a.work->demand&(1u<<j)){const auto& d=a.entry->descriptors[j];auto& r=result[j];
            if(r.name!=d.name||r.flags!=d.flags){fail(a,Failure::MetadataChanged);break;}if(!(r.bitmap_word&(1u<<(u32(d.index)&31)))){fail(a,Failure::CacheNotReady);break;}if(!wire::finite_pose(r.pose)){fail(a,Failure::NonFiniteTransform);break;}if(!wire::parse_name(r.name_bytes,r.name_string)){fail(a,Failure::InvalidName);break;}if(r.name_string!=d.exact_name){fail(a,Failure::MetadataChanged);break;}
        }}
    }
public:
    explicit Bank(::Reader& reader):owned_source_(std::make_unique<Source>(reader)),source_(*owned_source_),verifier_(source_){}
    // The same actual protocol can be compiled/exercised with a host-only byte
    // fixture; production uses the Reader overload above, never fake poses.
    explicit Bank(native_joints::FreshSource& source):source_(source),verifier_(source_){}
    void reset(){entries_.clear();epoch_=0;cursor_=0;verify_retry_=0;selection_turn_=0;verified_=version_rejected_=false;verifier_.reset();}
    Stats sample(u64 module,u64 epoch,std::vector<Work>& work){
        const auto started=now_ms();source_.begin();for(auto& w:work)if(w.entity)w.entity->native_joints={};if(epoch_!=epoch){reset();epoch_=epoch;}
        // This counter orders bounded host metadata eviction only; it is not a
        // native model generation. Saturation drops host metadata and retries.
        if(selection_turn_==std::numeric_limits<u64>::max()){reset();epoch_=epoch;}++selection_turn_;
        std::unordered_set<u32> current;for(const auto& w:work){current.insert(w.target.expected_handle);if(w.entity&&w.demand)++source_.stats.demanded;}for(auto i=entries_.begin();i!=entries_.end();)if(!current.count(i->second.handle))i=entries_.erase(i);else ++i;
        auto finish=[&](){source_.stats.elapsed_ms=now_ms()-started;return source_.stats;};
        if(!source_.stats.demanded||!epoch||source_.binding_epoch()!=epoch||work.empty())return finish();if(version_rejected_){source_.stats.first_failure=Failure::WrongVersion;return finish();}
        if(!verified_){if(now_ms()<verify_retry_){source_.stats.backoff_skipped=source_.stats.demanded;return finish();}if(!verifier_.bind(module)){source_.stats.first_failure=verifier_.last_failure();version_rejected_=verifier_.last_failure()==Failure::WrongVersion;verify_retry_=now_ms()+250;return finish();}verified_=true;}
        std::vector<Actor> actors;actors.reserve(128);std::unordered_set<u32> admitted;const auto count=work.size(),begin=cursor_%count;
        for(size_t n=0;n<count;++n){auto& w=work[(begin+n)%count];if(!w.entity||!w.demand)continue;if(w.demand!=native_joints::head&&w.demand!=native_joints::neck)continue;
            const u64 key=(u64(w.target.expected_handle)<<2)|w.demand;
            if(actors.size()>=128||(admitted.size()>=64&&!admitted.count(w.target.expected_handle))){++source_.stats.budget_skipped;continue;}
            if(entries_.size()>=128&&entries_.find(key)==entries_.end()){
                // Do not permanently pin the first actors when more current
                // actors demand points. Never erase an Entry referenced by an
                // already admitted row; its proof/pose lifetime spans the poll.
                auto victim=entries_.end();for(auto i=entries_.begin();i!=entries_.end();++i)if(!admitted.count(i->second.handle)&&(victim==entries_.end()||i->second.last_selected<victim->second.last_selected))victim=i;
                if(victim==entries_.end()){++source_.stats.budget_skipped;continue;}entries_.erase(victim);
            }
            auto& e=entries_[key];if(e.entity!=w.target.entity||e.identity!=w.target.expected_identity||e.handle!=w.target.expected_handle){e={};e.entity=w.target.entity;e.identity=w.target.expected_identity;e.handle=w.target.expected_handle;}e.last_selected=selection_turn_;if(now_ms()<e.next_retry){++source_.stats.backoff_skipped;continue;}
            Actor a;a.work=&w;a.entry=&e;a.started_ms=now_ms();a.warm=e.metadata&&e.plan.has_value();actors.push_back(std::move(a));++source_.stats.attempted;
            if(actors.back().warm)++source_.stats.warm_rows;else ++source_.stats.cold_rows;
            admitted.insert(w.target.expected_handle);
        }
        warm(actors);capture(actors,false,module);metadata(actors);selected(actors,false);selected(actors,true);capture(actors,true,module);
        const bool same_epoch=source_.binding_epoch()==epoch;const auto completed=now_ms();
        for(auto& a:actors){auto& e=*a.entry;if(!same_epoch)fail(a,Failure::BindingChanged);if(live(a)&&!wire::same_snapshot(a.a,a.b))fail(a,wire::key(a.a)==wire::key(a.b)?Failure::Inconsistent:Failure::MetadataChanged);
            if(live(a))for(unsigned j=0;j<2;++j)if(a.work->demand&(1u<<j)){const auto& x=a.first[j];const auto& y=a.second[j];if(x.name!=y.name||x.flags!=y.flags||x.name_string!=y.name_string){fail(a,Failure::MetadataChanged);break;}if(x.bitmap_word!=y.bitmap_word||std::memcmp(&x.pose,&y.pose,sizeof(x.pose))){fail(a,Failure::Inconsistent);break;}}
            if(!live(a)){if(source_.stats.first_failure==Failure::None)source_.stats.first_failure=a.failure;if(wire::invalidates_metadata(a.failure)){e.metadata=false;e.plan.reset();}if(negative_metadata(a.failure)){e.failures=std::min(e.failures+1,5u);e.next_retry=completed+std::min<u64>(5000,250ULL<<(e.failures-1));}continue;}
            const unsigned selected_index=a.work->demand==native_joints::head?0:1;
            if(!a.warm){native_joint_quarantine::Plan plan;plan.target=a.work->target;plan.module=module;plan.epoch=epoch;plan.expected=native_joint_quarantine::dependencies(a.a);plan.selected=e.descriptors[selected_index];e.plan=std::move(plan);a.completed_ms=completed;}
            e.failures=0;e.next_retry=0;auto& out=a.work->entity->native_joints;out.binding_epoch=epoch;out.started_ms=out.valid_mask?std::min(out.started_ms,a.started_ms):a.started_ms;out.completed_ms=std::max(out.completed_ms,a.completed_ms);
            for(unsigned j=0;j<2;++j)if(a.work->demand&(1u<<j)){const auto& p=a.second[j].pose;out.valid_mask|=1u<<j;out.model_index[j]=e.descriptors[j].index;out.world[j]={p.x,p.y,p.z};}++source_.stats.accepted;
        }
        cursor_=(begin+1)%count;if(!same_epoch){for(auto& w:work)if(w.entity)w.entity->native_joints={};reset();}return finish();
    }
};
} // namespace native_joint_backend

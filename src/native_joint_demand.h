#pragma once
#include "native_joint_reader.h"
#include "entities.h"
#include "config.h"
#include "world_units.h"
#include <atomic>

// Short host leases from real activated consumers. No activation state is
// reconstructed in the backend, and no lease authorizes input or eligibility.
namespace native_joint_demand {
enum class Consumer { Aim, Trigger, Magnet };
struct Request{unsigned mask=0;float maximum_distance_units=0;};
// Zero is explicit legacy inheritance. Existing profiles preserve their former
// Aim-range cap; a positive component override is independently meaningful.
inline constexpr float effective_distance(float configured,float inherited){
    const auto value=configured==0?inherited:configured;
    return native_joints::detail::finite(value)&&value>0?value:0;
}
inline float trigger_range(){return world_units::units(effective_distance(cfg.F("trigger.max_dist"),cfg.F("aim.max_dist")));}
inline float magnet_range(){return world_units::units(effective_distance(cfg.F("trigger.magnet_max_dist"),cfg.F("aim.max_dist")));}
template<std::size_t N>inline constexpr unsigned at_distance(const std::array<Request,N>& requests,float distance){
    if(!native_joints::detail::finite(distance)||distance<0)return 0;
    unsigned mask=0;for(const auto& r:requests)if(native_joints::detail::finite(r.maximum_distance_units)&&r.maximum_distance_units>0&&distance<=r.maximum_distance_units)mask|=r.mask;
    return mask&native_joints::supported_demand;
}
inline constexpr unsigned visual_head(bool wanted,bool player,bool valid,float maximum_distance,float distance){
    return wanted&&player&&valid&&native_joints::detail::finite(distance)&&distance>=0&&native_joints::detail::finite(maximum_distance)&&maximum_distance>0&&distance<=maximum_distance?native_joints::head:0;
}
struct Lease {
    std::atomic<u64> sequence{0},epoch{0},until_ms{0};
    std::atomic<unsigned> mask{0};
    std::atomic<float> maximum_distance_units{0};
    void publish(unsigned requested,u64 binding,u64 now,float maximum_distance=std::numeric_limits<float>::max()) {
        if(!std::isfinite(maximum_distance)||maximum_distance<=0)requested=0;
        sequence.fetch_add(1,std::memory_order_acq_rel);
        // A payload value read from a concurrent publication must order the
        // writer's preceding odd sequence before the final sequence load.
        // Relaxed payload access alone does not establish that portable proof.
        epoch.store(binding,std::memory_order_release);
        mask.store(requested&native_joints::supported_demand,std::memory_order_release);
        maximum_distance_units.store(requested?maximum_distance:0,std::memory_order_release);
        until_ms.store(requested&&binding?now+100:0,std::memory_order_release);
        sequence.fetch_add(1,std::memory_order_release);
    }
    Request request(u64 binding,u64 now) const {
        const auto before=sequence.load(std::memory_order_acquire);if(before&1)return {};
        const auto at=epoch.load(std::memory_order_acquire),until=until_ms.load(std::memory_order_acquire);
        const auto requested=mask.load(std::memory_order_acquire);
        const auto range=maximum_distance_units.load(std::memory_order_acquire);
        const auto after=sequence.load(std::memory_order_acquire);
        return before==after&&!(after&1)&&binding&&at==binding&&until>=now&&until-now<=100?Request{requested,range}:Request{};
    }
    unsigned read(u64 binding,u64 now)const{return request(binding,now).mask;}
};
inline Lease aim,trigger,magnet;
inline Lease& lease(Consumer consumer){return consumer==Consumer::Aim?aim:consumer==Consumer::Trigger?trigger:magnet;}
class Tick {
    Consumer consumer_;u64 epoch_;unsigned requested_=0;float maximum_distance_=0;
public:
    explicit Tick(Consumer consumer):consumer_(consumer),epoch_(snap.native_binding_epoch.load()){}
    ~Tick(){lease(consumer_).publish(requested_,epoch_,now_ms(),maximum_distance_);}
    void request(unsigned mask,float maximum_distance){requested_|=mask&native_joints::supported_demand;maximum_distance_=std::max(maximum_distance_,maximum_distance);}
};
inline std::array<Request,3> action_requests(u64 epoch,u64 now){return {aim.request(epoch,now),trigger.request(epoch,now),magnet.request(epoch,now)};}
inline unsigned selected_regions(){
    unsigned mask=0;
    if(cfg.I("aim.bone_head")>=0)mask|=native_joints::head;
    if(cfg.I("aim.bone_neck")>=0)mask|=native_joints::neck;
    return mask;
}
} // namespace native_joint_demand

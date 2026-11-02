// Compiler-only proposal. Compile this translation unit with /c or syntax-only.
// Do not link/run it. It makes no device, game, OS, file, or thread calls.
#include "output_queue_policy.h"
using namespace output_queue;

constexpr bool coalescing_retains_due_time_and_latest_counts(){
    Queue queue;queue.set_activity(Origin::Aim,true,100);
    Request request;request.kind=Kind::Move;request.origin=Origin::Aim;request.first=3;
    if(queue.push(request,{100,7,true,true},10,1)!=Push::Queued)return false;
    request.first=9;request.second=-2;
    if(queue.push(request,{105,7,true,true},10,1)!=Push::Coalesced||queue.size()!=1)return false;
    Request taken;if(queue.take({109,7,true,true},0,taken))return false;
    return queue.take({110,7,true,true},0,taken)&&taken.first==9&&taken.second==-2&&taken.ready_ms==110&&taken.submitted_ms==105;
}
static_assert(coalescing_retains_due_time_and_latest_counts());

constexpr bool cancellation_is_owner_specific(){
    Queue queue;queue.set_activity(Origin::Aim,true,100);queue.set_activity(Origin::Trigger,true,100);
    Request first;first.origin=Origin::Aim;first.kind=Kind::Tap;first.first=0;
    Request second=first;second.origin=Origin::Trigger;second.first=1;
    if(queue.push(first,{100,8,true,true},0,0)!=Push::Queued||queue.push(second,{100,8,true,true},0,0)!=Push::Queued)return false;
    auto aim_generation=queue.generation(Origin::Aim),trigger_generation=queue.generation(Origin::Trigger);
    Held aim{true,Origin::Aim,aim_generation,8,0,1,0},trigger{true,Origin::Trigger,trigger_generation,8,0,2,0};
    queue.cancel(Origin::Aim);
    Request taken;
    return queue.size()==1&&queue.release_due(aim,{101,8,true,true})&&!queue.release_due(trigger,{101,8,true,true})&&
        queue.take({101,8,true,true},0,taken)&&taken.origin==Origin::Trigger;
}
static_assert(cancellation_is_owner_specific());

constexpr bool release_debt_survives_full_queue_and_gate_changes(){
    Queue queue;Request request;request.kind=Kind::Tap;request.first=5;
    for(std::size_t index=0;index<capacity;++index)if(queue.push(request,{100,9,true,true},0,0)!=Push::Queued)return false;
    if(queue.push(request,{100,9,true,true},0,0)!=Push::Rejected)return false;
    Held held{true,Origin::General,queue.generation(Origin::General),9,300,1,0};
    if(queue.release_due(held,{299,9,true,true})||!queue.release_due(held,{300,9,true,true}))return false;
    // Owed releases ignore pending capacity, permission, connection and epoch.
    return queue.release_due(held,{101,9,true,false})&&queue.release_due(held,{101,9,false,true})&&queue.release_due(held,{101,10,true,true});
}
static_assert(release_debt_survives_full_queue_and_gate_changes());

constexpr bool stale_attachment_and_expired_commands_never_dispatch(){
    Queue queue;Request request;request.kind=Kind::Tap;Request taken;
    if(queue.push(request,{100,7,true,true},0,0)!=Push::Queued)return false;
    if(queue.take({101,8,true,true},0,taken)||queue.size())return false;
    if(queue.push(request,{100,8,true,true},0,0)!=Push::Queued)return false;
    return !queue.take({351,8,true,true},0,taken)&&queue.size()==0;
}
static_assert(stale_attachment_and_expired_commands_never_dispatch());

constexpr bool lease_renewal_after_gap_cancels_old_hold(){
    Queue queue;queue.set_activity(Origin::Aim,true,100);
    Held held{true,Origin::Aim,queue.generation(Origin::Aim),7,0,1,0};
    if(queue.release_due(held,{350,7,true,true}))return false;
    queue.set_activity(Origin::Aim,true,351);
    return queue.active(Origin::Aim,351)&&queue.release_due(held,{351,7,true,true});
}
static_assert(lease_renewal_after_gap_cancels_old_hold());

constexpr bool spacing_and_resource_cancellation(){
    Queue queue;queue.set_activity(Origin::Aim,true,100);queue.set_activity(Origin::Trigger,true,100);
    Request first;first.origin=Origin::Aim;first.kind=Kind::Chord;first.first=0;first.second=5;
    Request second=first;second.origin=Origin::Trigger;
    if(queue.push(first,{100,7,true,true},0,20)!=Push::Queued||queue.push(second,{100,7,true,true},0,20)!=Push::Queued)return false;
    queue.cancel_resource(Origin::Aim,0);Request taken;
    if(queue.size()!=1||queue.take({119,7,true,true},100,taken))return false;
    return queue.take({120,7,true,true},100,taken)&&taken.origin==Origin::Trigger;
}
static_assert(spacing_and_resource_cancellation());

constexpr bool explicit_up_invalidates_already_popped_down(){
    Queue queue;queue.set_activity(Origin::Aim,true,100);queue.set_activity(Origin::Trigger,true,100);
    Request request;request.kind=Kind::Down;request.origin=Origin::Aim;request.first=0;
    if(queue.push(request,{100,7,true,true},0,0)!=Push::Queued)return false;
    Request popped;if(!queue.take({100,7,true,true},0,popped))return false;
    queue.cancel_resource(Origin::Aim,0);
    if(queue.valid(popped,{101,7,true,true}))return false;
    request.origin=Origin::Trigger;
    return queue.push(request,{101,7,true,true},0,0)==Push::Queued;
}
static_assert(explicit_up_invalidates_already_popped_down());

constexpr bool source_image_age_is_independent_of_submission_age(){
    Queue queue;Request request;request.kind=Kind::Tap;request.source_sample_ms=100;Request taken;
    if(queue.push(request,{340,7,true,true},0,0)!=Push::Queued)return false;
    if(queue.take({351,7,true,true},0,taken)||queue.size())return false;
    if(queue.push(request,{351,7,true,true},0,0)!=Push::Rejected)return false;
    request.source_sample_ms=400;
    if(queue.push(request,{399,7,true,true},0,0)!=Push::Rejected)return false;
    // No-image General commands retain a zero optional source-sample timestamp.
    request.source_sample_ms=0;
    return queue.push(request,{399,7,true,true},0,0)==Push::Queued;
}
static_assert(source_image_age_is_independent_of_submission_age());

#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace output_queue {
enum class Origin : unsigned char { General, Aim, Trigger };
enum class Kind : unsigned char { Move, Down, Tap, Chord };
enum class Push : unsigned char { Rejected, Queued, Coalesced };
inline constexpr std::uint64_t lease_ms=250,command_ttl_ms=250;
inline constexpr std::size_t capacity=32,resource_count=261,max_held=8;
constexpr std::size_t owner(Origin value){return static_cast<std::size_t>(value);}
constexpr bool valid_owner(Origin value){return owner(value)<3;}
struct Context {std::uint64_t now=0,attachment=0;bool connected=false,permitted=false;};
struct Request {
    Kind kind=Kind::Move;Origin origin=Origin::General;
    int first=0,second=0,hold_ms=0;unsigned spacing_ms=0;
    std::uint64_t generation=0,attachment=0,submitted_ms=0,ready_ms=0,source_sample_ms=0;
    std::uint64_t first_release_generation=0,second_release_generation=0;
};
struct Held {
    bool active=false;Origin origin=Origin::General;
    std::uint64_t generation=0,attachment=0,release_ms=0,order=0;unsigned release_failures=0;
};
// Pure bounded policy. Its owner serializes access. Release state is separate
// from pending capacity, so a full queue can never reject an owed release.
class Queue {
    std::array<Request,capacity> pending_{};std::size_t size_=0;
    std::array<std::uint64_t,3> generation_{1,1,1},lease_until_{};
    std::array<std::array<std::uint64_t,5>,3> release_generation_{};
    constexpr void erase(std::size_t at){for(std::size_t i=at+1;i<size_;++i)pending_[i-1]=pending_[i];--size_;}
public:
    constexpr std::size_t size()const{return size_;}
    constexpr std::uint64_t generation(Origin origin)const{return generation_[owner(origin)];}
    constexpr bool active(Origin origin,std::uint64_t now)const{
        return valid_owner(origin)&&(origin==Origin::General||(lease_until_[owner(origin)]&&now<=lease_until_[owner(origin)]));
    }
    constexpr void cancel(Origin origin){
        if(!valid_owner(origin))return;auto index=owner(origin);
        if(++generation_[index]==0)++generation_[index];lease_until_[index]=0;
        for(std::size_t i=0;i<size_;)if(pending_[i].origin==origin)erase(i);else ++i;
    }
    constexpr void cancel_all(){cancel(Origin::General);cancel(Origin::Aim);cancel(Origin::Trigger);}
    constexpr void set_activity(Origin origin,bool enabled,std::uint64_t now){
        if(!valid_owner(origin))return;
        if(!enabled){cancel(origin);return;}
        if(origin==Origin::General)return;
        auto index=owner(origin);if(lease_until_[index]&&now>lease_until_[index])cancel(origin);
        lease_until_[index]=now+lease_ms;
    }
    constexpr void expire(std::uint64_t now){
        for(unsigned index=1;index<3;++index)if(lease_until_[index]&&now>lease_until_[index])cancel(static_cast<Origin>(index));
    }
    constexpr bool token_valid(Origin origin,std::uint64_t generation,std::uint64_t attachment,const Context& context)const{
        return context.connected&&context.permitted&&valid_owner(origin)&&active(origin,context.now)&&generation==generation_[owner(origin)]&&attachment==context.attachment;
    }
    constexpr bool valid(const Request& request,const Context& context)const{
        if(!token_valid(request.origin,request.generation,request.attachment,context)||context.now<request.submitted_ms||context.now-request.submitted_ms>command_ttl_ms)return false;
        if(request.source_sample_ms&&(context.now<request.source_sample_ms||context.now-request.source_sample_ms>command_ttl_ms))return false;
        if(request.kind!=Kind::Move){
            const auto& generations=release_generation_[owner(request.origin)];
            if(request.first>=0&&request.first<5&&request.first_release_generation!=generations[(std::size_t)request.first])return false;
            if(request.kind==Kind::Chord&&request.second>=0&&request.second<5&&request.second_release_generation!=generations[(std::size_t)request.second])return false;
        }return true;
    }
    constexpr bool release_due(const Held& held,const Context& context)const{
        return held.active&&((held.release_ms&&context.now>=held.release_ms)||!token_valid(held.origin,held.generation,held.attachment,context));
    }
    constexpr Push push(Request request,const Context& context,unsigned latency_ms,unsigned spacing_ms){
        if(!valid_owner(request.origin)||!context.connected||!context.permitted||!active(request.origin,context.now))return Push::Rejected;
        request.generation=generation(request.origin);request.attachment=context.attachment;request.submitted_ms=context.now;
        request.ready_ms=context.now+latency_ms;request.spacing_ms=spacing_ms;
        if(request.kind!=Kind::Move){
            const auto& generations=release_generation_[owner(request.origin)];
            if(request.first>=0&&request.first<5)request.first_release_generation=generations[(std::size_t)request.first];
            if(request.kind==Kind::Chord&&request.second>=0&&request.second<5)request.second_release_generation=generations[(std::size_t)request.second];
        }
        if(!valid(request,context))return Push::Rejected;
        if(size_&&request.kind==Kind::Move){
            auto& tail=pending_[size_-1];
            if(tail.kind==Kind::Move&&tail.origin==request.origin&&tail.generation==request.generation&&tail.attachment==request.attachment){
                // Latest counts replace unsent counts; retaining the first due
                // time prevents a stream of replacements postponing forever.
                request.ready_ms=tail.ready_ms;tail=request;return Push::Coalesced;
            }
        }
        if(size_==capacity)return Push::Rejected;pending_[size_++]=request;return Push::Queued;
    }
    constexpr bool take(const Context& context,std::uint64_t last_sent,Request& request){
        while(size_){
            const auto& head=pending_[0];if(!valid(head,context)){erase(0);continue;}
            auto spaced=last_sent?last_sent+head.spacing_ms:0;auto due=head.ready_ms>spaced?head.ready_ms:spaced;
            if(context.now<due)return false;request=head;erase(0);return true;
        }return false;
    }
    constexpr void cancel_resource(Origin origin,int resource){
        if(!valid_owner(origin)||resource<0||resource>=5)return;
        auto& generation=release_generation_[owner(origin)][(std::size_t)resource];if(++generation==0)++generation;
        for(std::size_t i=0;i<size_;){const auto& request=pending_[i];
            bool matches=request.origin==origin&&request.kind!=Kind::Move&&(request.first==resource||(request.kind==Kind::Chord&&request.second==resource));
            if(matches)erase(i);else ++i;
        }
    }
};
}

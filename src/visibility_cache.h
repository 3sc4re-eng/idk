#pragma once
#include "collision.h"
#include <array>
#include <cmath>
#include <cstring>
#include <unordered_map>

// Owned by one snapshot-publishing thread. This caches ray answers, not target
// visibility flags; projection and team state are evaluated on every snapshot.
namespace visibility_cache{
struct Answer{bool known=false,clear=false;};
class Rays{
    struct Key{
        std::array<u32,6> points{};u64 scene=0,obstacles=0;unsigned mask=0;
        bool operator==(const Key& other)const{return points==other.points&&scene==other.scene&&obstacles==other.obstacles&&mask==other.mask;}
    };
    struct Hash{
        size_t operator()(const Key& key)const{u64 hash=1469598103934665603ULL;auto mix=[&](u64 value){hash=(hash^value)*1099511628211ULL;};for(auto value:key.points)mix(value);mix(key.scene);mix(key.obstacles);mix(key.mask);return (size_t)hash;}
    };
    struct Entry{Answer answer;u64 at=0;};
    std::unordered_map<Key,Entry,Hash> values_;
    static bool make_key(Vec3 from,Vec3 to,collision::Revision revision,unsigned mask,Key& key){
        const float values[]={from.x,from.y,from.z,to.x,to.y,to.z};
        for(size_t i=0;i<6;i++){if(!std::isfinite(values[i]))return false;static_assert(sizeof(float)==sizeof(u32));std::memcpy(&key.points[i],&values[i],sizeof(float));}
        key.scene=revision.scene;key.obstacles=revision.obstacles;key.mask=mask;return mask!=0;
    }
    void prune(u64 now){for(auto it=values_.begin();it!=values_.end();)if(now<it->second.at||now-it->second.at>ttl_ms)it=values_.erase(it);else ++it;}
public:
    static constexpr u64 ttl_ms=50;static constexpr size_t capacity=1024;
    void clear(){values_.clear();}
    size_t size()const{return values_.size();}
    bool find(Vec3 from,Vec3 to,collision::Revision revision,unsigned mask,u64 now,Answer& answer){
        answer={};Key key;if(!make_key(from,to,revision,mask,key))return false;
        auto found=values_.find(key);if(found==values_.end())return false;
        if(now<found->second.at||now-found->second.at>ttl_ms){values_.erase(found);return false;}
        answer=found->second.answer;return true; // A lookup never refreshes TTL.
    }
    void remember(Vec3 from,Vec3 to,collision::Revision revision,unsigned mask,u64 now,Answer answer){
        if(!answer.known)return;Key key;if(!make_key(from,to,revision,mask,key))return;
        if(values_.size()>=capacity){prune(now);if(values_.size()>=capacity)values_.clear();}
        values_[key]={answer,now};
    }
};
inline bool same(collision::Revision a,collision::Revision b){return a.scene==b.scene&&a.obstacles==b.obstacles;}
}

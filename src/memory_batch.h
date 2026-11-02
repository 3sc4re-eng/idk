#pragma once
#include "utils.h"
#include <functional>
#include <map>

struct DmaRead {
    u64 address=0;void* data=nullptr;u32 size=0,transferred=0;bool valid=false;
};
using BatchRead=std::function<bool(std::vector<DmaRead>&)>;
namespace memory_batch {
struct Counters {u64 batches=0,requests=0,bytes=0,failed=0,hits=0,misses=0;};
// Fresh per-frame bytes, indexed by virtual page. A small field lookup should
// examine its own page's ranges, rather than every player's prefetched ranges.
// Each span still has independent complete-transfer validity; no cache survives
// clear(), no hole is filled, and a covered failed range is never silently retried.
class FrameCache {
    struct Span {u64 address;std::vector<unsigned char> bytes;bool executed=false,valid=false;};
    std::map<u64,std::vector<Span>> pages_;
    std::size_t span_count_=0;
    u64 advance(u64 at,u64 end,bool valid_only)const {
        const auto page=pages_.find(at>>12);if(page==pages_.end())return at;
        u64 next=at;
        for(const auto& span:page->second)
            if((!valid_only||(span.executed&&span.valid))&&at>=span.address&&at<span.address+span.bytes.size())
                next=std::max(next,std::min<u64>(end,span.address+span.bytes.size()));
        return next;
    }
    bool covered(u64 address,u32 size,bool valid_only)const {
        if(!size||address>UINT64_MAX-size)return false;
        const u64 end=address+size;
        for(u64 at=address;at<end;){const auto next=advance(at,end,valid_only);if(next==at)return false;at=next;}
        return true;
    }
public:
    Counters counters;
    void clear(){pages_.clear();span_count_=0;counters={};}
    bool queue(u64 address,u32 size){
        if(address<0x10000||!size||size>8u*1024*1024||address>0x00007fffffffffffULL-size)return false;
        while(size){
            const u32 part=std::min<u32>(size,4096-u32(address&4095));const u64 key=address>>12;
            auto found=pages_.find(key);
            if(found==pages_.end()){
                if(span_count_>=8192)return false;
                found=pages_.try_emplace(key).first;
            }
            auto& ranges=found->second;bool exists=false;
            for(const auto& span:ranges)if(address>=span.address&&address+part<=span.address+span.bytes.size()){exists=true;break;}
            if(!exists){
                u64 low=address,high=address+part;
                // Merge touching queued ranges only. No gaps or completed
                // samples are absorbed into a new request.
                for(std::size_t i=0;i<ranges.size();){const auto& span=ranges[i];
                    if(!span.executed&&span.address<=high&&low<=span.address+span.bytes.size()){
                        low=std::min(low,span.address);high=std::max(high,span.address+span.bytes.size());ranges.erase(ranges.begin()+i);--span_count_;i=0;
                    }else ++i;
                }
                if(span_count_>=8192)return false;
                ranges.push_back({low,std::vector<unsigned char>(std::size_t(high-low)),false,false});++span_count_;
            }
            address+=part;size-=part;
        }
        return true;
    }
    bool flush(const BatchRead& batch,const std::function<bool(u64,void*,u32)>& fallback){
        std::vector<DmaRead> requests;std::vector<Span*> owners;
        for(auto& page:pages_)for(auto& span:page.second)if(!span.executed){requests.push_back({span.address,span.bytes.data(),u32(span.bytes.size()),0,false});owners.push_back(&span);}
        bool all=true;
        // Keep the existing bounded burst and full-transfer requirements.
        for(std::size_t offset=0;offset<requests.size();offset+=512){const auto count=std::min<std::size_t>(512,requests.size()-offset);
            std::vector<DmaRead> part(requests.begin()+offset,requests.begin()+offset+count);++counters.batches;counters.requests+=count;
            bool executed=true;if(batch)executed=batch(part);else for(auto& request:part){request.valid=fallback&&fallback(request.address,request.data,request.size);request.transferred=request.valid?request.size:0;}
            for(std::size_t i=0;i<count;++i){auto& span=*owners[offset+i];span.executed=true;span.valid=executed&&part[i].valid&&part[i].transferred==part[i].size;
                if(span.valid)counters.bytes+=part[i].size;else{++counters.failed;all=false;}
            }
        }
        return all;
    }
    bool contains(u64 address,u32 size)const{return covered(address,size,true);}
    bool planned(u64 address,u32 size)const{return covered(address,size,false);}
    bool copy(u64 address,void* destination,u32 size){
        // Validate the complete range before touching the caller's buffer.
        if(!destination||!contains(address,size)){++counters.misses;return false;}
        auto* out=static_cast<unsigned char*>(destination);u64 at=address,end=address+size;
        while(at<end){const auto& ranges=pages_.find(at>>12)->second;
            for(const auto& span:ranges)if(span.executed&&span.valid&&at>=span.address&&at<span.address+span.bytes.size()){
                const auto count=std::min<u64>(end-at,span.address+span.bytes.size()-at);std::memcpy(out+(at-address),span.bytes.data()+(at-span.address),std::size_t(count));at+=count;break;
            }
        }
        ++counters.hits;return true;
    }
};
}

#include "scan_incremental.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace incremental_discovery {
namespace {
constexpr Address maximum=0x00007fffffffffffULL;
bool span(Address a,std::uint64_t n){return a>=0x10000&&n&&a<=maximum&&n-1<=maximum-a;}
std::uint32_t word(const std::uint8_t* p){std::uint32_t v=0;std::memcpy(&v,p,sizeof(v));return v;}
Address pointer(const std::uint8_t* p){Address v=0;std::memcpy(&v,p,sizeof(v));return v;}
}
Engine::Engine(Read read,Scatter scatter,Classify classify):read_(std::move(read)),scatter_(std::move(scatter)),classify_(std::move(classify)){}
void Engine::erase_identity(std::uint32_t index){if(identities_.erase(index))identity_revision_=next_identity_revision(identity_revision_);}
void Engine::put_identity(std::uint32_t index,Identity value){
    const auto found=identities_.find(index);if(found!=identities_.end()&&found->second==value)return;
    identities_.insert_or_assign(index,std::move(value));identity_revision_=next_identity_revision(identity_revision_);
}
void Engine::reset(const Config& config){
    config_=config;identities_.clear();identity_revision_=next_identity_revision(identity_revision_);known_class_cache_.clear();unknown_class_cache_.clear();chunks_.clear();pending_.clear();pending_position_=0;chunk_position_=0;diagnostics_={};
}
bool Engine::valid_config()const{
    if(!read_||!scatter_||!classify_||!config_.session_token||!span(config_.entities_global,8)||!span(config_.client_base,config_.client_size))return false;
    if(config_.stride<0x34||config_.stride>1024||!config_.entries_per_chunk||config_.entries_per_chunk>512||!config_.chunk_count||config_.chunk_count>64||!config_.max_headers_per_step||config_.max_headers_per_step>128)return false;
    if(config_.handle_offset>config_.stride-4||config_.flags_offset>config_.stride-4||config_.object_identity_offset<8||config_.object_identity_offset>120||config_.chunk_table_offset>4096||config_.cycle_interval_ms>60000)return false;
    return std::uint64_t(config_.entries_per_chunk)*config_.chunk_count<=std::uint64_t(config_.index_mask)+1;
}
bool Engine::begin_cycle(std::uint64_t now){
    const auto previous_attempt=diagnostics_.attempted_ms;const auto cycle=diagnostics_.cycle+1;diagnostics_={};diagnostics_.cycle=cycle;diagnostics_.attempted_ms=now;diagnostics_.interval_ms=previous_attempt?now-previous_attempt:0;diagnostics_.active=true;diagnostics_.status="incremental discovery in progress";
    Address system=0;++diagnostics_.direct_calls;
    if(!read_(config_.entities_global,&system,8)||!span(system,config_.chunk_table_offset+std::uint64_t(config_.chunk_count)*8)){finish_cycle(now,"entity system pointer unavailable");return false;}
    chunks_.assign(config_.chunk_count,0);++diagnostics_.direct_calls;
    if(!read_(system+config_.chunk_table_offset,chunks_.data(),config_.chunk_count*8)){finish_cycle(now,"entity chunk table unavailable");return false;}
    pending_.clear();pending_position_=0;chunk_position_=0;return true;
}
void Engine::summarize(){
    diagnostics_.accepted=static_cast<std::uint32_t>(identities_.size());diagnostics_.pawns=diagnostics_.controllers=0;
    for(const auto& pair:identities_){if(pair.second.kind==1)++diagnostics_.pawns;else if(pair.second.kind==2)++diagnostics_.controllers;}
}
void Engine::finish_cycle(std::uint64_t now,const std::string& status){diagnostics_.active=false;diagnostics_.complete=true;diagnostics_.completed_ms=now;diagnostics_.status=status;summarize();}
bool Engine::load_one_chunk(){
    pending_.clear();pending_position_=0;
    // Empty slots consume no PCIe transactions and do not spend the one-chunk
    // I/O budget. They DO cover their indices: old entries are invalid there.
    while(chunk_position_<chunks_.size()&&!chunks_[chunk_position_]){
        const auto first=chunk_position_*config_.entries_per_chunk;
        for(std::uint32_t i=0;i<config_.entries_per_chunk;++i)erase_identity(first+i);
        ++chunk_position_;
    }
    if(chunk_position_==chunks_.size())return false;
    const auto chunk=chunk_position_++;const auto base=chunks_[chunk];++diagnostics_.chunks;
    const auto bytes=config_.stride*config_.entries_per_chunk;
    if(!span(base,bytes)){++diagnostics_.invalid_chunks;const auto first=chunk*config_.entries_per_chunk;for(std::uint32_t i=0;i<config_.entries_per_chunk;++i)erase_identity(first+i);return true;}
    // A new chunk allocation invalidates old identity-record addresses even if
    // some pages of that NEW chunk cannot currently be acquired.
    for(std::uint32_t i=0;i<config_.entries_per_chunk;++i){auto old=identities_.find(chunk*config_.entries_per_chunk+i);if(old!=identities_.end()&&old->second.record!=base+std::uint64_t(i)*config_.stride)erase_identity(chunk*config_.entries_per_chunk+i);}
    std::vector<std::uint8_t> block(bytes);std::vector<Request> requests;std::uint32_t offset=0;
    // Buffer is allocated once before PrepareEx; it remains stable through the
    // supplied callback, which must itself close the VMM scatter handle first.
    while(offset<bytes){const auto address=base+offset;const auto count=std::min<std::uint32_t>(4096-static_cast<std::uint32_t>(address&4095),bytes-offset);requests.push_back({address,block.data()+offset,count,false});offset+=count;}
    ++diagnostics_.scatter_calls;diagnostics_.step_pages+=static_cast<std::uint32_t>(requests.size());const auto executed=scatter_(requests);
    std::vector<bool> valid(requests.size(),false);
    for(std::size_t i=0;i<requests.size();++i){valid[i]=executed&&requests[i].complete;if(valid[i])++diagnostics_.pages_read;else{++diagnostics_.pages_unavailable;if(!diagnostics_.first_unavailable_page)diagnostics_.first_unavailable_page=requests[i].address;}}
    const auto record_bytes=std::max<std::uint32_t>(8,std::max(config_.handle_offset+4,config_.flags_offset+4));
    const auto first_page=base/4096;
    for(std::uint32_t i=0;i<config_.entries_per_chunk;++i){
        const auto position=i*config_.stride;const auto first=(base+position)/4096-first_page,last=(base+position+record_bytes-1)/4096-first_page;
        bool covered=true;for(auto page=first;page<=last;++page)covered=covered&&valid[static_cast<std::size_t>(page)];
        // Do not zero-fill holes or delete identities from unavailable records.
        // Any retained identity is still revalidated by the fast entity lane.
        if(!covered)continue;
        ++diagnostics_.records_read;const auto* raw=block.data()+position;const auto entity=pointer(raw);const auto handle=word(raw+config_.handle_offset),flags=word(raw+config_.flags_offset);const auto index=chunk*config_.entries_per_chunk+i;
        auto old=identities_.find(index);if(old!=identities_.end()&&(old->second.entity!=entity||old->second.handle!=handle||old->second.record!=base+position))erase_identity(index);
        if(!entity){++diagnostics_.empty_records;erase_identity(index);continue;}
        if(flags&config_.reject_flags){++diagnostics_.flags_rejected;erase_identity(index);continue;}
        if(!span(entity,config_.object_identity_offset+8)){++diagnostics_.invalid_addresses;erase_identity(index);continue;}
        if((handle&config_.index_mask)!=index||handle==UINT32_MAX){++diagnostics_.serial_mismatches;erase_identity(index);continue;}
        ++diagnostics_.candidates;pending_.push_back({entity,base+position,handle,index});
    }
    return true;
}
bool Engine::read_headers(std::uint32_t budget){
    const auto count=std::min<std::size_t>(budget,pending_.size()-pending_position_);if(!count)return false;
    const auto header_size=config_.object_identity_offset+8;
    const auto record_size=std::max<std::uint32_t>(8,std::max(config_.handle_offset+4,config_.flags_offset+4));
    std::vector<std::array<std::uint8_t,128>> bytes(count);std::vector<std::array<std::uint8_t,1024>> records(count);std::vector<Request> requests;std::vector<std::pair<std::size_t,std::size_t>> spans(count);
    auto append=[&](Address base,std::uint8_t* data,std::uint32_t length){std::uint32_t offset=0;while(offset<length){const auto address=base+offset;const auto size=std::min<std::uint32_t>(length-offset,4096-static_cast<std::uint32_t>(address&4095));requests.push_back({address,data+offset,size,false});offset+=size;}};
    for(std::size_t i=0;i<count;++i){spans[i].first=requests.size();const auto& candidate=pending_[pending_position_+i];append(candidate.entity,bytes[i].data(),header_size);append(candidate.record,records[i].data(),record_size);spans[i].second=requests.size();}
    ++diagnostics_.scatter_calls;diagnostics_.step_headers+=static_cast<std::uint32_t>(count);const auto executed=scatter_(requests);
    for(std::size_t i=0;i<count;++i){
        const auto& candidate=pending_[pending_position_+i];bool complete=executed;for(auto r=spans[i].first;r<spans[i].second;++r)complete=complete&&requests[r].complete;
        if(!complete){++diagnostics_.object_unreadable;erase_identity(candidate.index);continue;}
        // Candidates can wait several fast frames before their budgeted header
        // pass. Recheck record pointer/serial/flags in the SAME scatter phase,
        // rather than reaccepting a handle that was recycled while queued.
        if(pointer(records[i].data())!=candidate.entity||word(records[i].data()+config_.handle_offset)!=candidate.handle){++diagnostics_.serial_mismatches;erase_identity(candidate.index);continue;}
        if(word(records[i].data()+config_.flags_offset)&config_.reject_flags){++diagnostics_.flags_rejected;erase_identity(candidate.index);continue;}
        const auto vtable=pointer(bytes[i].data()),identity=pointer(bytes[i].data()+config_.object_identity_offset);
        if(vtable<config_.client_base||vtable-config_.client_base>=config_.client_size){++diagnostics_.vtable_outside_client;erase_identity(candidate.index);continue;}
        if(identity!=candidate.record){++diagnostics_.identity_mismatches;erase_identity(candidate.index);continue;}
        const auto rva=static_cast<std::uint32_t>(vtable-config_.client_base);Class type;auto known=known_class_cache_.find(rva);
        if(known!=known_class_cache_.end())type=known->second;
        else if(unknown_class_cache_.count(rva)||!classify_(rva,type)){if(unknown_class_cache_.size()>=8192)unknown_class_cache_.clear();unknown_class_cache_[rva]=true;++diagnostics_.unknown_classes;if(!diagnostics_.first_unknown_vtable)diagnostics_.first_unknown_vtable=vtable;erase_identity(candidate.index);continue;}
        else {if(known_class_cache_.size()>=8192)known_class_cache_.clear();known_class_cache_.emplace(rva,type);}
        ++diagnostics_.known_classes;put_identity(candidate.index,{candidate.entity,candidate.record,vtable,candidate.handle,type.kind,type.name});
    }
    pending_position_+=count;return true;
}
bool Engine::step(std::uint64_t now){
    diagnostics_.step_headers=0;diagnostics_.step_pages=0;
    if(!valid_config()){diagnostics_.status="invalid incremental discovery configuration";diagnostics_.active=false;return false;}
    if(!diagnostics_.active){if(diagnostics_.complete&&now>=diagnostics_.completed_ms&&now-diagnostics_.completed_ms<config_.cycle_interval_ms)return false;if(!begin_cycle(now))return false;}
    bool work=false;
    if(pending_position_==pending_.size())work=load_one_chunk();
    work=read_headers(config_.max_headers_per_step)||work;summarize();
    if(pending_position_==pending_.size()){
        // Advance through trailing absent chunks without another I/O operation.
        while(chunk_position_<chunks_.size()&&!chunks_[chunk_position_]){const auto first=chunk_position_*config_.entries_per_chunk;for(std::uint32_t i=0;i<config_.entries_per_chunk;++i)erase_identity(first+i);++chunk_position_;}
        if(chunk_position_==chunks_.size())finish_cycle(now,identities_.empty()?"covered readable records; no validated identities":"incremental identity cycle complete");
    }
    return work;
}
} // namespace incremental_discovery

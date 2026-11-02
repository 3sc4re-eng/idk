// Compiler-evaluated checks plus host-byte fixtures. No main; never executed
// as part of this proposal. There are no game, process or device calls here.
#include "engine_map_reader.h"
#include <cassert>
#include <map>
#include <vector>
using namespace engine_map;
static_assert(full(6)&&!full(0)&&!full(2)&&!full(5)&&!full(7));
static_assert(normalize("DL_MIDTOWN").valid&&normalize("DL_MIDTOWN").view()=="dl_midtown");
static_assert(normalize("hero_testing").view()=="hero_testing");
static_assert(!normalize("").valid&&!normalize("maps/dl_midtown").valid);
static_assert(!normalize("../dl_midtown").valid&&!normalize("dl_midtown.vpk").valid);
static_assert(!normalize("map name").valid&&!normalize("C:map").valid);
static_assert(!normalize("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa").valid);
static_assert(normalize("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa").valid);
static_assert(!span(0x1000,4)&&!span(highest_address,2)&&span(highest_address,1));

namespace engine_map_fixture {
struct Memory {
    std::map<u64,unsigned char> data;
    void fill(u64 address,u32 count){for(u32 i=0;i<count;++i)data[address+i]=0;}
    template<class T>void put(u64 address,const T& value){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(std::size_t i=0;i<sizeof(value);++i)data[address+i]=bytes[i];}
    void text(u64 address,std::string_view value,u32 capacity){fill(address,capacity);assert(value.size()<capacity);for(std::size_t i=0;i<value.size();++i)data[address+i]=static_cast<unsigned char>(value[i]);}
    bool read(u64 address,void* destination,u32 count){auto* bytes=static_cast<unsigned char*>(destination);for(u32 i=0;i<count;++i){auto found=data.find(address+i);if(found==data.end())return false;bytes[i]=found->second;}return true;}
};
inline constexpr u64 base=0x180000000ULL,client=0x500000000ULL,full_address=0x600000000ULL,short_address=0x600000200ULL;
void fixture_checks(){
    Memory memory;
    memory.put(base+engine_map_layout::service_instance_rva,base+engine_map_layout::service_expected_vtable_rva);
    memory.put(base+engine_map_layout::active_client_global_rva,client);
    memory.put(client,base+engine_map_layout::client_expected_vtable_rva);
    memory.fill(client+0x210,0x38);memory.put(client+0x210,full_address);memory.put(client+0x218,short_address);
    memory.put(client+0x230,std::int32_t(6));memory.put(client+0x244,u32(0));
    memory.text(full_address,"maps/dl_midtown.vpk",129);memory.text(short_address,"dl_midtown",65);
    unsigned fresh_calls=0;
    auto cached=[&](u64 a,void* b,u32 n){return memory.read(a,b,n);};
    auto fresh=[&](u64 a,void* b,u32 n){++fresh_calls;return memory.read(a,b,n);};
    Monitor monitor;auto result=monitor.update(cached,fresh,base,1000);
    assert(result.ready()&&result.map=="dl_midtown"&&result.header.spawn==0);
    const auto first_calls=fresh_calls;result=monitor.update(cached,fresh,base,1500);
    assert(result.ready()&&result.name_sampled_ms==1000&&fresh_calls==first_calls);
    memory.put(client+0x230,std::int32_t(5));result=monitor.update(cached,fresh,base,1501);
    assert(!result.ready()&&result.map.empty()&&result.stage==Stage::NotFull&&fresh_calls==first_calls);
    memory.put(client+0x230,std::int32_t(6));result=monitor.update(cached,fresh,base,1502);
    assert(result.ready()&&result.name_sampled_ms==1502);
    memory.put(client+0x244,u32(0xffffffff)); // raw counters are not positive-only IDs
    memory.text(full_address,"maps/hero_testing.vpk",129);memory.text(short_address,"hero_testing",65);
    result=monitor.update(cached,fresh,base,1503);assert(result.ready()&&result.map=="hero_testing"&&result.header.spawn==0xffffffff);
    memory.text(full_address,"maps/wrong.vpk",129);result=monitor.update(cached,fresh,base,2504);
    assert(!result.ready()&&result.map.empty()&&result.stage==Stage::NameMismatch);
    const auto failed_calls=fresh_calls;result=monitor.update(cached,fresh,base,2600);
    assert(!result.ready()&&fresh_calls==failed_calls);
    // Header invalidation is immediate even during the name retry backoff.
    memory.put(base+engine_map_layout::active_client_global_rva,u64(0));result=monitor.update(cached,fresh,base,2601);
    assert(!result.ready()&&result.map.empty()&&result.stage==Stage::NoClient);
    memory.put(base+engine_map_layout::active_client_global_rva,client);
    memory.text(full_address,"maps/hero_testing.vpk",129);result=monitor.update(cached,fresh,base,2602);assert(result.ready());
    // Corrupted or uninitialized object types are never chased as a client.
    memory.put(client,u64(0));result=monitor.update(cached,fresh,base,2603);
    assert(!result.ready()&&result.stage==Stage::ClientTypeMismatch);
    memory.put(client,base+engine_map_layout::client_expected_vtable_rva);
    // A name pair changing in place with stable pointers is rejected.
    bool changed=false;
    auto racing=[&](u64 a,void* b,u32 n){const bool ok=memory.read(a,b,n);if(!changed&&a==short_address){changed=true;memory.text(full_address,"maps/dl_midtown.vpk",129);memory.text(short_address,"dl_midtown",65);}return ok;};
    monitor.reset();result=monitor.update(cached,racing,base,3000);assert(!result.ready()&&result.stage==Stage::ChangedDuringRead);
    // Immutable-layout validation is a separate bind-time operation.
    for(const auto& check:engine_map_layout::code_checks)for(std::size_t i=0;i<check.bytes.size();++i)memory.data[base+check.rva+i]=check.bytes[i];
    struct Entry{u32 table,index,function;};
    constexpr Entry entries[]={{0x51fa10,23,0x1d12b0},{0x4e2790,64,0x731c0},{0x4e2790,65,0x73220},{0x4e47c0,109,0x83910},{0x4dd548,109,0x5f160}};
    for(const auto& entry:entries)memory.put(base+entry.table+u64(entry.index)*8,base+entry.function);
    std::string error;assert(verify_layout(cached,base,error));
    memory.data[base+engine_map_layout::code_checks[0].rva]^=1;
    assert(!verify_layout(cached,base,error)&&!error.empty());
}
}

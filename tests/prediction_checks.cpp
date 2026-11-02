#include "prediction.h"
#include "prediction_native.h"
#include <cassert>
#include <map>
#include <cstdio>
struct Memory{
    std::map<u64,u8> bytes;
    void zero(u64 p,size_t n){for(size_t i=0;i<n;i++)bytes[p+i]=0;}
    template<class T>void put(u64 p,T v){auto* b=(u8*)&v;for(size_t i=0;i<sizeof(v);i++)bytes[p+i]=b[i];}
    bool read(u64 p,void* b,u32 n){for(u32 i=0;i<n;i++){auto it=bytes.find(p+i);if(it==bytes.end())return false;((u8*)b)[i]=it->second;}return true;}
};
int main(){
    prediction::Shot shot;shot.valid=true;shot.kind=prediction::Kind::Projectile;shot.speed=1000;shot.lifetime=4;
    auto stationary=prediction::solve(shot,{0,0,0},{},{1000,0,0},{});assert(stationary.valid&&std::fabs(stationary.flight-1)<.0001f);
    auto moving=prediction::solve(shot,{0,0,0},{},{1000,0,0},{0,200,0});assert(moving.valid&&moving.point.y>200&&moving.flight>1);
    assert(prediction::position(moving,moving.flight,shot.gravity).dist(moving.target)<.01f);
    auto slower=moving;shot.speed=500;slower=prediction::solve(shot,{0,0,0},{},{1000,0,0},{0,200,0});assert(slower.valid&&slower.flight>moving.flight&&slower.point.y>moving.point.y);
    shot.speed=1000;shot.gravity=800;auto falling=prediction::solve(shot,{0,0,0},{},{600,0,0},{});assert(falling.valid&&falling.point.z>0&&prediction::position(falling,falling.flight,800).dist(falling.target)<.01f);
    shot.gravity=0;shot.inherit_velocity=1;auto inherit=prediction::solve(shot,{0,0,0},{0,100,0},{1000,0,0},{});assert(inherit.valid&&inherit.point.y<0&&prediction::position(inherit,inherit.flight,0).dist(inherit.target)<.01f);
    shot.inherit_velocity=0;auto delayed=prediction::solve(shot,{0,0,0},{},{1000,0,0},{0,200,0},.05f);assert(delayed.valid&&delayed.point.y>moving.point.y);
    assert(!prediction::solve(shot,{0,0,0},{},{1000,0,0},{1100,0,0}).valid);shot.lifetime=.2f;assert(!prediction::solve(shot,{0,0,0},{},{1000,0,0},{}).valid);
    shot.kind=prediction::Kind::Beam;auto beam=prediction::solve(shot,{0,0,0},{},{1000,0,0},{0,200,0},.05f);assert(beam.valid&&beam.flight==0&&std::fabs(beam.point.y-10)<.001f);
    shot.valid=false;assert(!prediction::solve(shot,{},{},{1000,0,0},{}).valid);
    Memory memory;u64 client=0x10000000,vdata=0x20000000,mod=0x30000000,nodes=0x40000000,gravity=0x50000000;
    memory.put(client+prediction_native::primary_symbol,u64(1234));memory.put(vdata+0x18c,u32(1));memory.put(vdata+0x190,nodes);memory.put(vdata+0x198,int(0));memory.zero(nodes,0x8e8);memory.put(nodes+0x10,u64(1234));u64 info=nodes+0x18;
    memory.put(info+0xd8,30000.f);memory.put(info+0xe0,1.5f);memory.put(info+0xec,4.f);memory.put(info+0x50,100000.f);memory.put(info+0xe4,2.f);
    memory.put(client+prediction_native::gravity_reference+8,gravity);memory.put(gravity+0x58,800.f);memory.zero(mod,0x4000);memory.put(mod+0xa0+177,u8(255));memory.put(mod+0x210,u32(1));
    memory.put(client+prediction_native::cache_enabled,u8(1));memory.put(client+prediction_native::cache_modes+176,u8(3));
    u64 cache=mod+0x410+176*48;memory.put(cache,u32(2));memory.put(cache+4,prediction_native::parameter_hash);memory.put(cache+8,u32(1));memory.put(cache+12,u32(512));memory.put(cache+16,u32(2));memory.put(cache+32,45.f);
    auto reader=[&](u64 p,void* b,u32 n){return memory.read(p,b,n);};auto weapon=prediction_native::read(reader,client,vdata,mod,512,false);assert(weapon.valid&&std::fabs(weapon.speed-43500)<.1f&&weapon.gravity==1200);
    memory.put(cache+32,60.f);weapon=prediction_native::read(reader,client,vdata,mod,512,false);assert(weapon.valid&&std::fabs(weapon.speed-48000)<.1f);
    memory.put(mod+0x210,u32(2));assert(!prediction_native::read(reader,client,vdata,mod,512,false).valid);memory.put(mod+0x210,u32(1));memory.put(cache,u32(3));assert(!prediction_native::read(reader,client,vdata,mod,512,false).valid);memory.put(cache,u32(4));
    memory.put(client+prediction_native::cache_modes+176,u8(4));assert(prediction_native::read(reader,client,vdata,mod,512,false).valid);assert(!prediction_native::read(reader,client,vdata,mod,513,false).valid);
    memory.put(cache+4,u32(0));assert(!prediction_native::read(reader,client,vdata,mod,512,false).valid);
    std::puts("PASS: distinct weapon speeds, moving-target intercept, gravity, inherited velocity, latency, beams, lifetime/unreachable rejection, live item-speed modifiers, and dirty/stale cache rejection");
}

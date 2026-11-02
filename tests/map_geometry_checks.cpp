#include "map_identity.h"
#include "target_geometry.h"
#include <cassert>
#include <limits>

namespace map_geometry_checks {
void page_terminator_boundary(){
    unsigned calls=0;std::string text;
    auto only_one_page=[&](std::uint64_t address,void* dst,std::uint32_t size){
        ++calls;assert(address==0x10ffe&&size==2);auto bytes=static_cast<char*>(dst);bytes[0]='x';bytes[1]=0;return true;
    };
    assert(map_identity::cstring(only_one_page,0x10ffe,text));assert(text=="x"&&calls==1);
    assert(map_identity::from_model("MAPS\\DL_HIDEOUT\\world.vmdl")=="dl_hideout");
    assert(map_identity::from_model("maps/dl_midtown/world.vmdl_c")=="dl_midtown");
    assert(map_identity::from_model("maps/../world.vmdl").empty());
    assert(map_identity::from_model("models/heroes/warden.vmdl").empty());
    assert(!map_identity::cstring([](auto,void*,auto){return false;},0x10000,text)&&text.empty());
    assert(!map_identity::cstring([](auto,void* p,auto n){std::memset(p,'x',n);return true;},0x10000,text,8)&&text.empty());
}
void entity_geometry_provenance(){
    Entity entity;Vec3 point;assert(!target_geometry::region(entity,0,point));
    entity.bounds_valid=true;entity.bounds_min={-10,-20,0};entity.bounds_max={30,20,100};entity.pos={100,200,50};entity.facing={0,90,0};entity.scale=2;
    assert(target_geometry::region(entity,0,point));assert(std::fabs(point.x-100)<.01f&&std::fabs(point.y-220)<.01f&&point.z>230&&point.z<250);
    std::array<Vec3,8> corners;assert(target_geometry::corners(entity,corners));
    float low=1e9f,high=-1e9f;for(auto p:corners){low=std::min(low,p.z);high=std::max(high,p.z);}assert(low==50&&high==250);
    entity.scale=std::numeric_limits<float>::quiet_NaN();assert(!target_geometry::region(entity,0,point));
    entity.scale=1;entity.bounds_max.z=entity.bounds_min.z;assert(!target_geometry::corners(entity,corners));
}
}

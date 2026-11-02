// Source-only regression cases. Compile; do not execute in this environment.
#include "visibility_cache.h"
#include <cassert>
#include <limits>
int main(){
    visibility_cache::Rays cache;visibility_cache::Answer answer;Vec3 from{1,2,3},to{4,5,6};collision::Revision revision{7,11};
    constexpr unsigned mask=collision::Vision|collision::Bullet;
    cache.remember(from,to,revision,mask,100,{true,true});
    assert(cache.find(from,to,revision,mask,125,answer)&&answer.known&&answer.clear);
    assert(cache.find(from,to,revision,mask,150,answer));
    assert(!cache.find(from,to,revision,mask,151,answer));
    cache.remember(from,to,revision,mask,200,{true,false});
    auto camera=from;camera.x=std::nextafter(camera.x,2.f);assert(!cache.find(camera,to,revision,mask,201,answer));
    auto region=to;region.z=std::nextafter(region.z,7.f);assert(!cache.find(from,region,revision,mask,201,answer));
    assert(!cache.find(from,to,{8,11},mask,201,answer));assert(!cache.find(from,to,{7,12},mask,201,answer));
    assert(!cache.find(from,to,revision,collision::Vision,201,answer));
    assert(cache.find(from,to,revision,mask,201,answer)&&answer.known&&!answer.clear);
    cache.clear();cache.remember(from,to,revision,mask,300,{false,false});assert(cache.size()==0);
    camera.x=std::numeric_limits<float>::quiet_NaN();cache.remember(camera,to,revision,mask,300,{true,true});assert(cache.size()==0);
    cache.remember(from,to,revision,mask,400,{true,true});assert(!cache.find(from,to,revision,mask,399,answer));
    for(unsigned i=0;i<2048;i++){auto point=to;point.x+=(float)i;cache.remember(from,point,revision,mask,500,{true,true});assert(cache.size()<=visibility_cache::Rays::capacity);}
}

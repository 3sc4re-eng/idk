#pragma once
#include "prediction.h"
#include <array>
#include <functional>
namespace prediction_native{
using Read=std::function<bool(u64,void*,u32)>;
inline constexpr u32 primary_symbol=0x3641c28,gravity_reference=0x39986e0;
inline constexpr u32 cache_enabled=0x3e1b7af,cache_modes=0x3e1b6b0,cache_default_mode=0x3e1b7b4;
inline constexpr u32 parameter_hash=0x63392de3;
template<class T>inline bool get(const Read& read,u64 a,T& v){return a>0x10000&&a<0x00007fffffffffffULL&&read(a,&v,sizeof(v));}
inline bool multiplier(const Read& read,u64 client,u64 modifiers,int value,int tick,float& result){
    unsigned char enabled=0,mode=0;if(!get(read,client+cache_enabled,enabled)||enabled!=1||!get(read,client+cache_modes+value,mode))return false;
    if(!mode&&!get(read,client+cache_default_mode,mode))return false;
    // These modes explicitly ignore call-specific parameters. Other modes need a different cache key.
    if(mode!=3&&mode!=4)return false;
    u32 epoch=0;uint16_t dirty=0;if(!get(read,modifiers+0x210,epoch)||!epoch||!get(read,modifiers+0x214+value*2,dirty)||dirty)return false;
    std::array<unsigned char,48> entry{};u64 at=modifiers+0x410+value*48;if(!read(at,entry.data(),(u32)entry.size()))return false;
    auto word=[&](int offset){u32 v;std::memcpy(&v,entry.data()+offset,4);return v;};
    if((word(0)&1)||word(4)!=parameter_hash||word(8)!=epoch||(mode==4&&(tick<0||word(12)!=(u32)tick)))return false;
    u32 after=0,last_epoch=0;if(!get(read,at,after)||after!=word(0)||!get(read,modifiers+0x210,last_epoch)||last_epoch!=epoch)return false;
    u32 kind=word(16);float percent=0;std::memcpy(&percent,entry.data()+32,4);
    if(kind==0){result=1;return true;}if((kind!=2&&kind!=5)||!std::isfinite(percent)||percent < -100||percent>10000)return false;
    result=std::max(0.f,(100+percent)*.01f);return true;
}
inline bool no_modifiers(const Read& read,u64 modifiers,int value){
    unsigned char list=0;if(!get(read,modifiers+0xa0+value,list))return false;if(list==255)return true;
    u64 groups=0;int count=-1;return get(read,modifiers+0x90,groups)&&get(read,groups+list*24,count)&&count==0;
}
inline bool weapon_info(const Read& read,u64 client,u64 vdata,u64& result){
    u64 symbol=0,nodes=0;int root=-1;u32 capacity=0;
    if(!get(read,client+primary_symbol,symbol)||!symbol||!get(read,vdata+0x18c,capacity)||!get(read,vdata+0x190,nodes)||!get(read,vdata+0x198,root))return false;
    capacity&=0x7fffffff;if(!capacity||capacity>128)return false;
    for(unsigned tries=0;tries<capacity&&root>=0&&(u32)root<capacity;tries++){
        u64 node=nodes+(u64)root*0x8e8,key=0;int left=-1,right=-1;
        if(!get(read,node+0x10,key))return false;if(key==symbol){result=node+0x18;return true;}
        if(!get(read,node,left)||!get(read,node+4,right))return false;root=symbol<key?left:right;
    }return false;
}
inline prediction::Shot read(const Read& memory,u64 client,u64 vdata,u64 modifiers,int tick,bool ability){
    prediction::Shot shot;u64 info=0;if(!weapon_info(memory,client,vdata,info)){shot.reason="Weapon definition unavailable";return shot;}
    std::array<unsigned char,0x108> raw{};if(!memory(info,raw.data(),(u32)raw.size()))return shot;
    auto scalar=[&](int offset){float v=0;std::memcpy(&v,raw.data()+offset,4);return v;};int type=0;std::memcpy(&type,raw.data()+0xc,4);
    if(type==1){shot.range=scalar(0x50);shot.delay=scalar(0x40);shot.kind=prediction::Kind::Beam;shot.valid=std::isfinite(shot.range+shot.delay)&&shot.range>=0&&shot.delay>=0&&shot.delay<=5;shot.reason="Beam: no projectile flight";return shot;}
    if(type!=0||raw[0xba]||raw[0xf8]){shot.reason="Curved, charged or variable-gravity attack needs a separate verified profile";return shot;}
    // m_mapWeaponInfos uses EBulletSpeed / bonus-bullet-speed stat 176.
    // Entity-based ability projectiles use a different profile and stat 175.
    float factor=0;if(no_modifiers(memory,modifiers,176))factor=1;else if(!multiplier(memory,client,modifiers,176,tick,factor)){shot.reason="Live speed modifier cache unavailable";return shot;}
    if(!ability&&!no_modifiers(memory,modifiers,177)){shot.reason="Base-speed override requires a verified profile";return shot;}
    u64 gravity_data=0;float gravity=0;if(!get(memory,client+gravity_reference+8,gravity_data)||!get(memory,gravity_data+0x58,gravity)||!std::isfinite(gravity)||gravity<0||gravity>4000){shot.reason="Live gravity unavailable";return shot;}
    float speed=scalar(0xd8),scale=scalar(0xe0),inherit=scalar(0xf4),delay=scalar(0x40),life=scalar(0xec),range=scalar(0x50),radius=scalar(0xe4),random=scalar(0xdc);
    if(!std::isfinite(speed+scale+inherit+delay+life+range+radius+random)||speed<=0||speed>1e7f||scale<0||scale>20||inherit<0||inherit>2||delay<0||delay>5||life<0||life>120||range<0||range>1e7f||radius<0||radius>1000||random<0||random>1){shot.reason="Invalid weapon ballistics";return shot;}
    shot.kind=prediction::Kind::Projectile;shot.speed=speed*factor;shot.gravity=gravity*scale;shot.inherit_velocity=inherit;shot.delay=delay;shot.lifetime=life;shot.range=range;shot.radius=radius;shot.variable_speed=random>0;shot.valid=shot.speed>0;shot.reason=shot.valid?"Live weapon definition and modifier cache":"Projectile speed is zero";return shot;
}
}

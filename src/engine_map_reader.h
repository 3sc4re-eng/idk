#pragma once
#include "engine_map_layout.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

// Read-only, build-verified engine map identity. The caller supplies byte reads;
// no process/device APIs, target calls, asset loading or collision queries live
// here. A Ready result identifies a current map, not an available geometry file.
namespace engine_map {
using u64=std::uint64_t;using u32=std::uint32_t;
inline constexpr u64 highest_address=0x00007fffffffffffULL;
inline constexpr u64 retry_ms=250,refresh_ms=1000;
inline constexpr std::int32_t full_state=6;
constexpr bool full(std::int32_t state)noexcept{return state==full_state;}
constexpr bool span(u64 address,u64 bytes)noexcept{return address>=0x10000&&bytes&&address<=highest_address&&bytes-1<=highest_address-address;}
constexpr bool module(u64 base)noexcept{return span(base,engine_map_layout::image_size);}

enum class Stage {
    ServiceReadFailed,ServiceTypeMismatch,ClientPointerReadFailed,NoClient,
    ClientPointerInvalid,ClientTypeReadFailed,ClientTypeMismatch,ClientDataReadFailed,
    NotFull,NamePointerInvalid,HeaderReady,NameReadFailed,NameNotAscii,
    NameUnterminated,NameInvalid,NameMismatch,ChangedDuringRead,Ready
};
inline const char* stage_text(Stage stage)noexcept{
    switch(stage){
    case Stage::ServiceReadFailed:return "engine service unreadable";
    case Stage::ServiceTypeMismatch:return "engine service type mismatch";
    case Stage::ClientPointerReadFailed:return "active client pointer unreadable";
    case Stage::NoClient:return "no active game client";
    case Stage::ClientPointerInvalid:return "active client pointer invalid";
    case Stage::ClientTypeReadFailed:return "active client type unreadable";
    case Stage::ClientTypeMismatch:return "active client type mismatch";
    case Stage::ClientDataReadFailed:return "active client map fields unreadable";
    case Stage::NotFull:return "game client is not in FULL sign-on state";
    case Stage::NamePointerInvalid:return "engine map string pointer unavailable";
    case Stage::HeaderReady:return "typed FULL-state map header acquired";
    case Stage::NameReadFailed:return "engine map string unreadable";
    case Stage::NameNotAscii:return "engine map string contains non-ASCII bytes";
    case Stage::NameUnterminated:return "engine map string exceeds its bounded length";
    case Stage::NameInvalid:return "engine map identifier is unsupported or invalid";
    case Stage::NameMismatch:return "engine full and short map names disagree";
    case Stage::ChangedDuringRead:return "engine map changed during acquisition";
    default:return "engine map identified";
    }
}

struct Header {
    Stage stage=Stage::ServiceReadFailed;
    u64 service_vtable=0,client=0,client_vtable=0,full_pointer=0,short_pointer=0;
    std::int32_t state=0;u32 spawn=0;
    bool ready()const noexcept{return stage==Stage::HeaderReady;}
};
inline bool same(const Header& a,const Header& b)noexcept{
    return a.stage==b.stage&&a.service_vtable==b.service_vtable&&a.client==b.client&&
        a.client_vtable==b.client_vtable&&a.full_pointer==b.full_pointer&&
        a.short_pointer==b.short_pointer&&a.state==b.state&&a.spawn==b.spawn;
}

// Root prefetch contract: service vtable (8), active-client pointer (8),
// client vtable (8), and client+0x210..0x248 (0x38 bytes). Independent validity
// of these groups is preserved by checking every supplied read result.
template<class Read>Header read_header(Read read,u64 base){
    Header header;if(!module(base))return header;
    if(!read(base+engine_map_layout::service_instance_rva,&header.service_vtable,8))return header;
    if(header.service_vtable!=base+engine_map_layout::service_expected_vtable_rva){header.stage=Stage::ServiceTypeMismatch;return header;}
    header.stage=Stage::ClientPointerReadFailed;
    if(!read(base+engine_map_layout::active_client_global_rva,&header.client,8))return header;
    if(!header.client){header.stage=Stage::NoClient;return header;}
    if(!span(header.client,0x248)){header.stage=Stage::ClientPointerInvalid;return header;}
    header.stage=Stage::ClientTypeReadFailed;
    if(!read(header.client,&header.client_vtable,8))return header;
    if(header.client_vtable!=base+engine_map_layout::client_expected_vtable_rva){header.stage=Stage::ClientTypeMismatch;return header;}
    std::array<unsigned char,0x38> bytes{};header.stage=Stage::ClientDataReadFailed;
    if(!read(header.client+0x210,bytes.data(),static_cast<u32>(bytes.size())))return header;
    std::memcpy(&header.full_pointer,bytes.data(),8);std::memcpy(&header.short_pointer,bytes.data()+8,8);
    std::memcpy(&header.state,bytes.data()+0x20,4);std::memcpy(&header.spawn,bytes.data()+0x34,4);
    if(!full(header.state)){header.stage=Stage::NotFull;return header;}
    if(!span(header.full_pointer,1)||!span(header.short_pointer,1)){header.stage=Stage::NamePointerInvalid;return header;}
    header.stage=Stage::HeaderReady;return header;
}

struct NormalizedName {
    std::array<char,65> bytes{};std::size_t length=0;bool valid=false;
    constexpr std::string_view view()const noexcept{return {bytes.data(),length};}
};
constexpr NormalizedName normalize(std::string_view value)noexcept{
    NormalizedName result;if(value.empty()||value.size()>64)return result;
    for(std::size_t i=0;i<value.size();++i){char c=value[i];if(c>='A'&&c<='Z')c=char(c-'A'+'a');
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return {};
        result.bytes[i]=c;
    }
    result.length=value.size();result.valid=true;return result;
}

template<class Read>Stage read_name(Read read,u64 address,std::string& out,std::size_t maximum){
    out.clear();if(!span(address,1))return Stage::NamePointerInvalid;
    while(out.size()<=maximum){
        std::array<char,64> bytes{};
        const auto count=std::min<std::size_t>({bytes.size(),maximum+1-out.size(),4096-std::size_t(address&4095),std::size_t(highest_address-address+1)});
        if(!count||!read(address,bytes.data(),static_cast<u32>(count)))return Stage::NameReadFailed;
        const auto* end=static_cast<const char*>(std::memchr(bytes.data(),0,count));const auto length=end?std::size_t(end-bytes.data()):count;
        for(std::size_t i=0;i<length;++i)if(static_cast<unsigned char>(bytes[i])<32||static_cast<unsigned char>(bytes[i])>126)return Stage::NameNotAscii;
        out.append(bytes.data(),length);if(end)return out.empty()?Stage::NameInvalid:Stage::Ready;
        address+=count;if(address>highest_address)return Stage::NameReadFailed;
    }
    return Stage::NameUnterminated;
}

struct Result {
    Stage stage=Stage::ServiceReadFailed;Header header;
    std::string map,raw_full,raw_short,detail;
    u64 name_sampled_ms=0;
    bool ready()const noexcept{return stage==Stage::Ready&&!map.empty();}
};

class Monitor {
    Header observed_{};bool have_observation_=false,have_clock_=false;
    u64 retry_at_=0,last_now_=0;
    Result result_;
    static u64 later(u64 now,u64 amount)noexcept{return amount>std::numeric_limits<u64>::max()-now?std::numeric_limits<u64>::max():now+amount;}
    Result fail(const Header& header,Stage stage,u64 now,std::string raw_full={},std::string raw_short={}){
        result_={stage,header,{},std::move(raw_full),std::move(raw_short),stage_text(stage),0};retry_at_=later(now,retry_ms);return result_;
    }
public:
    void reset(){observed_={};have_observation_=have_clock_=false;retry_at_=last_now_=0;result_={};}
    template<class CachedRead,class FreshRead>Result update(CachedRead cached,FreshRead fresh,u64 base,u64 now){
        const auto header=read_header(cached,base);
        const bool clock_changed=have_clock_&&now<last_now_;last_now_=now;have_clock_=true;
        if(!header.ready()){
            // The public identity disappears immediately on a bad header or
            // non-FULL state, even while name retry backoff is in progress.
            have_observation_=false;observed_=header;return fail(header,header.stage,now);
        }
        if(clock_changed||!have_observation_||!same(header,observed_)){
            result_={};retry_at_=0;observed_=header;have_observation_=true;
        }
        if(result_.ready()&&now>=result_.name_sampled_ms&&now-result_.name_sampled_ms<refresh_ms){result_.header=header;return result_;}
        if(now<retry_at_){result_.header=header;return result_;}
        const auto before=read_header(fresh,base);
        if(!before.ready()||!same(before,header))return fail(before,Stage::ChangedDuringRead,now);
        std::string first_full,first_short,second_full,second_short;
        auto stage=read_name(fresh,before.full_pointer,first_full,128);
        if(stage!=Stage::Ready)return fail(before,stage,now,std::move(first_full));
        stage=read_name(fresh,before.short_pointer,first_short,64);
        if(stage!=Stage::Ready)return fail(before,stage,now,std::move(first_full),std::move(first_short));
        const auto normalized=normalize(first_short);
        if(!normalized.valid)return fail(before,Stage::NameInvalid,now,std::move(first_full),std::move(first_short));
        if(first_full!="maps/"+first_short+".vpk")return fail(before,Stage::NameMismatch,now,std::move(first_full),std::move(first_short));
        stage=read_name(fresh,before.full_pointer,second_full,128);
        if(stage!=Stage::Ready)return fail(before,stage,now,std::move(first_full),std::move(first_short));
        stage=read_name(fresh,before.short_pointer,second_short,64);
        if(stage!=Stage::Ready)return fail(before,stage,now,std::move(first_full),std::move(first_short));
        const auto after=read_header(fresh,base);
        if(!after.ready()||!same(before,after)||first_full!=second_full||first_short!=second_short)return fail(after,Stage::ChangedDuringRead,now,std::move(first_full),std::move(first_short));
        result_={Stage::Ready,after,std::string(normalized.view()),std::move(first_full),std::move(first_short),stage_text(Stage::Ready),now};retry_at_=0;return result_;
    }
};

// Caller already validates the pinned module PE identity during attachment.
// These immutable instructions and static vtable entries are checked once per
// attachment, never as part of each ordinary update.
template<class Read>bool verify_layout(Read read,u64 base,std::string& error){
    if(!module(base)){error="Engine map module range invalid";return false;}
    for(const auto& check:engine_map_layout::code_checks){
        std::array<unsigned char,128> bytes{};
        if(check.bytes.empty()||check.bytes.size()>bytes.size()||check.rva>engine_map_layout::image_size-check.bytes.size()){
            error="Invalid engine map code guard";return false;
        }
        if(!read(base+check.rva,bytes.data(),static_cast<u32>(check.bytes.size()))){error="Engine map code unreadable: "+std::string(check.name);return false;}
        if(!std::equal(check.bytes.begin(),check.bytes.end(),bytes.begin())){error="Engine map code mismatch: "+std::string(check.name);return false;}
    }
    struct Entry{u32 table,index,function;};
    constexpr Entry entries[]={{0x51fa10,23,0x1d12b0},{0x4e2790,64,0x731c0},{0x4e2790,65,0x73220},{0x4e47c0,109,0x83910},{0x4dd548,109,0x5f160}};
    for(const auto& entry:entries){u64 pointer=0;if(!read(base+entry.table+u64(entry.index)*8,&pointer,8)||pointer!=base+entry.function){error="Engine map static vtable mismatch";return false;}}
    error.clear();return true;
}
}

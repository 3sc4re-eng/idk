#pragma once
#include "map_identity.h"
#include <cstdint>
#include <cstring>
#include <algorithm>

// Diagnostics for the EXISTING C_World model candidate. This is not a newly
// verified engine2 loaded-map provider and does not resolve arbitrary symbols.
namespace map_probe {
enum class Stage {IdentityRead,IdentityMismatch,HandleRead,HandleMismatch,FlagsRead,FlagsRejected,SceneRead,SceneInvalid,SceneHeaderRead,SymbolRead,SymbolNull,SymbolNotPointer,StringRead,StringNonAscii,StringUnterminated,StringEmpty,NotMapPath,LegacyCandidate};
inline const char* name(Stage s){switch(s){case Stage::IdentityRead:return "identity_read_failed";case Stage::IdentityMismatch:return "identity_pointer_changed";case Stage::HandleRead:return "handle_read_failed";case Stage::HandleMismatch:return "serial_handle_changed";case Stage::FlagsRead:return "flags_read_failed";case Stage::FlagsRejected:return "identity_flags_rejected";case Stage::SceneRead:return "scene_pointer_read_failed";case Stage::SceneInvalid:return "scene_pointer_invalid";case Stage::SceneHeaderRead:return "scene_header_read_failed";case Stage::SymbolRead:return "model_symbol_read_failed";case Stage::SymbolNull:return "model_symbol_null";case Stage::SymbolNotPointer:return "model_symbol_not_pointer";case Stage::StringRead:return "resource_string_read_failed";case Stage::StringNonAscii:return "resource_string_non_ascii";case Stage::StringUnterminated:return "resource_string_unterminated";case Stage::StringEmpty:return "resource_string_empty";case Stage::NotMapPath:return "resource_not_map_path";default:return "legacy_model_candidate";}}
struct Layout {std::uint32_t entity_identity=0,entity_scene=0,model_state=0,model_name=0;std::uint64_t client_base=0;std::uint32_t client_size=0;};
struct Probe {Stage stage=Stage::IdentityRead;std::uint64_t record=0,scene=0,scene_vtable=0,symbol=0,failed_address=0;std::uint32_t handle=0,flags=0,failed_bytes=0,read_calls=0,requested_bytes=0;std::string resource,map;};
inline bool span(std::uint64_t a,std::uint32_t n){return a>=0x10000&&n&&a<=0x00007fffffffffffULL&&n-1<=0x00007fffffffffffULL-a;}
template<class Read>Probe inspect(Read read,const Layout& layout,std::uint64_t entity,std::uint64_t expected_record,std::uint32_t expected_handle){
    Probe p;auto get=[&](std::uint64_t a,void* destination,std::uint32_t n){++p.read_calls;p.requested_bytes+=n;if(!span(a,n)||!read(a,destination,n)){p.failed_address=a;p.failed_bytes=n;return false;}return true;};
    if(!span(entity,1)||layout.entity_identity>65536||layout.entity_scene>65536||layout.model_state>65536||layout.model_name>65536){p.failed_address=entity;return p;}
    if(!get(entity+layout.entity_identity,&p.record,8))return p;
    if(p.record!=expected_record){p.stage=Stage::IdentityMismatch;return p;}
    p.stage=Stage::HandleRead;if(!get(p.record+0x10,&p.handle,4))return p;
    if(p.handle!=expected_handle){p.stage=Stage::HandleMismatch;return p;}
    p.stage=Stage::FlagsRead;if(!get(p.record+0x30,&p.flags,4))return p;
    if(p.flags&0x717){p.stage=Stage::FlagsRejected;return p;}
    p.stage=Stage::SceneRead;if(!get(entity+layout.entity_scene,&p.scene,8))return p;
    if(!span(p.scene,8)){p.stage=Stage::SceneInvalid;return p;}
    p.stage=Stage::SceneHeaderRead;if(!get(p.scene,&p.scene_vtable,8))return p;
    // The vtable is diagnostic evidence, not a guessed cast validation rule.
    // CUtlSymbolLarge is logged as an opaque value before the legacy pointer
    // interpretation. A null/nonpointer value is never chased or synthesized.
    p.stage=Stage::SymbolRead;if(!get(p.scene+layout.model_state+layout.model_name,&p.symbol,8))return p;
    if(!p.symbol){p.stage=Stage::SymbolNull;return p;}
    if(!span(p.symbol,1)){p.stage=Stage::SymbolNotPointer;return p;}
    auto address=p.symbol;
    while(p.resource.size()<384){char bytes[64]{};const auto count=static_cast<std::uint32_t>(std::min<std::size_t>({sizeof(bytes),384-p.resource.size(),4096-(address&4095)}));
        p.stage=Stage::StringRead;if(!get(address,bytes,count))return p;
        auto end=static_cast<const char*>(std::memchr(bytes,0,count));const auto length=end?std::size_t(end-bytes):std::size_t(count);
        for(std::size_t i=0;i<length;++i)if(static_cast<unsigned char>(bytes[i])<32||static_cast<unsigned char>(bytes[i])>126){p.stage=Stage::StringNonAscii;return p;}
        p.resource.append(bytes,length);if(end){if(p.resource.empty()){p.stage=Stage::StringEmpty;return p;}p.map=map_identity::from_model(p.resource);p.stage=p.map.empty()?Stage::NotMapPath:Stage::LegacyCandidate;return p;}address+=count;
    }p.stage=Stage::StringUnterminated;return p;
}
} // namespace map_probe

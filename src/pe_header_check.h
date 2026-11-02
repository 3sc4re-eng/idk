#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>

// Exact-completion read callbacks only. No zero-fill, cached-header fallback or
// target execution. Byte layout is the documented PE32+ header layout.
namespace pe_header_check {
enum class Stage {Ready,BaseInvalid,DosRead,DosMagic,Lfanew,NtAddress,NtRead,NtSignature,Machine,OptionalMagic,BuildMismatch};
struct Expected {std::uint32_t timestamp=0,image_size=0;};
struct Result {
    Stage stage=Stage::BaseInvalid;std::uint64_t base=0,address=0;
    std::uint32_t requested=0,read_calls=0,requested_bytes=0,timestamp=0,image_size=0,signature=0;
    std::uint16_t mz=0,machine=0,optional_magic=0;std::int32_t lfanew=0;
    bool dos_complete=false,nt_complete=false;Expected expected;
    std::array<unsigned char,64> dos{};std::array<unsigned char,264> nt{};
    bool ready()const{return stage==Stage::Ready;}
    bool metadata_retry_allowed()const{return !ready()&&stage!=Stage::BuildMismatch;}
};
template<class T> T field(const unsigned char* bytes,std::size_t offset){T value{};std::memcpy(&value,bytes+offset,sizeof(value));return value;}
inline const char* name(Stage s){switch(s){case Stage::Ready:return "matched";case Stage::BaseInvalid:return "module base invalid";case Stage::DosRead:return "DOS read unavailable/incomplete";case Stage::DosMagic:return "DOS MZ mismatch";case Stage::Lfanew:return "DOS e_lfanew invalid";case Stage::NtAddress:return "NT address invalid";case Stage::NtRead:return "NT read unavailable/incomplete";case Stage::NtSignature:return "NT signature mismatch";case Stage::Machine:return "PE machine mismatch";case Stage::OptionalMagic:return "PE optional magic mismatch";default:return "build mismatch";}}
template<class Read> Result inspect(std::uint64_t base,Expected expected,Read&& read){
    Result r;r.base=base;r.expected=expected;
    constexpr std::uint64_t limit=0x00007fffffffffffULL;
    if(base<0x10000||base>=limit||base>limit-r.dos.size())return r;
    auto fetch=[&](std::uint64_t address,void* data,std::uint32_t size){r.address=address;r.requested=size;++r.read_calls;r.requested_bytes+=size;return read(address,data,size);};
    r.stage=Stage::DosRead;if(!fetch(base,r.dos.data(),(std::uint32_t)r.dos.size()))return r;
    r.dos_complete=true;r.mz=field<std::uint16_t>(r.dos.data(),0);r.lfanew=field<std::int32_t>(r.dos.data(),0x3c);
    r.stage=Stage::DosMagic;if(r.mz!=0x5a4d)return r;
    r.stage=Stage::Lfanew;if(r.lfanew<64||r.lfanew>0x100000)return r;
    r.stage=Stage::NtAddress;if(base>limit-std::uint32_t(r.lfanew)||base+std::uint32_t(r.lfanew)>limit-r.nt.size())return r;
    r.stage=Stage::NtRead;if(!fetch(base+std::uint32_t(r.lfanew),r.nt.data(),(std::uint32_t)r.nt.size()))return r;
    r.nt_complete=true;r.signature=field<std::uint32_t>(r.nt.data(),0);r.machine=field<std::uint16_t>(r.nt.data(),4);r.timestamp=field<std::uint32_t>(r.nt.data(),8);r.optional_magic=field<std::uint16_t>(r.nt.data(),24);r.image_size=field<std::uint32_t>(r.nt.data(),24+56);
    r.stage=Stage::NtSignature;if(r.signature!=0x00004550)return r;
    r.stage=Stage::Machine;if(r.machine!=0x8664)return r;
    r.stage=Stage::OptionalMagic;if(r.optional_magic!=0x20b)return r;
    r.stage=Stage::BuildMismatch;if(r.timestamp!=expected.timestamp||r.image_size!=expected.image_size)return r;
    r.stage=Stage::Ready;return r;
}
inline std::string summary(const char* module,const Result& r){
    std::string text=std::string(module)+": ";
    switch(r.stage){
    case Stage::DosRead:return text+"cannot read DOS header; restart Deadlock and retry";
    case Stage::NtRead:return text+"cannot read PE header; restart Deadlock and retry";
    case Stage::BuildMismatch:return text+"build mismatch; update required";
    default:return text+name(r.stage);
    }
}
inline std::string hex(const unsigned char* bytes,std::size_t count,bool complete){
    if(!complete)return "<unavailable/incomplete>";char text[33]{};if(count>16)count=16;for(std::size_t i=0;i<count;++i)std::snprintf(text+i*2,3,"%02X",bytes[i]);return text;
}
inline std::string describe(const char* module,const Result& r){
    char detail[768];auto dos=hex(r.dos.data(),16,r.dos_complete),nt=hex(r.nt.data(),16,r.nt_complete);
    std::snprintf(detail,sizeof(detail),"%s: %s | base=0x%llX request=0x%llX/%u calls=%u requested_bytes=%u MZ=%04X e_lfanew=%d NT=%08X machine=%04X magic=%04X timestamp=%08X expected=%08X image=%08X expected=%08X dos16=%s nt16=%s",module,name(r.stage),(unsigned long long)r.base,(unsigned long long)r.address,r.requested,r.read_calls,r.requested_bytes,r.mz,r.lfanew,r.signature,r.machine,r.optional_magic,r.timestamp,r.expected.timestamp,r.image_size,r.expected.image_size,dos.c_str(),nt.c_str());return detail;
}
} // namespace pe_header_check

#include "pe_header_check.h"
#include <cassert>
namespace {
using namespace pe_header_check;
struct Image {
    std::array<unsigned char,1024> bytes{};bool dos_read=true,nt_read=true;unsigned calls=0;
    template<class T> void set(std::size_t offset,T value){std::memcpy(bytes.data()+offset,&value,sizeof(value));}
    Image(){set<std::uint16_t>(0,0x5a4d);set<std::int32_t>(0x3c,128);set<std::uint32_t>(128,0x4550);set<std::uint16_t>(132,0x8664);set<std::uint32_t>(136,123);set<std::uint16_t>(152,0x20b);set<std::uint32_t>(208,0x10000);}
    Result inspect(){return pe_header_check::inspect(0x100000,{123,0x10000},[&](std::uint64_t address,void* data,std::uint32_t size){++calls;auto offset=address-0x100000;if(offset+size>bytes.size())return false;std::memcpy(data,bytes.data()+offset,size);return offset==0?dos_read:nt_read;});}
};
}
void pe_header_boundary_checks(){
    Image good;auto p=good.inspect();assert(p.ready()&&p.read_calls==2&&p.requested_bytes==328&&!p.metadata_retry_allowed());
    Image partial;partial.dos_read=false;p=partial.inspect();assert(p.stage==Stage::DosRead&&!p.dos_complete&&!p.ready()&&partial.calls==1); // Bytes exist but callback denied exact completion.
    Image mz;mz.set<std::uint16_t>(0,0);p=mz.inspect();assert(p.stage==Stage::DosMagic&&p.dos_complete&&!p.nt_complete);
    Image offset;offset.set<std::int32_t>(0x3c,-1);assert(offset.inspect().stage==Stage::Lfanew);
    Image nt;nt.nt_read=false;assert(nt.inspect().stage==Stage::NtRead);
    Image sig;sig.set<std::uint32_t>(128,0);assert(sig.inspect().stage==Stage::NtSignature);
    Image machine;machine.set<std::uint16_t>(132,0x14c);assert(machine.inspect().stage==Stage::Machine);
    Image magic;magic.set<std::uint16_t>(152,0x10b);assert(magic.inspect().stage==Stage::OptionalMagic);
    Image version;version.set<std::uint32_t>(136,124);p=version.inspect();assert(p.stage==Stage::BuildMismatch&&!p.metadata_retry_allowed());
    Image size;size.set<std::uint32_t>(208,0x20000);assert(size.inspect().stage==Stage::BuildMismatch);
    unsigned reads=0;p=pe_header_check::inspect(0xffffffffffffffffULL,{123,0x10000},[&](std::uint64_t,void*,std::uint32_t){++reads;return true;});assert(p.stage==Stage::BaseInvalid&&reads==0);
}

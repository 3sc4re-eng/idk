// Source-only assertions: syntax checked; never built/linked/executed as tests.
#include "scan_incremental.h"
#include <cassert>
#include <cstring>
#include <map>

namespace incremental_discovery::proposal_checks {
namespace {
void write32(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value){std::memcpy(bytes.data()+at,&value,4);}
void write64(std::vector<std::uint8_t>& bytes,std::size_t at,Address value){std::memcpy(bytes.data()+at,&value,8);}
struct Fixture {
    std::map<Address,std::vector<std::uint8_t>> regions;
    Address missing_page=0;
    unsigned classify_calls=0,scatter_calls=0;
    Fixture(){regions[0x100000].resize(8);write64(regions[0x100000],0,0x200000);regions[0x200000].resize(24);write64(regions[0x200000],16,0x300000);regions[0x300000].resize(0x70*512);for(unsigned index:{0u,1u,2u,3u,4u,5u,37u})object(index);}
    void object(unsigned index){const Address base=0x500000+index*0x100;auto& bytes=regions[0x300000];write64(bytes,index*0x70,base);write32(bytes,index*0x70+0x10,index);auto& header=regions[base];header.resize(24);write64(header,0,0x401000);write64(header,16,0x300000+index*0x70);}
    bool read(Address address,void* out,std::uint32_t size){if(missing_page&&address<missing_page+4096&&address+size>missing_page)return false;for(const auto& pair:regions)if(address>=pair.first&&address-pair.first<pair.second.size()&&size<=pair.second.size()-(address-pair.first)){std::memcpy(out,pair.second.data()+(address-pair.first),size);return true;}return false;}
    bool scatter(std::vector<Request>& requests){++scatter_calls;for(auto& request:requests){assert(request.size<=4096-(request.address&4095));request.complete=read(request.address,request.destination,request.size);if(!request.complete)std::memset(request.destination,0,request.size);}return true;}
};
}
void source_only_incremental_checks(){
    Fixture fixture;Engine engine([&](Address a,void* b,std::uint32_t n){return fixture.read(a,b,n);},[&](std::vector<Request>& requests){return fixture.scatter(requests);},[&](std::uint32_t rva,Class& out){++fixture.classify_calls;if(rva!=0x1000)return false;out={1,"FixturePawn"};return true;});
    Config config;config.entities_global=0x100000;config.client_base=0x400000;config.client_size=0x10000;config.session_token=1;config.chunk_count=1;config.max_headers_per_step=2;config.cycle_interval_ms=0;engine.reset(config);
    assert(engine.step(100));assert(engine.identities().size()==2);assert(engine.diagnostics().step_headers==2);assert(engine.diagnostics().step_pages<=15);assert(engine.active());assert(fixture.classify_calls==1); // class cache saves classification, not validation reads
    assert(engine.step(101));assert(engine.identities().size()==4);assert(engine.diagnostics().step_pages==0);assert(engine.diagnostics().step_headers==2);
    write32(fixture.regions[0x300000],4*0x70+0x10,0x8004); // queued handle is recycled before its header phase
    assert(engine.step(102));assert(!engine.identities().count(4));assert(engine.identities().count(5));assert(engine.diagnostics().serial_mismatches==1);
    assert(engine.step(103));assert(!engine.active());assert(engine.identities().count(37));
    fixture.missing_page=0x301000;assert(engine.step(104));assert(engine.identities().count(37));assert(engine.diagnostics().pages_unavailable==1); // unreadable same-chunk record is retained, never reconstructed from zero fill
    while(engine.active())engine.step(105);
    fixture.regions[0x700000].resize(0x70*512);write64(fixture.regions[0x200000],16,0x700000);fixture.missing_page=0x701000;assert(engine.step(106));assert(!engine.identities().count(37)); // new chunk allocation invalidates old records even across holes
    config.session_token=2;engine.reset(config);assert(engine.identities().empty());assert(!engine.active());assert(engine.diagnostics().cycle==0);
}
} // namespace incremental_discovery::proposal_checks

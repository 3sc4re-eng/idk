// Compile-only source checks. No main; never run as a binary in this proposal.
#include "memory_batch.h"
#include <cassert>

namespace reader_performance_checks {
void frame_cache_boundaries(){
    memory_batch::FrameCache cache;unsigned executions=0;bool hole=false;
    BatchRead batch=[&](std::vector<DmaRead>& reads){++executions;for(auto& read:reads){assert((read.address&4095)+read.size<=4096);std::memset(read.data,0x5a,read.size);read.valid=!(hole&&read.address>=0x11000);read.transferred=read.valid?read.size:0;}return true;};
    assert(cache.queue(0x10ffe,6));assert(!cache.contains(0x10ffe,6));assert(cache.flush(batch,{}));assert(executions==1&&cache.counters.requests==2);unsigned char out[6]{};assert(cache.copy(0x10ffe,out,6));for(auto value:out)assert(value==0x5a);
    // A repeated covered range does not issue a second DMA transaction.
    assert(cache.queue(0x10fff,3));assert(cache.flush(batch,{}));assert(executions==1);
    cache.clear();hole=true;assert(cache.queue(0x10ffe,6));assert(!cache.flush(batch,{}));assert(cache.contains(0x10ffe,2));assert(!cache.contains(0x10ffe,3));std::memset(out,0xab,sizeof(out));assert(!cache.copy(0x10ffe,out,6));for(auto value:out)assert(value==0xab); // failed copy does not fill a plausible synthetic object
    assert(cache.planned(0x10ffe,6));assert(cache.counters.failed==1);cache.clear();assert(!cache.planned(0x10ffe,6));assert(!cache.contains(0x10ffe,6)); // no bytes survive into another frame
    assert(!cache.queue(UINT64_MAX-1,16));assert(!cache.queue(0x10000,0));assert(!cache.queue(0x10,8));
    cache.clear();unsigned joined=0;assert(cache.queue(0x14000,8));assert(cache.queue(0x14008,4));assert(cache.queue(0x14004,8));
    assert(cache.flush([&](std::vector<DmaRead>& reads){joined=(unsigned)reads.size();assert(joined==1&&reads[0].address==0x14000&&reads[0].size==12);for(auto& r:reads){std::memset(r.data,1,r.size);r.valid=true;r.transferred=r.size;}return true;},{}));assert(joined==1);
    cache.clear();
    // A transport claiming success without a complete transfer remains invalid.
    assert(cache.queue(0x12000,4));assert(!cache.flush([](std::vector<DmaRead>& reads){reads[0].valid=true;reads[0].transferred=3;return true;},{}));assert(!cache.contains(0x12000,4));
}
void page_index_and_completed_samples(){
    memory_batch::FrameCache cache;unsigned calls=0;
    BatchRead batch=[&](std::vector<DmaRead>& reads){++calls;assert(reads.size()<=512);for(auto& read:reads){assert((read.address&4095)+read.size<=4096);std::memset(read.data,(int)calls,read.size);read.valid=true;read.transferred=read.size;}return true;};
    assert(cache.queue(0x18004,4)&&cache.flush(batch,{}));
    assert(cache.queue(0x18000,4)&&cache.flush(batch,{}));
    unsigned char out[8]{};assert(cache.copy(0x18000,out,8));
    for(unsigned i=0;i<8;++i)assert(out[i]==(i<4?2:1)); // extending a page cannot freshen completed samples
    cache.clear();calls=0;assert(cache.queue(0x18000,4)&&cache.queue(0x18020,4));assert(cache.flush(batch,{}));
    assert(!cache.planned(0x18000,0x24));std::memset(out,0x7a,sizeof(out));assert(!cache.copy(0x18000,out,sizeof(out)));for(auto value:out)assert(value==0x7a);
    cache.clear();calls=0;for(unsigned i=0;i<600;++i)assert(cache.queue(0x10000+u64(i)*4096,4));assert(cache.flush(batch,{}));assert(calls==2&&cache.counters.requests==600);
    for(unsigned i=0;i<600;++i){unsigned char bytes[4]{};assert(cache.copy(0x10000+u64(i)*4096,bytes,4));for(auto value:bytes)assert(value==(i<512?1:2));}
    assert(!cache.contains(0x10004,1));cache.clear();assert(!cache.contains(0x10000,4));
}
} // namespace reader_performance_checks

// Compile-time only: no executable, DLL load, target handle or DMA access.
#include "dma_scatter_session.h"
#ifndef _Check_return_opt_
#define _Check_return_opt_
#endif
#ifndef _Post_ptr_invalid_
#define _Post_ptr_invalid_
#endif
#ifndef _Frees_ptr_opt_
#define _Frees_ptr_opt_
#endif
#include "vmmdll.h"
#include <array>
#include <type_traits>
using namespace dma_scatter;
static_assert(std::is_same_v<decltype(&VMMDLL_Scatter_Initialize),Start>);
static_assert(std::is_same_v<decltype(&VMMDLL_Scatter_PrepareEx),Prepare>);
static_assert(std::is_same_v<decltype(&VMMDLL_Scatter_ExecuteRead),Execute>);
static_assert(std::is_same_v<decltype(&VMMDLL_Scatter_Clear),Clear>);
static_assert(std::is_same_v<decltype(&VMMDLL_Scatter_CloseHandle),Close>);
static_assert(flags==(VMMDLL_FLAG_NOCACHE|VMMDLL_FLAG_SCATTER_PREPAREEX_NOMEMZERO));
static_assert(page_part(0x10fff,32)==1&&page_part(0x11000,31)==31);
static_assert(page_pieces(0x10000,4096)==1&&page_pieces(0x10fff,4096)==2);
static_assert(page_pieces(0x10001,8u*1024*1024)==2049);
static_assert(page_pieces(0x10fff,4096)*max_requests==2*max_requests);
static_assert(max_pieces==24576);
static_assert(Session::max_piece_storage_bytes==960*1024);
// Legal baseline boundary: 8,192 requests, <=8MiB each, exactly32MiB total.
// Both ends of an unaligned request can require separate partial pages.
static_assert(8188+4==max_requests);
static_assert(8384514<=8u*1024*1024);
static_assert(std::size_t(8188)*2+std::size_t(4)*8384514==max_bytes);
static_assert(page_pieces(0x10fff,2)*8188+page_pieces(0x10fff,8384514)*4==24572);
static_assert(page_pieces(0x10fff,2)*8188+page_pieces(0x10fff,8384514)*4<=max_pieces);
static_assert(complete(true,true,32,32));
static_assert(!complete(true,true,31,32)&&!complete(true,false,32,32)&&!complete(false,true,32,32));
static_assert(same_context(true,10,10,4,4));
static_assert(!same_context(false,10,10,4,4)&&!same_context(true,10,11,4,4)&&!same_context(true,10,10,4,0));
static_assert(!close_before_prepare(false,false,false,false,32,64,3,5));
static_assert(close_before_prepare(false,false,false,false,65,64,3,5)); // bytes must grow
static_assert(close_before_prepare(false,false,false,false,32,64,6,5)); // read-count storage must grow
static_assert(close_before_prepare(true,false,false,false,32,64,3,5)); // runtime exports changed
static_assert(close_before_prepare(false,true,false,false,32,64,3,5)); // VMM changed
static_assert(close_before_prepare(false,false,true,false,32,64,3,5)); // PID changed
static_assert(close_before_prepare(false,false,false,true,32,64,3,5)); // binding changed
static_assert(!reopen_after_clear(true,true));
static_assert(reopen_after_clear(true,false)); // Clear failed: close then reopen
static_assert(reopen_after_clear(false,false)); // Clear export missing: close-per-call fallback

// Page partitioning preserves addresses and exact bytes, including adjacent
// duplicate/overlapping logical requests. It never merges provenance owners.
struct Request{u64 address;u32 size;};
constexpr bool partition_checks(){
    constexpr std::array<Request,7> q={{{0x10fff,32},{0x11000,31},{0x11000,31},{0x12001,4096},{0x13000,0x7e0},{0x14000,4},{0x14003,8}}};
    std::size_t bytes=0,pieces=0;
    for(const auto& r:q){u32 done=0;std::size_t count=0;while(done<r.size){const auto n=page_part(r.address+done,r.size-done);if(!n||((r.address+done)&4095)+n>4096)return false;done+=n;++count;}if(count!=page_pieces(r.address,r.size)||done!=r.size)return false;bytes+=done;pieces+=count;}
    return bytes==6218&&pieces==9;
}
static_assert(partition_checks());

// The same production completion predicate prevents one failed/partial piece
// from accepting a cross-page parent even when all its other pieces succeeded.
constexpr bool parent_checks(){
    bool valid=true;u32 transferred=0;
    constexpr std::array<u32,3> sizes={1,4096,7},got={1,4095,7};
    for(std::size_t i=0;i<sizes.size();++i){if(!complete(true,true,got[i],sizes[i]))valid=false;else transferred+=got[i];}
    return !valid&&transferred==8;
}
static_assert(parent_checks());

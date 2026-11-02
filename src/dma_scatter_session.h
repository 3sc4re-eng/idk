#pragma once
#include "memory_batch.h"
#include "vmm_api.h"
#include <windows.h>
#include <mutex>
#include <span>
#include <memory>

// Only the VMM handle and host allocations persist. Every operation still
// performs a new uncached ExecuteRead and validates every page transfer count.
namespace dma_scatter {
inline constexpr DWORD flags=0x0001|0x1000; // NOCACHE | PREPAREEX_NOMEMZERO
inline constexpr std::size_t max_bytes=32u*1024*1024,max_requests=8192;
// A legal request can have a partial first AND last page. For each request,
// pieces <= size/4096 + 2, so this covers every existing accepted workload.
inline constexpr std::size_t max_pieces=max_bytes/4096+2*max_requests;
using Start=void*(*)(vmm_api::Handle,DWORD,DWORD);
using Prepare=BOOL(*)(void*,ULONG64,DWORD,PBYTE,PDWORD);
using Execute=BOOL(*)(void*);
using Clear=BOOL(*)(void*,DWORD,DWORD);
using Close=void(*)(void*);
struct Api {
    Start start=nullptr;Prepare prepare=nullptr;Execute execute=nullptr;
    Clear clear=nullptr;Close close=nullptr;
    bool available()const noexcept{return start&&prepare&&execute&&close;}
    bool operator==(const Api&)const=default;
};
constexpr u32 page_part(u64 address,u32 remaining)noexcept{return std::min<u32>(remaining,4096-u32(address&4095));}
constexpr std::size_t page_pieces(u64 address,u32 size)noexcept{return size?(std::size_t(address&4095)+size+4095)/4096:0;}
constexpr bool complete(bool executed,bool prepared,u32 got,u32 wanted)noexcept{return executed&&prepared&&got==wanted;}
constexpr bool same_context(bool ready,DWORD before_pid,DWORD after_pid,u64 before_epoch,u64 after_epoch)noexcept{return ready&&before_pid==after_pid&&before_epoch==after_epoch;}
constexpr bool close_before_prepare(bool api_changed,bool vmm_changed,bool pid_changed,bool epoch_changed,std::size_t wanted_bytes,std::size_t owned_bytes,std::size_t wanted_pieces,std::size_t owned_pieces)noexcept{return api_changed||vmm_changed||pid_changed||epoch_changed||wanted_bytes>owned_bytes||wanted_pieces>owned_pieces;}
constexpr bool reopen_after_clear(bool clear_available,bool clear_succeeded)noexcept{return !clear_available||!clear_succeeded;}

class Session {
    struct Piece{u64 address=0;std::size_t offset=0,parent=0;DWORD size=0,got=0;bool prepared=false;};
    std::mutex mutex_;
    Api api_{};void* handle_=nullptr;vmm_api::Handle vmm_=nullptr;DWORD pid_=0;u64 epoch_=0;
    // Never replace either array while a handle exists. PrepareEx's pb and
    // pcbRead point exclusively here, never into caller or per-call storage.
    std::unique_ptr<unsigned char[]> bytes_;
    std::unique_ptr<Piece[]> pieces_;
    std::size_t owned_bytes_=0,owned_pieces_=0;
    void close_unlocked()noexcept{if(handle_&&api_.close)api_.close(handle_);handle_=nullptr;vmm_=nullptr;pid_=0;epoch_=0;}
    bool begin(const Api& api,vmm_api::Handle vmm,DWORD pid,u64 epoch,std::size_t bytes,std::size_t pieces){
        if(!api.available()||!vmm||!pid||bytes>max_bytes||pieces>max_pieces)return false;
        // SDK requires PrepareEx buffers to stay allocated until Close, even
        // though Clear resets the request list. Close BEFORE any reallocation.
        if(handle_&&close_before_prepare(api_!=api,vmm_!=vmm,pid_!=pid,epoch_!=epoch,bytes,owned_bytes_,pieces,owned_pieces_))close_unlocked();
        if(bytes>owned_bytes_){bytes_=std::make_unique<unsigned char[]>(bytes);owned_bytes_=bytes;}
        if(pieces>owned_pieces_){pieces_=std::make_unique<Piece[]>(pieces);owned_pieces_=pieces;}
        if(handle_&&reopen_after_clear(api.clear!=nullptr,api.clear&&api.clear(handle_,pid,flags)!=FALSE))close_unlocked();
        api_=api;
        if(!handle_){handle_=api.start(vmm,pid,flags);if(!handle_)return false;vmm_=vmm;pid_=pid;epoch_=epoch;}
        return true;
    }
public:
    static constexpr std::size_t max_piece_storage_bytes=sizeof(Piece)*max_pieces;
    Session()=default;Session(const Session&)=delete;Session& operator=(const Session&)=delete;
    ~Session(){close_unlocked();}
    void close(){std::lock_guard<std::mutex> lock(mutex_);close_unlocked();}
    bool read(const Api& api,vmm_api::Handle vmm,DWORD pid,u64 epoch,std::span<DmaRead> requests){
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t byte_count=0,piece_count=0;
        for(const auto& r:requests){byte_count+=r.size;piece_count+=page_pieces(r.address,r.size);}
        if(!begin(api,vmm,pid,epoch,byte_count,piece_count))return false;
        std::size_t index=0,offset=0;
        for(std::size_t parent=0;parent<requests.size();++parent){const auto& r=requests[parent];u32 done=0;
            while(done<r.size){const auto count=page_part(r.address+done,r.size-done);auto& p=pieces_[index++];
                p={r.address+done,offset,parent,count,0,false};
                p.prepared=api.prepare(handle_,p.address,p.size,bytes_.get()+offset,&p.got)!=FALSE;
                done+=count;offset+=count;
            }
        }
        const bool executed=api.execute(handle_)!=FALSE;
        for(auto& r:requests){r.valid=executed;r.transferred=0;}
        for(std::size_t i=0;i<piece_count;++i){const auto& p=pieces_[i];auto& r=requests[p.parent];
            if(!complete(executed,p.prepared,p.got,p.size))r.valid=false;else r.transferred+=p.got;
        }
        offset=0;for(auto& r:requests){r.valid=r.valid&&r.transferred==r.size;
            if(r.valid)std::memcpy(r.data,bytes_.get()+offset,r.size);else r.transferred=0;
            offset+=r.size;
        }
        // Older runtimes without Clear retain the existing close-per-call path.
        if(!api.clear)close_unlocked();
        return executed;
    }
};
} // namespace dma_scatter

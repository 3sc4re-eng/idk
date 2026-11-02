#pragma once
#include "utils.h"
#include "vmm_api.h"
#include "memory_batch.h"
#include "dma_scatter_session.h"
#include <windows.h>
#include <string>
#include <atomic>
#include <limits>
struct GameModule;
struct MemStatus{ bool ok=false; std::string text="disconnected"; };
class Reader{
public:
    bool init_vmm();
    bool init_local();
    bool ready() const { return ready_.load(); }
    // ACE_NATIVE_JOINT_INTEGRATION_V1: actual route fact; no vendor call.
    bool dma_read_ready() const noexcept {return ready_.load(std::memory_order_acquire)&&via_vmm_.load(std::memory_order_acquire)&&target_pid_.load(std::memory_order_acquire)!=0;}
    u64 attachment_epoch() const noexcept {return attachment_epoch_.load(std::memory_order_acquire);}
    void invalidate_attachment_epoch() noexcept {attachment_epoch_.store(0,std::memory_order_release);}
    // Binding owner only, while the entity reader is stopped and after layout
    // validation. Sequence saturation fails closed instead of reusing an epoch.
    bool publish_attachment_epoch() noexcept {
        invalidate_attachment_epoch();if(!dma_read_ready())return false;
        auto value=attachment_sequence_.load(std::memory_order_relaxed);
        do{if(value==std::numeric_limits<u64>::max())return false;}
        while(!attachment_sequence_.compare_exchange_weak(value,value+1,std::memory_order_acq_rel,std::memory_order_relaxed));
        attachment_epoch_.store(value+1,std::memory_order_release);return true;
    }
    // No vendor calls or mutable status strings are accessed by this UI snapshot.
    std::string diagnostics() const;
    // Caller must stop/join the entity reader and finish other memory calls first.
    void disconnect();
    bool read(u64 addr,void* buf,u32 size);
    bool refresh_caches();
    bool read_batch(u64 addr,void* buf,u32 size);
    // Real VMM scatter: valid/transferred is authoritative per request.
    bool scatter(std::vector<DmaRead>& requests);
    bool write(u64 addr,const void* buf,u32 size);
    bool read_u32(u64 a,u32& o){ return read(a,&o,4); }
    bool read_u64(u64 a,u64& o){ return read(a,&o,8); }
    bool read_f32(u64 a,float& o){ return read(a,&o,4); }
    bool read_hwid(std::string& out);
    bool find_process(const GameModule& game);
    DWORD process_id() const { return target_pid_.load(); }
    bool refresh_process_list();bool refresh_process_modules();
    u64 module_base(const char* name);
    u64 find_pattern(u64 start,u64 size,const char* pattern,const char* mask);
    std::atomic<u64> total_read_bytes{0};std::atomic<u32> read_errors{0};
    std::atomic<u64> scalar_read_calls{0},scatter_calls{0},scatter_requests{0},requested_read_bytes{0};
    MemStatus status;
private:
    static constexpr u64 unavailable=std::numeric_limits<u64>::max();
    std::atomic<bool> ready_{false},via_vmm_{false};
    std::atomic<u64> attachment_epoch_{0},attachment_sequence_{0};
    std::atomic<DWORD> target_pid_{0};
    std::atomic<u64> fpga_device_id_{unavailable},fpga_id_{unavailable},fpga_major_{unavailable},fpga_minor_{unavailable};
    std::atomic<u64> vmm_major_{unavailable},vmm_minor_{unavailable},vmm_revision_{unavailable};
    vmm_api::Handle vmm_=nullptr; HANDLE local_proc_=nullptr;
    HMODULE vmm_dll_=nullptr;
    dma_scatter::Session scatter_session_;
    bool read_vmm(u64 a,void* b,u32 s);
    bool read_local(u64 a,void* b,u32 s);
    void* fn(const char* n){ return vmm_dll_?(void*)GetProcAddress(vmm_dll_,n):nullptr; }
};
extern Reader mem;

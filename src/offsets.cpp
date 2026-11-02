#include "offsets.h"
#include "dma.h"
#include "live_backend.h"
#include "deadlock_layout.h"
#include "pe_header_check.h"
Offsets off;
void Offsets::report_status(AttachPhase phase,const std::string& message){
    {std::lock_guard<std::mutex> lock(mutex);phase_=phase;report=message;}
    plat_stage("attach",message);
}
bool Offsets::validate(){
    // Copy state under the status lock; never hold it across a vendor call.
    u64 base=0;u32 pid=0;{std::lock_guard<std::mutex> lock(mutex);base=game_base;pid=bound_pid;}
    if(!base||pid!=mem.process_id()||(mem.dma_read_ready()&&!mem.attachment_epoch()))return false; // ACE_NATIVE_JOINT_INTEGRATION_V1
    const auto probe=pe_header_check::inspect(base,{dl_layout::client_timestamp,dl_layout::client_image_size},[&](u64 address,void* bytes,u32 size){return mem.read(address,bytes,size);});
    if(!probe.ready())plat_log("PE revalidation: PID=%lu %s",(unsigned long)pid,pe_header_check::describe("client.dll",probe).c_str());
    return probe.ready();
}
void Offsets::invalidate(const std::string& reason){
    mem.invalidate_attachment_epoch();snap.native_binding_epoch=0;snap.frame_valid=false;
    bool changed=false;{std::lock_guard<std::mutex> lock(mutex);changed=resolved.load()||report!=reason;resolved=false;report=reason;phase_=AttachPhase::Error;}
    if(changed){plat_log("Game layout invalidated: %s",reason.c_str());plat_stage("attach",std::string("awaiting retry: ")+reason);}
}
bool Offsets::resolve(GameModule* game){
    std::lock_guard<std::mutex> operation(operation_mutex_);
    if(entity_reader_active()){report_status(AttachPhase::Error,"Attachment requires the entity reader to be stopped");return false;}
    mem.invalidate_attachment_epoch();snap.native_binding_epoch=0;snap.frame_valid=false;
    {std::lock_guard<std::mutex> lock(mutex);resolved=false;g=game;game_base=game_size=entity_list=view_matrix=local_ent=0;bound_pid=0;}
    auto fail=[&](AttachPhase phase,const std::string& detail){report_status(phase,detail);plat_log("Game attach failed: %s",detail.c_str());return false;};
    report_status(AttachPhase::FindingProcess,"Locating game process");
    if(game!=&games::DEADLOCK)return fail(AttachPhase::Error,"This game has no live backend");
    if(!mem.find_process(*game))return fail(AttachPhase::Error,"Deadlock process not found after process-list refresh");
    DWORD pid=mem.process_id();report_status(AttachPhase::FindingModules,"Locating required game modules");
    auto modules=[&](){return live_backend::Binding{mem.module_base("client.dll"),mem.module_base("engine2.dll"),mem.module_base("inputsystem.dll")};};
    auto binding=modules();if((!binding.client||!binding.engine||!binding.input)&&mem.refresh_process_modules())binding=modules();
    plat_log("Game modules: PID=%lu client=0x%llX engine2=0x%llX inputsystem=0x%llX",(unsigned long)pid,(unsigned long long)binding.client,(unsigned long long)binding.engine,(unsigned long long)binding.input);
    if(!binding.client||!binding.engine||!binding.input){std::string missing;if(!binding.client)missing+="client.dll ";if(!binding.engine)missing+="engine2.dll ";if(!binding.input)missing+="inputsystem.dll ";return fail(AttachPhase::Error,"Required modules unavailable: "+missing+"(PID "+std::to_string(pid)+")");}
    report_status(AttachPhase::Validating,"Validating Deadlock build and layout");
    plat_log("Layout source: compiled revision %u; module bases discovered at runtime; schema/field discovery not implemented",dl_layout::revision);
    auto failure_phase=[&](const std::string& detail){return (detail.find("build mismatch")!=std::string::npos||detail.find("global layout mismatch")!=std::string::npos)?AttachPhase::UpdateRequired:AttachPhase::Error;};
    std::string detail;bool metadata_failure=false;
    if(!live_backend::live.bind(binding,detail,&metadata_failure)){
        if(!metadata_failure)return fail(failure_phase(detail),detail);
        // Startup loader has no reader; overlay recovery joins it before resolve.
        // Never refresh/reselect beneath a running reader or continue on old data.
        if(entity_reader_active())return fail(AttachPhase::Error,"PE metadata recovery requires the reader to be stopped: "+detail);
        plat_log("PE metadata recovery: one bounded refresh after PID=%lu %s",(unsigned long)pid,detail.c_str());
        report_status(AttachPhase::FindingProcess,"Refreshing process/module metadata after PE failure");
        const bool caches=mem.refresh_caches(),processes=mem.refresh_process_list();
        plat_log("PE recovery refresh: memory/TLB=%s process_list=%s",caches?"complete":"unavailable",processes?"complete":"unavailable");
        if(!mem.find_process(*game))return fail(AttachPhase::Error,"PE recovery: game process unavailable after refresh");
        pid=mem.process_id();const bool process=mem.refresh_process_modules();binding=modules();
        plat_log("PE recovery modules: PID=%lu process_refresh=%s client=0x%llX engine2=0x%llX inputsystem=0x%llX",(unsigned long)pid,process?"complete":"unavailable",(unsigned long long)binding.client,(unsigned long long)binding.engine,(unsigned long long)binding.input);
        if(!binding.client||!binding.engine||!binding.input)return fail(AttachPhase::Error,"PE recovery: required modules unavailable after fresh lookup (PID "+std::to_string(pid)+")");
        report_status(AttachPhase::Validating,"Revalidating all modules and layout after fresh lookup");
        if(!live_backend::live.bind(binding,detail))return fail(failure_phase(detail),"After one metadata retry: "+detail);
    }
    if(mem.process_id()!=pid){live_backend::live.reset();return fail(AttachPhase::Error,"Game process changed during attachment; retry required");}
    if(mem.dma_read_ready()&&!mem.publish_attachment_epoch()){live_backend::live.reset();return fail(AttachPhase::Error,"Could not publish a unique DMA attachment epoch");}
    {std::lock_guard<std::mutex> lock(mutex);game_base=binding.client;game_size=dl_layout::client_image_size;entity_list=game_base+dl_layout::entities_rva;view_matrix=game_base+dl_layout::view_rva;bound_pid=pid;phase_=AttachPhase::Ready;report="Layout validated; waiting for live snapshots";resolved=true;}
    SYSTEMTIME attached{};GetSystemTime(&attached);plat_log("Game attachment observed: %04u-%02u-%02u %02u:%02u:%02u UTC (attachment time, not game process creation time)",attached.wYear,attached.wMonth,attached.wDay,attached.wHour,attached.wMinute,attached.wSecond);
    plat_log("Layout matched: Deadlock revision %u, PID=%lu; waiting for validated snapshots",dl_layout::revision,(unsigned long)pid);plat_stage("attach","layout ready");return true;
}

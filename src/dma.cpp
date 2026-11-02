#include "dma.h"
#include "vmm_api.h"
#include <cstddef>
#include "config.h"
#include "game.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <sstream>
#include <array>
Reader mem;
namespace {
std::string diagnostic_value(u64 value,bool hex=false){
    if(value==std::numeric_limits<u64>::max())return "unavailable";
    if(!hex)return std::to_string(value);
    std::ostringstream text;text<<"0x"<<std::hex<<value;return text.str();
}
}
std::string Reader::diagnostics() const{
    const auto pid=target_pid_.load();
    return std::string("Memory ")+(ready_.load()?(via_vmm_.load()?"DMA connected":"local connected"):"disconnected")+
        " | Process PID: "+(pid?std::to_string(pid):std::string("not found"))+
        " | Bytes read: "+std::to_string(total_read_bytes.load())+
        " | Read failures (total): "+std::to_string(read_errors.load())+" | Scalar calls: "+std::to_string(scalar_read_calls.load())+" | Scatter calls/requests: "+std::to_string(scatter_calls.load())+"/"+std::to_string(scatter_requests.load())+
        " | PCIe device ID: "+diagnostic_value(fpga_device_id_.load(),true)+
        " | FPGA ID: "+diagnostic_value(fpga_id_.load(),true)+
        " | FPGA firmware: "+(fpga_major_.load()==unavailable?std::string("unavailable"):diagnostic_value(fpga_major_.load())+"."+diagnostic_value(fpga_minor_.load()))+
        " | VMM version: "+(vmm_major_.load()==unavailable?std::string("unavailable"):diagnostic_value(vmm_major_.load())+"."+diagnostic_value(vmm_minor_.load())+"."+diagnostic_value(vmm_revision_.load()));
}
void Reader::disconnect(){
    invalidate_attachment_epoch(); // ACE_NATIVE_JOINT_INTEGRATION_V1
    // There must be no concurrent reader/attach/vendor calls at this point.
    plat_stage("dma","disconnecting");ready_=false;target_pid_=0;
    scatter_session_.close(); // Close scatter before its parent VMM handle.
    if(vmm_){using Close=void(*)(vmm_api::Handle);auto close=(Close)fn("VMMDLL_Close");if(close)close(vmm_);else plat_log("DMA disconnect: VMMDLL_Close export unavailable");vmm_=nullptr;}
    if(local_proc_){CloseHandle(local_proc_);local_proc_=nullptr;}
    via_vmm_=false;fpga_device_id_=unavailable;fpga_id_=unavailable;fpga_major_=unavailable;fpga_minor_=unavailable;
    vmm_major_=unavailable;vmm_minor_=unavailable;vmm_revision_=unavailable;
    total_read_bytes=0;read_errors=0;scalar_read_calls=0;scatter_calls=0;scatter_requests=0;requested_read_bytes=0;status.ok=false;status.text="disconnected";
    plat_log("DMA disconnected");plat_stage("dma","disconnected");
}
bool Reader::init_vmm(){
    if(ready())return true;
    using INIT=vmm_api::Initialize;
    struct ErrorInfo{DWORD version,bytes,future[16],user_request,chars;WCHAR text[1];};
    static_assert(offsetof(ErrorInfo,text)==80);
    using INITEX=vmm_api::Handle(*)(DWORD,LPCSTR[],ErrorInfo**);
    plat_stage("dma","loading VMM runtime");
    if(!vmm_dll_)vmm_dll_=LoadLibraryA("vmm.dll");
    if(!vmm_dll_){status.text="vmm.dll could not be loaded";plat_log("DMA: %s (Windows error %lu)",status.text.c_str(),GetLastError());plat_stage("dma",status.text);return false;}
    INIT initf=(INIT)fn("VMMDLL_Initialize");INITEX initex=(INITEX)fn("VMMDLL_InitializeEx");
    if(!initf&&!initex){status.text="vmm.dll has no initialization export";plat_log("DMA: %s",status.text.c_str());plat_stage("dma",status.text);return false;}
    HMODULE leech=GetModuleHandleA("leechcore.dll");if(!leech)leech=LoadLibraryA("leechcore.dll");
    auto release=(void(*)(void*)) (leech?GetProcAddress(leech,"LcMemFree"):nullptr);
    static u64 last_report=0;bool report=now_ms()-last_report>30000;
    std::string reason;
    auto attempt=[&](const char* mode,DWORD argc,const char* argv[]){
        ErrorInfo* error=nullptr;reason.clear();u64 started=now_ms();
        plat_stage("dma",std::string("initializing ")+mode);plat_log("DMA initialization started: %s",mode);
        vmm_=initex?initex(argc,argv,&error):initf(argc,argv);
        plat_log("DMA initialization returned: %s, result=%s, elapsed=%llu ms",mode,vmm_?"connected":"failed",(unsigned long long)(now_ms()-started));
        if(error){
            if(error->version==0xc0fe0002&&error->bytes>=offsetof(ErrorInfo,text)&&error->chars){
                size_t available=(error->bytes - offsetof(ErrorInfo,text))/sizeof(WCHAR);size_t count=std::min<size_t>(error->chars,available);
                std::wstring wide(error->text,count);while(!wide.empty()&&wide.back()==0)wide.pop_back();
                int bytes=WideCharToMultiByte(CP_UTF8,0,wide.data(),(int)wide.size(),nullptr,0,nullptr,nullptr);
                if(bytes>0){std::string detail(bytes,'\0');WideCharToMultiByte(CP_UTF8,0,wide.data(),(int)wide.size(),detail.data(),bytes,nullptr,nullptr);reason=detail;}
            }
            if(release)release(error);
        }
        if(vmm_){
            using GetConfig=BOOL(*)(vmm_api::Handle,ULONG64,PULONG64);auto getconfig=(GetConfig)fn("VMMDLL_ConfigGet");
            auto query=[&](const char* name,ULONG64 option,std::atomic<u64>& destination){
                destination=unavailable;if(!getconfig){plat_log("DMA metadata %s: unavailable (ConfigGet export missing)",name);return;}
                ULONG64 value=0;plat_stage("dma",std::string("reading metadata: ")+name);
                if(getconfig(vmm_,option,&value)){destination=value;plat_log("DMA metadata %s: %s",name,diagnostic_value(value,true).c_str());}
                else plat_log("DMA metadata %s: unsupported/unavailable",name);
            };
            // VMMDLL_ConfigGet accepts read-only LeechCore LC_OPT values.
            query("VMM major",0x2000000900000000ULL,vmm_major_);query("VMM minor",0x2000000A00000000ULL,vmm_minor_);query("VMM revision",0x2000000B00000000ULL,vmm_revision_);
            plat_log("VMM runtime version: %s.%s.%s",diagnostic_value(vmm_major_.load()).c_str(),diagnostic_value(vmm_minor_.load()).c_str(),diagnostic_value(vmm_revision_.load()).c_str());
            query("FPGA device ID",0x0300008000000000ULL,fpga_device_id_);query("FPGA ID",0x0300008100000000ULL,fpga_id_);
            query("FPGA version major",0x0300008200000000ULL,fpga_major_);query("FPGA version minor",0x0300008300000000ULL,fpga_minor_);
            for(const char* dll:{"vmm.dll","leechcore.dll","ftd3xx.dll"}){char path[2048]{};HMODULE module=GetModuleHandleA(dll);if(module&&GetModuleFileNameA(module,path,sizeof(path)))plat_log("Loaded runtime: %s",path);}
            target_pid_=0;total_read_bytes=0;read_errors=0;scalar_read_calls=0;scatter_calls=0;scatter_requests=0;requested_read_bytes=0;via_vmm_=true;ready_=true;status.ok=true;status.text=std::string("DMA connected (")+mode+")";
            plat_log("%s",status.text.c_str());plat_log("%s",diagnostics().c_str());plat_stage("dma","ready");return true;
        }
        plat_stage("dma",std::string("initialization failed: ")+mode);
        if(report)plat_log("DMA: %s initialization failed%s%s",mode,reason.empty()?"":": ",reason.empty()?"":reason.c_str());return false;
    };
    const char* plain[]={"","-device","fpga"};const char* verbose[]={"","-device","fpga","-printf","-v"};
    const char* algo1[]={"","-device","fpga://algo=1","-printf","-v"};const char* algo2[]={"","-device","fpga://algo=2","-printf","-v"};
    if(attempt("fpga",3,plain)||attempt("fpga verbose",5,verbose)||attempt("fpga algo=1",5,algo1)||attempt("fpga algo=2",5,algo2))return true;
    status.ok=false;status.text=reason.empty()?"ACE could not initialize FPGA (no VMM detail returned)":reason;plat_stage("dma","connection failed; awaiting retry");
    if(report){plat_log("DMA connection failed: %s",status.text.c_str());last_report=now_ms();}return false;
}
bool Reader::init_local(){
    invalidate_attachment_epoch();
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W pe{};pe.dwSize=sizeof(pe);DWORD pid=0;
    if(snap!=INVALID_HANDLE_VALUE){if(Process32FirstW(snap,&pe)){do{if(games::from_process(pe.szExeFile)){pid=pe.th32ProcessID;break;}}while(Process32NextW(snap,&pe));}CloseHandle(snap);}
    if(!pid){status.text="game not found (dev mode)";return false;}
    local_proc_=OpenProcess(PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_QUERY_INFORMATION,FALSE,pid);
    if(!local_proc_){status.text="cannot open game (admin?)";plat_log("FAILED: cannot open game - run as admin");return false;}
    target_pid_=pid;via_vmm_=false;ready_=true;status.ok=true;status.text="local read mode (dev)";plat_log("Memory: local dev read mode");return true;
}
bool Reader::read_vmm(u64 addr,void* buf,u32 size){
    auto read=(vmm_api::Read)fn("VMMDLL_MemReadEx");DWORD pid=target_pid_.load();if(!read||!pid||!buf||!size)return false;
    DWORD got=0;if(!read(vmm_,pid,addr,(PBYTE)buf,size,&got,1)||got!=size){
        u32 failures=read_errors.fetch_add(1)+1;static std::atomic<u64> last_report{0};u64 now=now_ms();
        if(failures<=3||now-last_report.load()>5000){last_report=now;plat_log("VMM read failed: PID=%lu address=0x%llX requested=%u transferred=%lu total_failures=%u",(unsigned long)pid,(unsigned long long)addr,size,(unsigned long)got,failures);}return false;
    }
    total_read_bytes.fetch_add(got);return true;
}
bool Reader::read_local(u64 addr,void* buf,u32 size){
    SIZE_T got=0;if(!ReadProcessMemory(local_proc_,(LPCVOID)(uintptr_t)addr,buf,size,&got)||(u32)got!=size){read_errors.fetch_add(1);return false;}total_read_bytes.fetch_add(size);return true;
}
bool Reader::read(u64 a,void* b,u32 s){scalar_read_calls.fetch_add(1);if(!ready())return false;requested_read_bytes.fetch_add(s);return via_vmm_.load()?read_vmm(a,b,s):read_local(a,b,s);}
bool Reader::read_batch(u64 a,void* b,u32 s){return read(a,b,s);}
bool Reader::scatter(std::vector<DmaRead>& requests){
    for(auto& r:requests){r.valid=false;r.transferred=0;}
    if(!ready()||requests.empty()||requests.size()>8192)return false;
    const DWORD pid=target_pid_.load();if(!pid)return false;
    u64 total=0;for(const auto& r:requests){if(!r.data||!r.size||r.address<0x10000||r.address>0x00007fffffffffffULL-r.size||r.size>8u*1024*1024)return false;total+=r.size;if(total>32u*1024*1024)return false;}
    scatter_calls.fetch_add(1);scatter_requests.fetch_add(requests.size());requested_read_bytes.fetch_add(total);
    if(!via_vmm_.load()){for(auto& r:requests){r.valid=read_local(r.address,r.data,r.size);r.transferred=r.valid?r.size:0;}return true;}
    const dma_scatter::Api api{(dma_scatter::Start)fn("VMMDLL_Scatter_Initialize"),(dma_scatter::Prepare)fn("VMMDLL_Scatter_PrepareEx"),(dma_scatter::Execute)fn("VMMDLL_Scatter_ExecuteRead"),(dma_scatter::Clear)fn("VMMDLL_Scatter_Clear"),(dma_scatter::Close)fn("VMMDLL_Scatter_CloseHandle")};
    if(!api.available()){read_errors.fetch_add((u32)requests.size());return false;}
    const auto epoch=attachment_epoch();
    const bool executed=scatter_session_.read(api,vmm_,pid,epoch,requests);
    const bool current=dma_scatter::same_context(ready(),pid,target_pid_.load(),epoch,attachment_epoch());
    for(auto& r:requests){r.valid=current&&r.valid&&r.transferred==r.size;if(r.valid)total_read_bytes.fetch_add(r.size);else{r.transferred=0;read_errors.fetch_add(1);}}
    return executed;
}
bool Reader::write(u64 a,const void* b,u32 s){
    if(!ready()||!cfg.write_allowed())return false;
    if(!via_vmm_.load()&&local_proc_){SIZE_T got=0;return WriteProcessMemory(local_proc_,(LPVOID)(uintptr_t)a,(LPCVOID)b,s,&got)&&(u32)got==s;}
    auto write=(vmm_api::Write)fn("VMMDLL_MemWrite");DWORD pid=target_pid_.load();return write&&pid&&write(vmm_,pid,a,(PBYTE)b,s);
}
bool Reader::read_hwid(std::string& out){
    HKEY k=nullptr;
    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Cryptography",0,KEY_READ|KEY_WOW64_64KEY,&k)==ERROR_SUCCESS){
        char buf[128]{};DWORD sz=sizeof(buf)-1;if(RegQueryValueExA(k,"MachineGuid",0,0,(BYTE*)buf,&sz)==ERROR_SUCCESS){RegCloseKey(k);out=buf;return true;}RegCloseKey(k);
    }
    out="unknown-hwid";return false;
}
bool Reader::refresh_process_list(){
    if(!ready()||!via_vmm_.load())return false;auto configure=(vmm_api::Configure)fn("VMMDLL_ConfigSet");
    plat_stage("attach","refreshing process list");u64 start=now_ms();bool result=configure&&configure(vmm_,vmm_api::refresh_fast,1)!=FALSE;
    plat_log("Process list refresh: %s, elapsed=%llu ms",result?"complete":"unavailable",(unsigned long long)(now_ms()-start));return result;
}
bool Reader::refresh_process_modules(){
    DWORD pid=target_pid_.load();if(!ready()||!via_vmm_.load()||!pid)return false;auto configure=(vmm_api::Configure)fn("VMMDLL_ConfigSet");
    plat_stage("attach","refreshing selected process");u64 start=now_ms();bool result=configure&&configure(vmm_,vmm_api::refresh_process|(ULONG64)pid,1)!=FALSE;
    plat_log("Process refresh: PID=%lu, %s, elapsed=%llu ms",(unsigned long)pid,result?"complete":"unavailable",(unsigned long long)(now_ms()-start));return result;
}
bool Reader::find_process(const GameModule& game){
    invalidate_attachment_epoch();
    if(!ready())return false;if(!via_vmm_.load())return local_proc_&&target_pid_.load()!=0;
    auto lookup=(vmm_api::Pid)fn("VMMDLL_PidGetFromName");if(!lookup){target_pid_=0;plat_log("Process discovery unavailable: VMMDLL_PidGetFromName export missing");return false;}
    std::array<std::string,4> converted;std::array<const char*,4> aliases{};
    for(unsigned i=0;i<4&&game.exes[i];i++){char name[260]{};if(WideCharToMultiByte(CP_UTF8,0,game.exes[i],-1,name,sizeof(name),nullptr,nullptr)>0){converted[i]=name;aliases[i]=converted[i].c_str();}}
    auto search=[&](){return vmm_api::first_pid(aliases.data(),aliases.size(),[&](const char* name,PDWORD pid){u64 started=now_ms();plat_stage("attach",std::string("PID lookup: ")+name);BOOL found=lookup(vmm_,name,pid);plat_log("PID lookup %s: %s, PID=%lu, elapsed=%llu ms",name,found&&*pid?"found":"not found",(unsigned long)*pid,(unsigned long long)(now_ms()-started));return found;});};
    DWORD selected=search();if(!selected&&refresh_process_list())selected=search();
    target_pid_=selected;if(selected)plat_log("Game process selected: %s, PID=%lu (main EXE module lookup is not required)",game.name,(unsigned long)selected);return selected!=0;
}
u64 Reader::module_base(const char* name){
    if(!ready()||!name||!*name)return 0;
    if(via_vmm_.load()){
        auto get_base=(vmm_api::Module)fn("VMMDLL_ProcessGetModuleBaseU");DWORD pid=target_pid_.load();
        if(!get_base||!pid){plat_log("Module lookup unavailable: %s (PID=%lu, export=%s)",name,(unsigned long)pid,get_base?"present":"missing");return 0;}
        u64 start=now_ms();plat_stage("attach",std::string("module lookup: ")+name);u64 base=get_base(vmm_,pid,name);plat_log("Module %s: PID=%lu base=0x%llX elapsed=%llu ms",name,(unsigned long)pid,(unsigned long long)base,(unsigned long long)(now_ms()-start));return base;
    }
    HMODULE mods[512];DWORD cb=0;
    if(local_proc_&&EnumProcessModules(local_proc_,mods,sizeof(mods),&cb))for(DWORD i=0;i<std::min<DWORD>(cb/sizeof(HMODULE),512);i++){char nm[64];if(GetModuleBaseNameA(local_proc_,mods[i],nm,64)&&_stricmp(nm,name)==0)return (u64)mods[i];}
    return 0;
}
u64 Reader::find_pattern(u64 start,u64 size,const char* pattern,const char* mask){
    if(!pattern||!mask||!*mask||size==0)return 0;const u32 PMAX=64;u32 plen=(u32)strlen(mask);if(plen>PMAX)plen=PMAX;std::vector<u8> window(0x10000);
    for(u64 off=0;off+plen<size;off+=0x9000){u32 chunk=(u32)(((off+0x10000)<size)?0x10000:(size-off));if(chunk<plen)break;if(!read(start+off,window.data(),chunk))continue;
        for(u32 i=0;i+plen<=chunk;i++){bool ok=true;for(u32 j=0;j<plen;j++)if(mask[j]=='x'&&window[i+j]!=(u8)pattern[j]){ok=false;break;}if(ok)return start+off+i;}}
    return 0;
}
bool Reader::refresh_caches(){
    if(!ready())return false;if(!via_vmm_.load())return true;auto configure=(vmm_api::Configure)fn("VMMDLL_ConfigSet");return configure&&configure(vmm_,0x2001ffff00000000ULL,1)!=FALSE;
}

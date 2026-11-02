#include "utils.h"
#include "build_info.h"
#include <windows.h>
#include <cstdarg>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <sstream>
namespace {
constexpr std::uintmax_t max_log_bytes=5*1024*1024;
std::mutex file_mutex,console_mutex,stage_mutex,wait_mutex;
std::ofstream log_file;
std::filesystem::path log_name;
std::string session_banner;
std::deque<std::string> recent;
std::once_flag init_once;
std::atomic<bool> heartbeat_running{false};
std::condition_variable heartbeat_wake;
std::thread heartbeat_thread;
struct Stage {std::string detail;u64 started=0;};
std::map<std::string,Stage> stages;
std::string utf8(const std::wstring& value){
    if(value.empty())return {};
    int length=WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),nullptr,0,nullptr,nullptr);
    if(length<=0)return {};std::string result(length,'\0');
    WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),result.data(),length,nullptr,nullptr);return result;
}
bool rotate_locked(bool header=true){
    if(log_name.empty())return false;
    log_file.close();std::filesystem::path backup=log_name;backup+=L".1";
    bool moved=MoveFileExW(log_name.c_str(),backup.c_str(),MOVEFILE_REPLACE_EXISTING)!=0;
    log_file.open(log_name,std::ios::out|(moved?std::ios::trunc:std::ios::app));
    if(moved&&header&&log_file.is_open())log_file<<session_banner<<'\n';
    return moved&&log_file.is_open();
}
void heartbeat(){
    std::unique_lock wait(wait_mutex);
    while(heartbeat_running){
        if(heartbeat_wake.wait_for(wait,std::chrono::seconds(5),[]{return !heartbeat_running.load();}))break;
        wait.unlock();std::vector<std::pair<std::string,Stage>> snapshot;
        {std::lock_guard lock(stage_mutex);snapshot.assign(stages.begin(),stages.end());}
        std::string line="Health";u64 now=now_ms();
        if(snapshot.empty())line+=" | no startup/reader stage has been registered";
        for(const auto& item:snapshot){line+=" | "+item.first+"="+item.second.detail+" ("+std::to_string(now-item.second.started)+" ms)";if(line.size()>1700){line+=" ...";break;}}
        plat_log("%s",line.c_str());wait.lock();
    }
}
struct Shutdown {~Shutdown(){plat_shutdown_diagnostics();}} shutdown_guard;
}
void plat_init_console(){
    std::call_once(init_once,[]{
        if(!GetConsoleWindow())AllocConsole();
        SetConsoleTitleA("AceSoftware diagnostics");
        FILE* stream=nullptr;freopen_s(&stream,"CONOUT$","w",stdout);freopen_s(&stream,"CONOUT$","w",stderr);
        setvbuf(stdout,nullptr,_IONBF,0);setvbuf(stderr,nullptr,_IONBF,0);
        DWORD input_mode=0;HANDLE input=GetStdHandle(STD_INPUT_HANDLE);
        if(GetConsoleMode(input,&input_mode))SetConsoleMode(input,(input_mode|ENABLE_EXTENDED_FLAGS)&~ENABLE_QUICK_EDIT_MODE);
        SYSTEMTIME started{};GetSystemTime(&started);char banner[256]{};
        snprintf(banner,sizeof(banner),"Log file | ACE session started %04u-%02u-%02u %02u:%02u:%02u.%03u UTC | Build %s",started.wYear,started.wMonth,started.wDay,started.wHour,started.wMinute,started.wSecond,started.wMilliseconds,build_info::id);session_banner=banner;
        try{log_name=std::filesystem::absolute(L"Log file.log");}catch(...){log_name=L"Log file.log";}
        {std::lock_guard lock(file_mutex);std::error_code error;auto length=std::filesystem::file_size(log_name,error);
            if(!error&&length>0){if(!rotate_locked(false)){
                log_file.close();WCHAR fallback[160]{};swprintf(fallback,std::size(fallback),L"Log file - %04u-%02u-%02u_%02u-%02u-%02u-%03u-%lu.log",started.wYear,started.wMonth,started.wDay,started.wHour,started.wMinute,started.wSecond,started.wMilliseconds,(unsigned long)GetCurrentProcessId());log_name=log_name.parent_path()/fallback;log_file.open(log_name,std::ios::out|std::ios::trunc);
            }}else log_file.open(log_name,std::ios::out|std::ios::trunc);}
        plat_log("%s",session_banner.c_str());
        plat_log("Support log: %s",plat_log_path().c_str());
        WCHAR path[32768]{};DWORD count=GetModuleFileNameW(nullptr,path,(DWORD)std::size(path));
        if(count&&count<std::size(path))plat_log("Executable: %s",utf8(std::wstring(path,count)).c_str());
        count=GetCurrentDirectoryW((DWORD)std::size(path),path);
        if(count&&count<std::size(path))plat_log("Working directory: %s",utf8(std::wstring(path,count)).c_str());
        SYSTEM_INFO system{};GetNativeSystemInfo(&system);MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);
        if(GlobalMemoryStatusEx(&memory))plat_log("System: architecture=%u, CPU logical processors=%lu, RAM total=%llu MiB available=%llu MiB",(unsigned)system.wProcessorArchitecture,(unsigned long)system.dwNumberOfProcessors,(unsigned long long)(memory.ullTotalPhys/1048576),(unsigned long long)(memory.ullAvailPhys/1048576));
        DEVMODEW display{};display.dmSize=sizeof(display);
        if(EnumDisplaySettingsW(nullptr,ENUM_CURRENT_SETTINGS,&display))plat_log("Primary display: %lux%lu at %lu Hz, %lu bits/pixel",(unsigned long)display.dmPelsWidth,(unsigned long)display.dmPelsHeight,(unsigned long)display.dmDisplayFrequency,(unsigned long)display.dmBitsPerPel);
        plat_stage("app","startup",false);heartbeat_running=true;heartbeat_thread=std::thread(heartbeat);
    });
}
void plat_log(const char* fmt,...){
    char body[2048]{};va_list arguments;va_start(arguments,fmt);vsnprintf(body,sizeof(body),fmt,arguments);va_end(arguments);
    SYSTEMTIME clock;GetLocalTime(&clock);char prefix[64];
    snprintf(prefix,sizeof(prefix),"[%04u-%02u-%02u %02u:%02u:%02u.%03u] ",clock.wYear,clock.wMonth,clock.wDay,clock.wHour,clock.wMinute,clock.wSecond,clock.wMilliseconds);
    std::string line=std::string(prefix)+body;
    {std::lock_guard lock(file_mutex);recent.push_back(line);while(recent.size()>500)recent.pop_front();
        if(log_file.is_open()){auto pos=log_file.tellp();if(pos>=0&&(std::uintmax_t)pos+line.size()+1>max_log_bytes)rotate_locked();log_file<<line<<'\n';log_file.flush();}}
    // Console output never holds the file/ring mutex, so support-log snapshots
    // and disk logging remain available even if the console host is busy.
    {std::lock_guard lock(console_mutex);fwrite(line.data(),1,line.size(),stdout);fputc('\n',stdout);}
}
void plat_stage(const std::string& component,const std::string& detail,bool announce){
    bool changed=false;
    {std::lock_guard lock(stage_mutex);auto found=stages.find(component);changed=found==stages.end()||found->second.detail!=detail;stages[component]={detail,now_ms()};}
    if(changed&&announce)plat_log("Stage: %s / %s",component.c_str(),detail.c_str());
}
std::vector<std::string> plat_recent_logs(){std::lock_guard lock(file_mutex);return {recent.begin(),recent.end()};}
std::string plat_log_path(){std::lock_guard lock(file_mutex);return utf8(log_name.wstring());}
void plat_show_console(){
    HWND console=GetConsoleWindow();if(!console)return;
    ShowWindow(console,SW_RESTORE);SetWindowPos(console,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);SetForegroundWindow(console);
}
void plat_shutdown_diagnostics(){
    heartbeat_running=false;heartbeat_wake.notify_all();
    if(heartbeat_thread.joinable()&&heartbeat_thread.get_id()!=std::this_thread::get_id())heartbeat_thread.join();
    std::lock_guard lock(file_mutex);if(log_file.is_open()){log_file.flush();log_file.close();}
}

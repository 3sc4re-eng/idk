#include "decision_frame.h"
#include "entities.h"
#include "km_protocol.h"
#include "input.h"
#include "dma.h"
#include "decision_frame.h"
#include <condition_variable>
#include "input_timing.h"
#include "makcu_serial_parser.h"
#include <array>
#include <thread>
#include <exception>
#include "config.h"
#include "aim_model.h"
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
InputState inp;
static std::recursive_mutex serial_mutex;
static std::recursive_mutex connection_mutex;
static HANDLE serial=INVALID_HANDLE_VALUE;static std::atomic<u64> serial_sent{0},serial_failed{0};
static makcu_serial::Parser serial_parser;static bool serial_query_synchronized=false;static std::atomic<bool> device_is_makcu{false};static std::atomic<u64> seen_stream_events{0};
static void stop_mouse_monitor();static void start_mouse_monitor();
static void quiesce_output();static void connected_output();
static bool open_port(int com,int baud=115200){
    std::lock_guard<std::recursive_mutex> lock(serial_mutex);
    serial_parser.reset();serial_query_synchronized=false;device_is_makcu=false;seen_stream_events=0;
    char name[32]; snprintf(name,sizeof(name),"\\\\.\\COM%d",com);
    plat_stage("input","opening COM"+std::to_string(com)+" at "+std::to_string(baud));
    serial=CreateFileA(name,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(serial==INVALID_HANDLE_VALUE){plat_log("COM%d at %d: open failed (Win32 %lu)",com,baud,(unsigned long)GetLastError());return false;}
    DCB dcb{}; dcb.DCBlength=sizeof(dcb);
    if(!GetCommState(serial,&dcb)){plat_log("COM%d: GetCommState failed (Win32 %lu)",com,(unsigned long)GetLastError()); CloseHandle(serial); serial=INVALID_HANDLE_VALUE; return false; }
    dcb.BaudRate=baud; dcb.ByteSize=8; dcb.Parity=NOPARITY; dcb.StopBits=ONESTOPBIT;
    if(!SetCommState(serial,&dcb)){plat_log("COM%d at %d: SetCommState failed (Win32 %lu)",com,baud,(unsigned long)GetLastError()); CloseHandle(serial); serial=INVALID_HANDLE_VALUE; return false; }
    COMMTIMEOUTS to{};
    to.ReadIntervalTimeout=MAXDWORD;
    to.ReadTotalTimeoutMultiplier=0;
    to.ReadTotalTimeoutConstant=0;
    to.WriteTotalTimeoutMultiplier=0;
    to.WriteTotalTimeoutConstant=50;
    if(!SetCommTimeouts(serial,&to)){plat_log("COM%d: timeout configuration failed (Win32 %lu)",com,(unsigned long)GetLastError());CloseHandle(serial);serial=INVALID_HANDLE_VALUE;return false;}
    PurgeComm(serial,PURGE_RXCLEAR|PURGE_TXCLEAR);
    inp.com_port=com;inp.baud_rate=baud;
    return true;
}
static void close_port(){
    stop_mouse_monitor();
    quiesce_output(); // drain owed releases before locking/closing the handle
    std::lock_guard<std::recursive_mutex> lock(serial_mutex);inp.device_ok=false;inp.keyboard_ok=false;inp.keyboard_api=false;device_is_makcu=false;
    if(serial!=INVALID_HANDLE_VALUE) CloseHandle(serial);
    serial=INVALID_HANDLE_VALUE;
}
static bool wr(const char* fmt,...){
    std::lock_guard<std::recursive_mutex> lock(serial_mutex);
    if(serial==INVALID_HANDLE_VALUE) return false;
    char buf[128]; va_list v; va_start(v,fmt);
    int n=vsnprintf(buf,sizeof(buf),fmt,v); va_end(v);
    if(n<=0||n>=(int)sizeof(buf)) return false;
    DWORD w_=0;bool ok=WriteFile(serial,buf,(DWORD)n,&w_,0)&&w_==(DWORD)n;if(ok)serial_sent++;else serial_failed++;return ok;
}
static bool pump_serial_locked(){
    for(int pass=0;pass<16;pass++){
        char bytes[256];DWORD got=0;if(!ReadFile(serial,bytes,sizeof(bytes),&got,nullptr))return false;
        if(!got)break;serial_parser.feed(bytes,got);seen_stream_events=serial_parser.stream_events;
    }return true;
}
struct SerialQueryTrace{std::string observed;u64 elapsed_ms=0;bool timed_out=false;};
static bool reply_matches(const std::string& query,const std::string& frame){
    if(frame.find("ERR")!=std::string::npos)return true;
    if(query=="km.version()")return frame.find("MAKCU")!=std::string::npos||frame.find("KMBOX")!=std::string::npos||frame.find("KMBox")!=std::string::npos;
    if(query=="km.ping()")return frame.find("PONG")!=std::string::npos;
    int value=0;return makcu_serial::ownership(frame,query,value);
}
static std::string query_locked(const char* query,const char* ending,unsigned timeout_ms,SerialQueryTrace* trace){
    if(!pump_serial_locked())return {};serial_parser.discard_completed();
    if(!wr("%s%s",query,ending))return {};
    u64 start=now_ms();while(now_ms()-start<std::max(1u,timeout_ms)){
        if(!pump_serial_locked())return {};std::string frame;
        while(serial_parser.take(frame)){
            if(trace&&trace->observed.size()<256)trace->observed+=(trace->observed.empty()?"":" | ")+std::string(query)+":"+frame.substr(0,256-trace->observed.size());
            if(reply_matches(query,frame)){if(trace)trace->elapsed_ms+=now_ms()-start;return frame;}
        }
        Sleep(1);
    }
    if(trace){trace->elapsed_ms+=now_ms()-start;trace->timed_out=true;auto partial=serial_parser.pending_preview();if(!makcu_serial::trimmed(partial).empty()&&trace->observed.size()<256)trace->observed+=" <partial>"+partial;}
    return {};
}
static std::string query_device(const char* query,const char* ending="\r\n",unsigned timeout_ms=180,SerialQueryTrace* trace=nullptr){
    std::lock_guard<std::recursive_mutex> lock(serial_mutex);if(trace)*trace={};
    if(serial==INVALID_HANDLE_VALUE)return {};
    if(!serial_query_synchronized&&std::strcmp(query,"km.version()")!=0){
        auto barrier=query_locked("km.version()","\r",std::min(timeout_ms,35u),trace);
        serial_query_synchronized=barrier.find("MAKCU")!=std::string::npos||barrier.find("KMBOX")!=std::string::npos||barrier.find("KMBox")!=std::string::npos;
        if(!serial_query_synchronized)return {};
    }
    auto reply=query_locked(query,ending,timeout_ms,trace);serial_query_synchronized=!reply.empty()&&reply.find("ERR")==std::string::npos;return reply;
}
static bool ping_device(){
    auto version=query_device("km.version()","\r");
    bool valid=version.find("MAKCU")!=std::string::npos||version.find("KMBOX")!=std::string::npos||version.find("KMBox")!=std::string::npos;
    if(!valid){auto alternate=query_device("km.version()","\r\n");valid=alternate.find("MAKCU")!=std::string::npos||alternate.find("KMBOX")!=std::string::npos||alternate.find("KMBox")!=std::string::npos;version+=alternate;}
    if(!valid){auto ping=query_device("km.ping()","\n");valid=ping.find("PONG")!=std::string::npos;version+=ping;}
    plat_log("COM%d probe: %s (%u response bytes)",inp.com_port,valid?"recognized device":"no recognized reply",(unsigned)version.size());
    if(!valid)return false;device_is_makcu=version.find("MAKCU")!=std::string::npos;
    auto keyboard=query_device("km.isdown(65)");inp.keyboard_api=keyboard.find("ERR")==std::string::npos&&(keyboard.find("0\r\n")!=std::string::npos||keyboard.find("1\r\n")!=std::string::npos||keyboard.find("2\r\n")!=std::string::npos||keyboard.find("3\r\n")!=std::string::npos);inp.keyboard_ok=false;
    return true;
}
struct PhysicalMouseMonitor{
    std::atomic<bool> run{false};std::thread worker;
    std::atomic<unsigned> physical{0},injected{0},failures{0};
    std::array<std::atomic<u64>,5> samples{};std::array<std::atomic<unsigned>,5> button_failures{};
    std::array<std::atomic<int>,5> capability{};std::atomic<u64> cycle_ms{0};std::atomic<int> state{0};
    ~PhysicalMouseMonitor(){run=false;if(worker.joinable())worker.join();}
};
static PhysicalMouseMonitor& physical_monitor(){static PhysicalMouseMonitor value;return value;}
static int physical_button_index(int key){switch(key){case VK_LBUTTON:return 0;case VK_RBUTTON:return 1;case VK_MBUTTON:return 2;case VK_XBUTTON1:return 3;case VK_XBUTTON2:return 4;default:return -1;}}
static std::string reply_preview(const std::string& input){
    if(input.empty())return "<no reply>";std::string text;char hex[8];
    for(unsigned char c:input){if(text.size()>160){text+="...";break;}if(c=='\r')text+="\\r";else if(c=='\n')text+="\\n";else if(c>=32&&c<127)text+=(char)c;else{snprintf(hex,sizeof(hex),"\\x%02X",c);text+=hex;}}
    return text;
}
static void stop_mouse_monitor(){
    auto& monitor=physical_monitor();monitor.run=false;if(monitor.worker.joinable())monitor.worker.join();
    for(auto& sample:monitor.samples)sample=0;for(auto& value:monitor.button_failures)value=0;for(auto& value:monitor.capability)value=0;
    monitor.physical=monitor.injected=0;monitor.cycle_ms=0;monitor.state=0;
}
static void start_mouse_monitor(){
    stop_mouse_monitor();auto& monitor=physical_monitor();monitor.failures=0;
    if(!device_is_makcu){monitor.state=4;return;}monitor.run=true;monitor.state=1;
    try{monitor.worker=std::thread([](){
        auto& monitor=physical_monitor();const char* primary[]={"km.left()","km.right()","km.middle()","km.side1()","km.side2()"};
        const char* alternate[]={nullptr,nullptr,nullptr,"km.ms1()","km.ms2()"};const char* endings[]={"\r\n","\r","\n"};
        std::array<unsigned,5> variants{};std::array<u64,5> retry_at{};unsigned probe_cursor=0;u64 last_report=0,last_cycle_start=0;
        plat_log("Physical mouse monitor started: independently verifying all five ownership queries; serial output connection is a separate capability");
        auto poll=[&](int index){
            unsigned alternatives=alternate[index]?2:1,variant=variants[index],query_index=variant%alternatives,ending_index=variant/alternatives;
            const char* query=query_index?alternate[index]:primary[index];SerialQueryTrace trace;int ownership=0;unsigned bit=1u<<index;
            auto reply=query_device(query,endings[ending_index],25,&trace);
            if(!reply.empty()&&makcu_serial::ownership(reply,query,ownership)){
                bool first=monitor.capability[index].load()!=1;unsigned before=monitor.physical.load();
                if(ownership&1)monitor.physical.fetch_or(bit);else monitor.physical.fetch_and(~bit);
                if(ownership&2)monitor.injected.fetch_or(bit);else monitor.injected.fetch_and(~bit);
                monitor.samples[index]=now_ms();monitor.capability[index]=1;monitor.button_failures[index]=0;retry_at[index]=0;
                if(first)plat_log("Physical button capability verified: button=%d query=%s ending=%u ownership=%d reply=%s",index+1,query,ending_index,ownership,reply_preview(reply).c_str());
                if((before&bit)!=(monitor.physical.load()&bit))plat_log("Physical mouse transition: button=%d held=%d ownership=%d (injected ownership excluded)",index+1,(ownership&1)!=0,ownership);
            }else{
                monitor.samples[index]=0;monitor.capability[index]=trace.timed_out?2:3;monitor.failures++;unsigned failed=++monitor.button_failures[index];
                variants[index]=(variant+1)%(alternatives*3);retry_at[index]=now_ms()+(failed<=alternatives*3?80:3000);
                if(failed<=alternatives*3||failed%20==0)plat_log("Physical button query unavailable: button=%d query=%s ending=%u elapsed=%llu ms timeout=%d reply=%s; retrying this button independently",index+1,query,ending_index,(unsigned long long)trace.elapsed_ms,trace.timed_out,reply_preview(trace.observed).c_str());
            }
        };
        while(monitor.run){
            u64 cycle_start=now_ms();if(last_cycle_start)monitor.cycle_ms=cycle_start-last_cycle_start;last_cycle_start=cycle_start;
            // Keep verified buttons fresh first. One unavailable-button probe per
            // cycle cannot prevent other buttons from continuing to work.
            for(int index=0;index<5&&monitor.run;index++)if(monitor.capability[index].load()==1)poll(index);
            for(int checked=0;checked<5&&monitor.run;checked++){
                int index=(int)(probe_cursor++%5);if(monitor.capability[index].load()!=1&&now_ms()>=retry_at[index]){poll(index);break;}
            }
            u64 now=now_ms();unsigned known=0;
            for(auto& sample:monitor.samples){u64 at=sample.load();known+=at&&now>=at&&now-at<=250;}
            monitor.state=known==5?2:1;
            if(now-last_report>5000){last_report=now;plat_log("Physical mouse monitor: known=%u/5 cycle=%llu ms physical_mask=0x%02X injected_mask=0x%02X failures=%u/%u/%u/%u/%u",known,(unsigned long long)monitor.cycle_ms.load(),monitor.physical.load(),monitor.injected.load(),monitor.button_failures[0].load(),monitor.button_failures[1].load(),monitor.button_failures[2].load(),monitor.button_failures[3].load(),monitor.button_failures[4].load());}
            u64 deadline=cycle_start+40;while(monitor.run&&now_ms()<deadline)Sleep((DWORD)std::min<u64>(5,deadline-now_ms()));
        }
    });}catch(const std::exception& error){monitor.run=false;monitor.state=3;plat_log("Physical mouse monitor could not start: %s",error.what());}
}
static bool try_port(int port){for(int baud:{4000000,115200}){int selected=cfg.I("input.baud");if((selected==1&&baud!=115200)||(selected==2&&baud!=4000000))continue;if(!open_port(port,baud))continue;plat_log("Probing COM%d at %d baud",port,baud);if(ping_device())return true;close_port();}return false;}
static std::vector<int> enum_com_ports(){
    std::vector<int> out;
    std::vector<char> names;
    DWORD length=0;
    for(DWORD capacity=4096;capacity<=262144;capacity*=2){
        names.resize(capacity);length=QueryDosDeviceA(nullptr,names.data(),capacity);
        if(length)break;
        if(GetLastError()!=ERROR_INSUFFICIENT_BUFFER)return out;
    }
    if(!length)return out;
    for(const char* p=names.data();*p;p+=strlen(p)+1){
        if(strncmp(p,"COM",3)!=0)continue;
        char* end=nullptr;long port=strtol(p+3,&end,10);
        if(!end||*end||port<=0||port>256)continue;
        char target[512]{};DWORD size=QueryDosDeviceA(p,target,sizeof(target));
        if(size&&strstr(target,"Bth"))continue;
        out.push_back((int)port);
    }
    std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());
    return out;
}
static bool possible_unlisted_port(int port){
    char name[20],target[512]{};snprintf(name,sizeof(name),"COM%d",port);
    DWORD size=QueryDosDeviceA(name,target,sizeof(target));
    return !size||strstr(target,"Bth")==nullptr;
}
namespace hw{
bool physical_mouse_state(int key,bool& held){
    held=false;int index=physical_button_index(key);if(index<0||!inp.device_ok||!device_is_makcu)return false;
    auto& monitor=physical_monitor();u64 sampled=monitor.samples[index].load(),now=now_ms();
    if(!sampled||now<sampled||now-sampled>250)return false;held=(monitor.physical.load()&(1u<<index))!=0;return true;
}
std::string physical_mouse_status(){
    if(!inp.device_ok)return "Serial output: disconnected / physical buttons unavailable";
    if(!device_is_makcu)return "Serial output: connected / physical button queries unavailable for this device";
    auto& monitor=physical_monitor();u64 now=now_ms();const char* names[]={"Left","Right","Middle","M4","M5"};std::string text="Physical buttons: ";
    for(int index=0;index<5;index++){u64 at=monitor.samples[index].load();bool known=at&&now>=at&&now-at<=250;bool held=(monitor.physical.load()&(1u<<index))!=0;
        if(index)text+=" / ";text+=names[index];text+=known?(held?": held":": released"): ": unknown";
    }return text+" / cycle "+std::to_string(monitor.cycle_ms.load())+" ms";
}
std::string diagnostic_summary(){return std::string("device=")+(inp.device_ok?"connected":"disconnected")+" panic="+(inp.panic?"on":"off")+" output="+(cfg.I("input.mode")==2?"disabled":"selected device")+" | "+physical_mouse_status()+" | serial command writes="+std::to_string(serial_sent.load())+" failures="+std::to_string(serial_failed.load())+" | "+automatic_output_status();}
static std::mutex diagnostic_mutex;static std::string keyboard_message="Keyboard output not tested";static u64 keyboard_test_started=0;static int keyboard_test_key=0;
std::string keyboard_status(){std::lock_guard<std::mutex> lock(diagnostic_mutex);return keyboard_message;}
bool can_send(int key){if(!Settings::action_output_key_allowed(key))return false;bool mouse=key==VK_LBUTTON||key==VK_RBUTTON||key==VK_MBUTTON||key==VK_XBUTTON1||key==VK_XBUTTON2;return inp.device_ok&&(mouse||(inp.keyboard_ok&&km_protocol::hid(key)>=0));}
std::string test_keyboard(){
    std::lock_guard<std::mutex> lock(diagnostic_mutex);int key=cfg.I("input.test_key");
    if(!inp.device_ok||!inp.keyboard_api)return keyboard_message="Keyboard API is unavailable on this connection";
    if(!snap.backend_managed||!snap.input_valid)return keyboard_message="Attach to the game so key delivery can be verified";
    if(inp.panic||cfg.I("input.mode")==2)return keyboard_message="Keyboard test paused by the panic key or disabled output device";
    if(game_key_down(key))return keyboard_message="Release the test key first";
    auto command=km_protocol::press(key,180);if(command.empty())return keyboard_message="Choose a supported keyboard test key";
    inp.keyboard_ok=false;keyboard_test_key=key;keyboard_test_started=now_ms();if(!wr("%s",command.c_str())){keyboard_test_started=0;return keyboard_message="Could not send the keyboard test to the device";}return keyboard_message="Test sent; waiting for the game-PC key bitmap";
}
void update_diagnostics(){
    std::lock_guard<std::mutex> lock(diagnostic_mutex);if(!keyboard_test_started)return;
    if(game_key_down(keyboard_test_key)){inp.keyboard_ok=true;keyboard_test_started=0;keyboard_message="Keyboard delivery verified by the game-PC key bitmap";}
    else if(now_ms()-keyboard_test_started>1200){inp.keyboard_ok=false;keyboard_test_started=0;keyboard_message="Keyboard delivery not observed; keyboard actions remain disabled";}
}
Vec2 screen_to_counts(Vec2 pixels,u64 width,u64 height,float dt,float max_turn){
    if(!decision_frame::managed())return pixels;float sensitivity=cfg.F("input.sensitivity"),adjust=1,fx=1,fy=1;
    if(const auto* frame=decision_frame::active()){
        if(cfg.B("input.auto_sensitivity")){sensitivity=frame->sensitivity;adjust=frame->fov_sensitivity;}
        fx=frame->projection_x();fy=std::sqrt(frame->view[4]*frame->view[4]+frame->view[5]*frame->view[5]+frame->view[6]*frame->view[6]);
    }else{std::lock_guard<std::mutex> lock(snap_mtx);if(cfg.B("input.auto_sensitivity")){sensitivity=snap.sensitivity;adjust=snap.fov_sensitivity;}
        fx=std::sqrt(snap.view[0]*snap.view[0]+snap.view[1]*snap.view[1]+snap.view[2]*snap.view[2]);fy=std::sqrt(snap.view[4]*snap.view[4]+snap.view[5]*snap.view[5]+snap.view[6]*snap.view[6]);}
    float x=cfg.F("input.yaw")*sensitivity*adjust,y=cfg.F("input.pitch")*sensitivity*adjust;if(x<=0||y<=0||fx<1e-5f||fy<1e-5f||!width||!height)return {};
    Vec2 angle{rad2deg(std::atan2(pixels.x,width*.5f*fx)),rad2deg(std::atan2(pixels.y,height*.5f*fy))};float limit=max_turn*clampf(dt,0,.067f);if(limit>0&&angle.len()>limit)angle=angle*(limit/angle.len());Vec2 counts{angle.x/x,angle.y/y};if(!std::isfinite(counts.len()))return {};return {clampf(counts.x,-32767,32767),clampf(counts.y,-32767,32767)};
}
Vec2 guided_move(Vec2 error,aim_model::Movement& state,u64 width,u64 height,float dt,bool magnet){
    const auto key=[&](const char* aim,const char* trigger){return magnet?trigger:aim;};
    const float smooth=aim_model::smoothing_factor(cfg.F(key("aim.smooth","trigger.magnet_smooth")));
    const float strength=cfg.F(key("aim.strength","trigger.magnet_strength"))/100.f;
    const int curve=cfg.I(key("aim.curve","trigger.magnet_curve"));
    if(!decision_frame::managed())return aim_model::consistent_motion(error,state,smooth,curve,dt,strength);
    float fx=0,fy=0,sensitivity=cfg.F("input.sensitivity"),adjust=1;
    if(const auto* frame=decision_frame::active()){
        fx=frame->projection_x();fy=std::sqrt(frame->view[4]*frame->view[4]+frame->view[5]*frame->view[5]+frame->view[6]*frame->view[6]);
        if(cfg.B("input.auto_sensitivity")){sensitivity=frame->sensitivity;adjust=frame->fov_sensitivity;}
    }else{std::lock_guard<std::mutex> lock(snap_mtx);
        fx=std::sqrt(snap.view[0]*snap.view[0]+snap.view[1]*snap.view[1]+snap.view[2]*snap.view[2]);
        fy=std::sqrt(snap.view[4]*snap.view[4]+snap.view[5]*snap.view[5]+snap.view[6]*snap.view[6]);
        if(cfg.B("input.auto_sensitivity")){sensitivity=snap.sensitivity;adjust=snap.fov_sensitivity;}
    }
    const float x=cfg.F("input.yaw")*sensitivity*adjust,y=cfg.F("input.pitch")*sensitivity*adjust;
    if(!width||!height||!std::isfinite(fx+fy+x+y)||fx<1e-5f||fy<1e-5f||x<=0||y<=0){state={};return {};}
    const Vec2 angle{rad2deg(std::atan2(error.x,width*.5f*fx)),rad2deg(std::atan2(error.y,height*.5f*fy))};
    const auto step=aim_model::consistent_motion(angle,state,smooth,curve,dt,strength);
    const Vec2 counts{step.x/x,step.y/y};
    if(!std::isfinite(counts.len())){state={};return {};}
    return {clampf(counts.x,-32767,32767),clampf(counts.y,-32767,32767)};
}
static bool output_blocked();

std::string test_connection(){std::lock_guard<std::recursive_mutex> connection(connection_mutex);if(!inp.device_ok)return "No connected input device";bool keyboard_verified=inp.keyboard_ok;bool responded=ping_device();if(responded&&keyboard_verified&&inp.keyboard_api)inp.keyboard_ok=true;if(responded&&device_is_makcu&&!physical_monitor().run)start_mouse_monitor();return responded?"Serial output device responded; physical buttons are verified separately":"Device did not respond";}
std::string test_mouse(){if(!inp.device_ok||cfg.I("input.mode")==2)return "No connected output device";if(inp.panic)return "Mouse test paused by the panic key";
    if(cfg.I("input.latency_ms"))Sleep((DWORD)cfg.I("input.latency_ms"));if(!wr("km.move(4,0)\r\n"))return "Mouse test could not write to the device";return "Sent a small movement right (4 counts); confirm on game PC";
}

std::string test_click(){
    if(!inp.device_ok||cfg.I("input.mode")==2)return "Connect an input device first";
    if(inp.panic)return "Click test paused by the panic key";
    if(cfg.I("input.latency_ms"))Sleep((DWORD)cfg.I("input.latency_ms"));bool pressed=wr("km.left(1)\r\n");Sleep(35);bool released=wr("km.left(0)\r\n");if(!released)return "Mouse release could not be sent";return pressed?"Sent one left click to the game PC":"Could not send the mouse press";
}
bool init(){
    Settings::ReadScope input_settings(cfg,true);
    std::lock_guard<std::recursive_mutex> connection(connection_mutex);
    close_port();
    plat_stage("input","connecting serial device");    plat_log("Connecting input device...");
    int wanted=cfg.I("input.com");
    if(wanted>0){
        if(try_port(wanted)){
            inp.device_ok=true;
            inp.set_device_text(std::string("COM")+std::to_string(wanted)+" @ "+std::to_string(inp.baud_rate)+" baud (verified)");
            plat_stage("input",inp.device_status());plat_log("Input OK: %s",inp.device_status().c_str());start_mouse_monitor();connected_output();
            return true;
        }
        close_port();
        plat_log("COM%d set manually but no device answered - scanning",wanted);
    }
    std::vector<int> ports=enum_com_ports();
    plat_log("Found %d enumerated serial port(s) to try",ports.size());for(int port:ports)plat_log("Candidate input port: COM%d",port);
    for(int c:ports){
        if(c==wanted) continue;
        if(try_port(c)){
            inp.device_ok=true;
            inp.set_device_text(std::string("COM")+std::to_string(c)+" @ "+std::to_string(inp.baud_rate)+" baud (verified)");
            cfg.set_int_if_unchanged("input.com",c,{{"input.com",cfg.I("input.com")},{"input.baud",cfg.I("input.baud")},{"input.mode",cfg.I("input.mode")}},false);
            plat_stage("input",inp.device_status());plat_log("Input OK: %s",inp.device_status().c_str());start_mouse_monitor();connected_output();
            return true;
        }
        close_port();
    }
    if(ports.empty()){
        plat_log("No COM ports enumerated; probing COM1-COM32 directly, excluding known Bluetooth ports");
        for(int c=1;c<=32;c++)if(c!=wanted&&possible_unlisted_port(c)&&try_port(c)){
            inp.device_ok=true;inp.set_device_text(std::string("COM")+std::to_string(c)+" @ "+std::to_string(inp.baud_rate)+" baud (verified)");
            cfg.set_int_if_unchanged("input.com",c,{{"input.com",cfg.I("input.com")},{"input.baud",cfg.I("input.baud")},{"input.mode",cfg.I("input.mode")}},false);
            plat_stage("input",inp.device_status());plat_log("Input OK: %s",inp.device_status().c_str());start_mouse_monitor();connected_output();return true;
        }
    }
    inp.device_ok=false;inp.set_device_text(ports.empty()?"No serial COM ports found":"No COM device answered Makcu/KMBox probe");
    plat_stage("input",inp.device_status());plat_log("Input device not connected: %s",inp.device_status().c_str());
    return false;
}
void disconnect(){std::lock_guard<std::recursive_mutex> connection(connection_mutex);close_port();inp.device_ok=false;inp.set_device_text("Disconnected");plat_log("Input device disconnected by user");}
void reconnect(){
    std::lock_guard<std::recursive_mutex> connection(connection_mutex);
    close_port(); inp.device_ok=false;
    plat_log("Input device reconnecting...");
    if(init()&&inp.device_ok) toast("input reconnected");
    else toast("input reconnect FAILED",{1,0.3f,0.3f,1});
}
static bool output_blocked(){return !inp.device_ok||cfg.I("input.mode")==2||inp.panic||!game_data_permits_output();}
static bool send_button(int b,bool down){
    const char* name=b==0?"left":b==1?"right":b==2?"middle":b==4?"side1":"side2";
    return wr("km.%s(%d)\r\n",name,down?1:0);
}
static int mouse_resource(int button){return button>=0&&button<=2?button:button==4?3:button==5?4:-1;}
static int key_resource(int key){
    switch(key){case VK_LBUTTON:return 0;case VK_RBUTTON:return 1;case VK_MBUTTON:return 2;case VK_XBUTTON1:return 3;case VK_XBUTTON2:return 4;}
    int code=km_protocol::hid(key);return code>=0&&code<256?5+code:-1;
}
static bool send_resource(int resource,bool down){
    if(resource<0||resource>=(int)output_queue::resource_count)return false;
    if(resource<5)return send_button(resource<3?resource:resource+1,down);
    return wr(down?"km.down(%d)\r\n":"km.up(%d)\r\n",resource-5);
}
static output_queue::Context output_context(bool connected){
    Settings::ReadScope output_settings(cfg);
    return {now_ms(),mem.attachment_epoch(),connected&&!inp.panic&&inp.device_ok,!output_blocked()};
}
class AutomaticOutput {
    std::mutex mutex_;std::condition_variable wake_,drained_;std::thread worker_;
    output_queue::Queue queue_;std::array<output_queue::Held,output_queue::resource_count> held_{};
    std::array<bool,output_queue::resource_count> uncertain_{};
    std::array<unsigned,3> release_buttons_{};
    bool accepting_=false,connection_open_=false,recovery_pending_=false,stopping_=false,exited_=false;
    u64 barrier_=0,drained_barrier_=0,last_sent_=0,press_order_=0;
    std::atomic<u64> accepted_{0},coalesced_{0},rejected_{0},release_failed_{0};
    std::atomic<unsigned> uncertain_count_{0};std::atomic<bool> fault_{false};
    static void advance(u64& value){if(++value==0)++value;}
    std::size_t held_count()const{std::size_t count=0;for(const auto& held:held_)count+=held.active;return count;}
    void release(int resource){
        auto& held=held_[(size_t)resource];
        // Release writes bypass permission/activity checks. Failed delivery is
        // retried twice while the handle remains open, without a render wait.
        if(send_resource(resource,false)){held={};return;}
        ++release_failed_;
        if(++held.release_failures<3){held.release_ms=now_ms()+5;return;}
        plat_log("Automatic output: release delivery uncertain after three attempts, resource=%d; output disabled until reconnect clears owed releases",resource);
        if(!uncertain_[(size_t)resource]){uncertain_[(size_t)resource]=true;++uncertain_count_;}held={};fault_=true;
        std::lock_guard lock(mutex_);accepting_=false;queue_.cancel_all();
    }
    bool revalidate(const output_queue::Request& request){
        auto context=output_context(true);std::lock_guard lock(mutex_);context.connected=context.connected&&accepting_&&!stopping_;
        return queue_.valid(request,context);
    }
    bool press(int resource,const output_queue::Request& request,u64 duration){
        // Recheck after serial contention, immediately before the write. Queue
        // locking ends inside revalidate(); no queue mutex spans device I/O.
        std::lock_guard<std::recursive_mutex> serial_lock(serial_mutex);
        if(!revalidate(request))return false;
        advance(press_order_);auto& held=held_[(size_t)resource];held={true,request.origin,request.generation,request.attachment,0,press_order_,0};
        // Track before attempting a write: a partial/failed write may still
        // have pressed the control and therefore always earns an up attempt.
        bool sent=send_resource(resource,true);held.release_ms=!sent?now_ms():(duration?now_ms()+duration:0);return sent;
    }
    void dispatch(const output_queue::Request& request){
        if(!revalidate(request))return;
        if(request.kind==output_queue::Kind::Move){
            std::lock_guard<std::recursive_mutex> serial_lock(serial_mutex);if(!revalidate(request))return;
            wr("km.move(%d,%d)\r\n",request.first,request.second);last_sent_=now_ms();return;
        }
        const int first=request.first,second=request.second;
        if(first<0||first>=(int)held_.size()||held_[(size_t)first].active)return;
        const bool chord=request.kind==output_queue::Kind::Chord;
        if(chord&&(second<0||second>=(int)held_.size()||second==first||held_[(size_t)second].active))return;
        if(held_count()+(chord?2:1)>output_queue::max_held)return;
        if((first>=5||(chord&&second>=5))&&!inp.keyboard_ok)return;
        auto duration=request.kind==output_queue::Kind::Down||chord?u64(0):(u64)request.hold_ms;
        bool first_sent=press(first,request,duration),second_sent=false;
        // Cancellation/attachment replacement between the two down writes
        // suppresses the second press; the first remains tracked for release.
        if(chord&&first_sent)second_sent=press(second,request,0);
        if(chord){
            auto common_release=now_ms()+(u64)request.hold_ms;
            if(first_sent&&held_[(size_t)first].active)held_[(size_t)first].release_ms=common_release;
            if(second_sent&&held_[(size_t)second].active)held_[(size_t)second].release_ms=common_release;
        }
        last_sent_=now_ms();
    }
    void recover(){
        // A newly verified existing connection must clear every uncertain up
        // before admitting output. This runs on the worker, never the caller.
        for(std::size_t resource=0;resource<uncertain_.size();++resource)if(uncertain_[resource]){
            bool released=false;for(unsigned attempt=0;attempt<3&&!released;++attempt)released=send_resource((int)resource,false);
            if(released){uncertain_[resource]=false;--uncertain_count_;}else ++release_failed_;
        }
        fault_=uncertain_count_.load()!=0;
        {std::lock_guard lock(mutex_);accepting_=connection_open_&&!stopping_&&!fault_.load();}
        if(fault_)plat_log("Automatic output remains disabled: reconnect could not clear %u uncertain releases",uncertain_count_.load());
    }
    void loop(){
        for(;;){
            bool accepting=false,recovery=false;{std::lock_guard lock(mutex_);accepting=accepting_;recovery=recovery_pending_;recovery_pending_=false;}
            if(recovery){recover();continue;}
            auto context=output_context(accepting);
            std::array<bool,output_queue::resource_count> due{};std::array<unsigned,3> releases{};
            {
                std::lock_guard lock(mutex_);context.connected=context.connected&&accepting_&&!stopping_;
                queue_.expire(context.now);if(!context.connected||!context.permitted)queue_.cancel_all();
                releases=release_buttons_;release_buttons_={};
                for(std::size_t resource=0;resource<held_.size();++resource){
                    const auto& held=held_[resource];
                    bool requested=held.active&&resource<5&&(releases[output_queue::owner(held.origin)]&(1u<<resource));
                    due[resource]=requested||queue_.release_due(held,context);
                }
            }
            std::array<int,output_queue::max_held> release_order{};std::size_t release_count=0;
            for(std::size_t resource=0;resource<held_.size();++resource)if(due[resource])release_order[release_count++]=(int)resource;
            // A chord releases in reverse press order when both controls are due.
            std::sort(release_order.begin(),release_order.begin()+release_count,[&](int a,int b){return held_[(size_t)a].order>held_[(size_t)b].order;});
            for(std::size_t index=0;index<release_count;++index)release(release_order[index]);
            // Explicit up without tracked ownership is still delivered, unless
            // another owner currently holds that resource.
            for(unsigned owner=0;owner<3;++owner)for(unsigned resource=0;resource<5;++resource)
                if((releases[owner]&(1u<<resource))&&!due[resource]&&!held_[resource].active)send_resource((int)resource,false);
            output_queue::Request request;bool have=false,stop=false;
            {
                std::unique_lock lock(mutex_);
                if(!accepting_&&held_count()==0&&release_buttons_==std::array<unsigned,3>{}){
                    drained_barrier_=barrier_;drained_.notify_all();
                }
                stop=stopping_&&held_count()==0;
                context.connected=context.connected&&accepting_&&!stopping_;
                if(!stop)have=queue_.take(context,last_sent_,request);
                if(!stop&&!have)wake_.wait_for(lock,std::chrono::milliseconds(5));
            }
            if(stop)return;if(have)dispatch(request);
        }
    }
    void run()noexcept{
        try{loop();}
        catch(...){
            // Unexpected worker failure must not silently leave a press owned.
            for(std::size_t i=0;i<held_.size();++i)if(held_[i].active){bool released=false;try{released=send_resource((int)i,false);}catch(...){}
                if(!released&&!uncertain_[i]){uncertain_[i]=true;++uncertain_count_;}held_[i]={};}
            fault_=uncertain_count_.load()!=0;
            std::lock_guard lock(mutex_);accepting_=false;stopping_=true;queue_.cancel_all();release_buttons_={};
        }
        std::lock_guard lock(mutex_);exited_=true;drained_barrier_=barrier_;drained_.notify_all();
    }
public:
    ~AutomaticOutput(){shutdown();}
    void connected(){
        bool failed=false;
        {std::lock_guard lock(mutex_);if(stopping_)return;
            if(!worker_.joinable()){try{worker_=std::thread([this]{run();});}catch(...){failed=true;}}
            accepting_=false;connection_open_=!failed&&worker_.joinable();recovery_pending_=connection_open_;
        }
        wake_.notify_one();if(failed)plat_log("Automatic output worker could not start; queued output remains disabled");
    }
    bool enqueue(output_queue::Request request){
        auto context=output_context(true);Settings::ReadScope output_settings(cfg);
        // Bind to the image that caused this command. Never restamp an old
        // decision with a freshly replaced live attachment's identity.
        if(const auto* image=decision_frame::active()){
            context.attachment=image->binding_epoch;request.source_sample_ms=image->entity_sample_ms;
            if(image->managed&&!request.source_sample_ms){++rejected_;return false;}
        }
        unsigned latency=(unsigned)std::clamp(cfg.I("input.latency_ms"),0,50),spacing=(unsigned)std::clamp(cfg.I("input.delay_ms"),0,20);
        output_queue::Push result;
        {std::lock_guard lock(mutex_);context.connected=context.connected&&accepting_&&!stopping_;queue_.expire(context.now);result=queue_.push(request,context,latency,spacing);}
        if(result==output_queue::Push::Rejected){++rejected_;return false;}
        ++accepted_;if(result==output_queue::Push::Coalesced)++coalesced_;wake_.notify_one();return true;
    }
    void activity(hw::Origin origin,bool active){
        {std::lock_guard lock(mutex_);queue_.set_activity(origin,active,now_ms());}wake_.notify_one();
    }
    void cancel(hw::Origin origin){
        {std::lock_guard lock(mutex_);queue_.cancel(origin);}wake_.notify_one();
    }
    void cancel_all(){
        {std::lock_guard lock(mutex_);queue_.cancel_all();}wake_.notify_one();
    }
    void up(int resource,hw::Origin origin){
        if(resource<0||resource>=5||!output_queue::valid_owner(origin))return;
        {std::lock_guard lock(mutex_);if(!accepting_||stopping_)return;queue_.cancel_resource(origin,resource);release_buttons_[output_queue::owner(origin)]|=1u<<resource;}wake_.notify_one();
    }
    bool quiesce(){
        std::unique_lock lock(mutex_);accepting_=false;connection_open_=false;recovery_pending_=false;queue_.cancel_all();advance(barrier_);auto requested=barrier_;
        if(!worker_.joinable()||exited_){release_buttons_={};drained_barrier_=requested;return !fault_.load();}
        wake_.notify_one();drained_.wait(lock,[&]{return drained_barrier_>=requested;});
        return !fault_.load();
    }
    // Terminal host operation, after producers stop. No mutex/connection lock
    // is held while joining. Reconnect uses quiesce(), never joins this worker.
    void shutdown(){
        {std::lock_guard lock(mutex_);accepting_=false;connection_open_=false;recovery_pending_=false;stopping_=true;queue_.cancel_all();advance(barrier_);}
        wake_.notify_one();if(worker_.joinable())worker_.join();
    }
    std::string status()const{
        return "async accepted="+std::to_string(accepted_.load())+" coalesced="+std::to_string(coalesced_.load())+" rejected="+std::to_string(rejected_.load())+" release-write-failures="+std::to_string(release_failed_.load())+" uncertain-releases="+std::to_string(uncertain_count_.load())+" fault="+(fault_.load()?"yes":"no");
    }
};
static AutomaticOutput& automatic_output(){static AutomaticOutput value;return value;}
static bool queue_output(output_queue::Kind kind,hw::Origin origin,int first,int second,int hold){
    output_queue::Request request;request.kind=kind;request.origin=origin;request.first=first;request.second=second;request.hold_ms=std::clamp(hold,1,500);
    return automatic_output().enqueue(request);
}
void mouse_move(int dx,int dy,Origin origin){if(dx||dy)queue_output(output_queue::Kind::Move,origin,dx,dy,0);}
void mouse_down(int button,Origin origin){int resource=mouse_resource(button);if(resource>=0)queue_output(output_queue::Kind::Down,origin,resource,0,0);}
void mouse_up(int button,Origin origin){automatic_output().up(mouse_resource(button),origin);}
void mouse_click(int button,int ms,Origin origin){int resource=mouse_resource(button);if(resource>=0)queue_output(output_queue::Kind::Tap,origin,resource,0,ms);}
void key_tap(int key,int ms,Origin origin){
    if(!Settings::action_output_key_allowed(key)||!can_send(key))return;int resource=key_resource(key);
    if(resource>=0)queue_output(output_queue::Kind::Tap,origin,resource,0,ms);
}
bool key_chord(int modifier,int key,int ms,Origin origin){
    if(!Settings::action_output_key_allowed(modifier)||!Settings::action_output_key_allowed(key)||!can_send(modifier)||!can_send(key))return false;
    int first=key_resource(modifier),second=key_resource(key);if(first<0||second<0||first==second)return false;
    return queue_output(output_queue::Kind::Chord,origin,first,second,ms);
}
void set_activity(Origin origin,bool active){automatic_output().activity(origin,active);}
void set_activity(bool aim_active,bool trigger_active){set_activity(Origin::Aim,aim_active);set_activity(Origin::Trigger,trigger_active);}
void cancel(Origin origin){automatic_output().cancel(origin);}
void cancel_all(){automatic_output().cancel_all();}
void shutdown_output(){automatic_output().shutdown();}
std::string automatic_output_status(){return automatic_output().status();}
}
// Connection code never waits while holding serial_mutex. The output worker
// never acquires connection_mutex, so reconnect can quiesce it under that lock.
static void quiesce_output(){if(!hw::automatic_output().quiesce())plat_log("Automatic output closure: release attempts completed with uncertain delivery; sticky fault retained for reconnect");}
static void connected_output(){hw::automatic_output().connected();}

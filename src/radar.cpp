#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <map>
#include "radar.h"
#include "entities.h"
#include "config.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <string>
namespace radar{
static std::atomic<bool> run_flag{false},alive{false};
static std::atomic<int> error_code{0};
static std::thread server;
static int bound_port=0;static bool bound_loopback=true;
int current_port(){return bound_port;}
bool loopback_only(){return bound_loopback;}
static std::string pin;
const char* current_pin(){return pin.c_str();}
bool running(){return alive;}
std::string status_text(){int e=error_code;return e?"Radar failed (socket error "+std::to_string(e)+")":(alive?"Radar running":(run_flag?"Starting radar...":"Radar stopped"));}
void stop(){run_flag=false;if(server.joinable())server.join();alive=false;}

static bool send_all(SOCKET socket,const std::string& bytes){
    size_t offset=0;const auto deadline=now_ms()+4000;
    while(offset<bytes.size()&&run_flag&&now_ms()<deadline){
        const int count=send(socket,bytes.data()+offset,(int)std::min<size_t>(65536,bytes.size()-offset),0);
        if(count>0){offset+=count;continue;}
        if(count==0)return false;
        const int error=WSAGetLastError();
        if(error!=WSAEWOULDBLOCK&&error!=WSAEINTR)return false;
        fd_set writable;FD_ZERO(&writable);FD_SET(socket,&writable);timeval timeout{0,50000};
        if(select(0,nullptr,&writable,nullptr,&timeout)==SOCKET_ERROR&&WSAGetLastError()!=WSAEINTR)return false;
    }
    return offset==bytes.size();
}
static bool send_response(SOCKET socket,const char* status,const char* type,const std::string& body){
    std::string response=std::string("HTTP/1.1 ")+status+"\r\nContent-Type: "+type+"\r\nContent-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nX-Content-Type-Options: nosniff\r\n\r\n"+body;
    return send_all(socket,response);
}
static bool receive_headers(SOCKET socket,std::string& request){
    const auto deadline=now_ms()+1000;char chunk[2048];
    while(run_flag&&now_ms()<deadline){
        int count=recv(socket,chunk,sizeof(chunk),0);
        if(count>0){if(request.size()+(size_t)count>8192)return false;request.append(chunk,count);if(request.find("\r\n\r\n")!=std::string::npos)return true;continue;}
        if(count==0)return false;int error=WSAGetLastError();if(error!=WSAEWOULDBLOCK&&error!=WSAEINTR)return false;
        fd_set readable;FD_ZERO(&readable);FD_SET(socket,&readable);timeval timeout{0,50000};
        if(select(0,&readable,nullptr,nullptr,&timeout)==SOCKET_ERROR&&WSAGetLastError()!=WSAEINTR)return false;
    }
    return false;
}
struct MapAssets{json document={{"version",1},{"maps",json::object()}};std::map<std::string,std::string> png;};
// Presentation-only copy of already-published snapshot fields; no target reads.
struct RadarXY{
    float x=0,y=0,hp=0,max_hp=0,ultimate_end=0,ultimate_stopped=0;
    u64 id=0,source_ms=0,ultimate_ms=0;u32 hero=0;const char* team="";
    bool hp_valid=false,max_hp_valid=false,hp_pair_valid=false;
    bool ultimate_valid=false,ultimate_trained=false;
};
struct RadarReadout{
    bool attached=false,frame_valid=false;u64 sample_ms=0,session_epoch=0;
    float game_time=0,reader_hz=0;std::string map;std::vector<RadarXY> points;
};
static RadarReadout capture_radar(){
    RadarReadout result;result.points.reserve(32);
    {
        std::lock_guard<std::mutex> lock(snap_mtx);
        result.attached=snap.in_match.load();result.frame_valid=snap.frame_valid.load();
        result.sample_ms=snap.last_read_ms.load();result.session_epoch=snap.native_binding_epoch.load();
        result.map=snap.map_name;result.game_time=snap.game_time;result.reader_hz=snap.read_hz_exact.load();
        auto add=[&](const Entity& e,const char* team){
            if(!e.base||!e.valid||!std::isfinite(e.pos.x)||!std::isfinite(e.pos.y))return;
            RadarXY point;point.x=e.pos.x;point.y=e.pos.y;point.id=e.base;point.team=team;
            point.source_ms=e.host_sample_ms;point.hero=e.hero;
            point.hp=e.hp;point.max_hp=e.max_hp;point.hp_valid=e.health_current_valid;
            point.max_hp_valid=e.health_max_valid;point.hp_pair_valid=e.health_pair_valid;
            point.ultimate_valid=e.public_ultimate_valid;point.ultimate_trained=e.public_ultimate_trained;
            point.ultimate_end=e.public_ultimate_cooldown_end;point.ultimate_stopped=e.public_ultimate_stopped_remaining;
            point.ultimate_ms=e.public_ultimate_sample_ms;result.points.push_back(point);
        };
        for(const auto& e:snap.enemies)add(e,"enemy");
        for(const auto& e:snap.friendlies)add(e,"friendly");add(snap.local,"local");
    }
    return result;
}
static json radar_age(u64 source_ms,u64 served_ms){return source_ms&&served_ms>=source_ms?json(served_ms-source_ms):json(nullptr);}
static bool radar_source_fresh(u64 source_ms,u64 served_ms){return source_ms&&served_ms>=source_ms&&served_ms-source_ms<=250;}
static json radar_entity(const RadarXY& point,const RadarReadout& readout,u64 served_ms){
    const char* name=hero_name_by_id(point.hero);
    const bool known_hero=point.hero&&name&&name[0]&&std::string(name)!="Unknown";
    const bool hp_known=point.hp_valid&&std::isfinite(point.hp);
    const bool max_hp_known=point.max_hp_valid&&std::isfinite(point.max_hp)&&point.max_hp>0;
    const bool pair_known=point.hp_pair_valid&&hp_known&&max_hp_known;
    const bool ultimate_known=point.ultimate_valid&&radar_source_fresh(point.ultimate_ms,served_ms)&&std::isfinite(readout.game_time)&&std::isfinite(point.ultimate_end)&&std::isfinite(point.ultimate_stopped);
    json ultimate={{"valid",ultimate_known},{"source_ms",point.ultimate_ms},{"age_ms",radar_age(point.ultimate_ms,served_ms)},{"trained",nullptr},{"ready",nullptr},{"remaining_seconds",nullptr}};
    if(ultimate_known){
        ultimate["trained"]=point.ultimate_trained;
        if(point.ultimate_trained){
            const float remaining=std::max(point.ultimate_stopped,point.ultimate_end-readout.game_time);
            ultimate["ready"]=remaining<=0;
            // Sampled public cooldown only. A browser clock must not run down paused time.
            ultimate["remaining_seconds"]=std::clamp(remaining,0.f,36000.f);
        }
    }
    return {{"x",point.x},{"y",point.y},{"id",std::to_string(point.id)},{"team",point.team},
        {"source_ms",point.source_ms},{"source_age_ms",radar_age(point.source_ms,served_ms)},
        {"hero_id",point.hero},{"hero_name",known_hero?json(name):json(nullptr)},
        {"hp",hp_known?json(point.hp):json(nullptr)},{"max_hp",max_hp_known?json(point.max_hp):json(nullptr)},
        {"hp_pair_valid",pair_known},{"ultimate",std::move(ultimate)}};
}
static bool safe_name(const std::string& name){return !name.empty()&&name.size()<=48&&std::all_of(name.begin(),name.end(),[](unsigned char c){return(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
static bool read_bounded(const std::filesystem::path& path,size_t maximum,std::string& bytes){
    std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)return false;const auto size=input.tellg();if(size<=0||(uint64_t)size>maximum)return false;bytes.resize((size_t)size);input.seekg(0);return(bool)input.read(bytes.data(),(std::streamsize)bytes.size());
}
static MapAssets load_map_assets(){
    MapAssets assets;wchar_t path[4096]{};const DWORD count=GetModuleFileNameW(nullptr,path,4096);if(!count||count>=4096)return assets;
    const auto directory=std::filesystem::path(path).parent_path()/"assets"/"radar";std::string text;
    if(!read_bounded(directory/"maps.json",65536,text)){plat_log("Radar: map-image configuration unavailable; custom browser calibration remains available");return assets;}
    try{
        const auto config=json::parse(text);if(config.value("version",0)!=1||!config.contains("maps")||!config["maps"].is_object()||config["maps"].size()>16)return assets;
        for(auto it=config["maps"].begin();it!=config["maps"].end();++it){
            try{
                const auto id=it.key();const auto& row=it.value();if(!safe_name(id)||!row.is_object())continue;
                const std::string filename=row.value("image","");if(filename.size()<5||!filename.ends_with(".png")||!safe_name(filename.substr(0,filename.size()-4)))continue;
                auto transform=row.at("transform");bool valid=transform.is_object();
                for(auto key:{"origin_x","origin_y","world_width","world_height","rotation_degrees","offset_u","offset_v"})valid=valid&&transform.contains(key)&&transform[key].is_number()&&std::isfinite(transform[key].get<double>());
                if(!valid)continue;
                if(std::abs(transform["origin_x"].get<double>())>1e8||std::abs(transform["origin_y"].get<double>())>1e8||transform["world_width"].get<double>()<=0||transform["world_width"].get<double>()>1e8||transform["world_height"].get<double>()<=0||transform["world_height"].get<double>()>1e8||std::abs(transform["rotation_degrees"].get<double>())>3600||std::abs(transform["offset_u"].get<double>())>10||std::abs(transform["offset_v"].get<double>())>10)continue;
                if(!transform.contains("flip_x")||!transform["flip_x"].is_boolean()||!transform.contains("flip_y")||!transform["flip_y"].is_boolean())continue;
                std::string image;if(!read_bounded(directory/filename,4*1024*1024,image)||image.size()<24||std::memcmp(image.data(),"\x89PNG\r\n\x1a\n",8)||std::memcmp(image.data()+12,"IHDR",4))continue;
                auto dimension=[&](size_t at){return(uint32_t)(u8)image[at]<<24|(uint32_t)(u8)image[at+1]<<16|(uint32_t)(u8)image[at+2]<<8|(u8)image[at+3];};
                const auto width=dimension(16),height=dimension(20);if(!width||!height||width>4096||height>4096)continue;
                const auto label=row.value("label",id),provenance=row.value("provenance",std::string()),key=row.value("calibration_key",id);
                if(label.size()>160||provenance.size()>1024||key.size()>128)continue;
                assets.document["maps"][id]={{"label",label},{"transform",transform},{"calibration_key",key},{"provenance",provenance}};
                assets.png.emplace(id,std::move(image));plat_log("Radar: loaded map image %s (%ux%u); world transform comes from maps.json, live alignment not verified",id.c_str(),width,height);
            }catch(const std::exception&){plat_log("Radar: rejected invalid map-image profile");}
        }
    }catch(const std::exception&){plat_log("Radar: invalid map-image configuration");}
    return assets;
}
static bool authorized(const std::string& path,const std::string& expected){
    auto q=path.find('?');if(q==std::string::npos)return false;
    std::string query=path.substr(q+1);size_t start=0;
    while(start<query.size()){auto end=query.find('&',start);auto part=query.substr(start,end==std::string::npos?end:end-start);
        if(part=="pin="+expected)return true;if(end==std::string::npos)break;start=end+1;}
    return false;
}
void start(bool loopback){
    stop();error_code=0;pin.clear();for(int i=0;i<6;i++)pin.push_back('0'+(int)rnd_rng(0,9.999f));
    int port=cfg.I("radar.port");bound_port=port;bound_loopback=loopback;std::string access_pin=pin;run_flag=true;
    server=std::thread([port,loopback,access_pin](){
        const MapAssets map_assets=load_map_assets();
        WSADATA w{};int result=WSAStartup(MAKEWORD(2,2),&w);
        if(result){error_code=result;run_flag=false;return;}
        SOCKET listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(listener==INVALID_SOCKET){error_code=WSAGetLastError();run_flag=false;WSACleanup();return;}
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons((u_short)port);
        address.sin_addr.s_addr=htonl(loopback?INADDR_LOOPBACK:INADDR_ANY);
        if(bind(listener,(sockaddr*)&address,sizeof(address))||listen(listener,4)){
            error_code=WSAGetLastError();run_flag=false;closesocket(listener);WSACleanup();return;
        }
        u_long nonblocking=1;if(ioctlsocket(listener,FIONBIO,&nonblocking)){error_code=WSAGetLastError();run_flag=false;closesocket(listener);WSACleanup();return;}alive=true;
        while(run_flag){
            fd_set readable;FD_ZERO(&readable);FD_SET(listener,&readable);timeval timeout{0,50000};
            const int ready=select(0,&readable,nullptr,nullptr,&timeout);
            if(!ready)continue;
            if(ready==SOCKET_ERROR){const int error=WSAGetLastError();if(error==WSAEINTR)continue;error_code=error;run_flag=false;break;}
            SOCKET client=accept(listener,nullptr,nullptr);
            if(client==INVALID_SOCKET){const int error=WSAGetLastError();if(error==WSAEWOULDBLOCK||error==WSAEINTR)continue;error_code=error;run_flag=false;break;}
            if(ioctlsocket(client,FIONBIO,&nonblocking)){closesocket(client);continue;}
            std::string request;if(!receive_headers(client,request)){closesocket(client);continue;}
            const auto line_end=request.find("\r\n"),a=request.find(' '),b=a==std::string::npos?a:request.find(' ',a+1);
            if(a==std::string::npos||b==std::string::npos||b>=line_end||request.substr(0,a)!="GET"){send_response(client,"405 Method Not Allowed","text/plain","GET required");closesocket(client);continue;}
            const std::string path=request.substr(a+1,b-a-1);
            auto q=path.find('?');auto endpoint=path.substr(0,q);
            // Apply the same PIN gate before every route, including the HTML page.
            if(!authorized(path,access_pin)){
                send_response(client,"403 Forbidden","application/json","{\"error\":\"Invalid PIN\"}");
                closesocket(client);continue;
            }
            if(endpoint=="/maps"||endpoint.starts_with("/map-image/")){
                if(!authorized(path,access_pin))send_response(client,"403 Forbidden","application/json","{\"error\":\"Invalid PIN\"}");
                else if(endpoint=="/maps")send_response(client,"200 OK","application/json",map_assets.document.dump());
                else{
                    const std::string id=endpoint.size()>15&&endpoint.ends_with(".png")?endpoint.substr(11,endpoint.size()-15):std::string();
                    const auto image=map_assets.png.find(id);
                    if(image==map_assets.png.end())send_response(client,"404 Not Found","text/plain","Map image not configured");
                    else send_response(client,"200 OK","image/png",image->second);
                }
            }else if(endpoint=="/data"){
                if(!authorized(path,access_pin))send_response(client,"403 Forbidden","application/json","{\"error\":\"Invalid PIN\"}");
                else{
                    const RadarReadout readout=capture_radar();
                    const auto served_ms=now_ms();const bool age_known=readout.sample_ms&&served_ms>=readout.sample_ms;
                    const bool fresh=readout.attached&&readout.frame_valid&&radar_source_fresh(readout.sample_ms,served_ms);
                    // JSON construction and serialization run after releasing snap_mtx.
                    json data={{"ents",json::array()},{"attached",readout.attached},{"frame_valid",readout.frame_valid},{"valid",fresh},{"map_name",readout.map},
                        {"snapshot_ms",readout.sample_ms},{"server_time_ms",served_ms},{"age_ms",age_known?radar_age(readout.sample_ms,served_ms):json(nullptr)},
                        {"session_epoch",std::to_string(readout.session_epoch)},
                        {"reader_hz",std::isfinite(readout.reader_hz)&&readout.reader_hz>0?json(readout.reader_hz):json(nullptr)}};
                    if(fresh)for(const auto& point:readout.points){
                        // Preserve original actor acquisition; a newer frame never freshens a held actor.
                        if(radar_source_fresh(point.source_ms,served_ms))data["ents"].push_back(radar_entity(point,readout,served_ms));
                    }
                    send_response(client,"200 OK","application/json",data.dump());
                }
            }else if(endpoint=="/"){
                const char* html=R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AceSoftware Radar</title>
<style>
:root{color-scheme:light;--ink:#19343f;--muted:#526970;--line:#afc1bc;--paper:#f7faf6;--accent:#226c69}*{box-sizing:border-box}html,body{width:100%;height:100%;overflow:hidden}body{margin:0;background:#d9e5df;color:var(--ink);font:14px system-ui,sans-serif}button,input,select{font:inherit;color:inherit}button,select,input[type=number]{border:1px solid var(--line);border-radius:7px;background:var(--paper);padding:7px 10px}button{cursor:pointer}button:hover{background:#e3eeea}button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid #458bba;outline-offset:2px}main{height:100vh;height:100dvh;display:grid;grid-template-rows:auto auto minmax(0,1fr);min-width:0}header{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:10px 16px;background:var(--paper);border-bottom:1px solid var(--line)}h1{font-size:18px;margin:0;white-space:nowrap}h1 span{font-weight:400;color:var(--muted)}.tools{display:flex;align-items:center;gap:8px;flex-wrap:wrap}.tools select{max-width:200px}.statusbar{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:7px 16px;font-size:12px;border-bottom:1px solid var(--line);background:#eaf1ec}.statusbar p{margin:0}.legend{display:flex;gap:12px;white-space:nowrap}.legend span:before{content:'●';margin-right:5px}.you:before{color:#6442ad}.friend:before{color:#087558}.enemy:before{color:#bb2948}#viewport{position:relative;min-height:0;overflow:hidden;background:#d5e1db}#viewport:fullscreen{width:100vw;height:100vh}canvas{display:block;width:100%;height:100%;touch-action:none;cursor:grab}canvas.dragging{cursor:grabbing}.map-tools{position:absolute;top:12px;left:12px;display:flex;gap:6px;align-items:center;background:#f7faf6ed;border:1px solid var(--line);border-radius:9px;padding:5px;box-shadow:0 2px 10px #25463b18}.map-tools button{min-width:34px;padding:5px 8px}#zoomValue{font-size:12px;min-width:44px;text-align:center}#notice{position:absolute;left:12px;bottom:12px;max-width:min(680px,calc(100% - 24px));margin:0;background:#f7faf6ed;border:1px solid var(--line);border-radius:8px;padding:7px 10px;color:#405951;font-size:12px;pointer-events:none}#drawer{position:absolute;z-index:3;inset:0 0 0 auto;width:min(420px,100%);background:#f7faf6f7;border-left:1px solid var(--line);box-shadow:-6px 0 24px #25463b25;overflow-y:auto;overscroll-behavior:contain;padding:16px}#drawer[hidden]{display:none}.drawer-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}.drawer-head h2{font-size:17px;margin:0}fieldset,details{border:1px solid var(--line);border-radius:9px;padding:12px;margin:0 0 14px}legend,summary{font-weight:650}summary{cursor:pointer}.checks{display:flex;gap:10px 16px;flex-wrap:wrap}.checks label{display:flex;align-items:center;gap:5px}.row{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:8px 0}.small{font-size:12px;color:var(--muted);line-height:1.5}.fields{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:12px 0}.fields label{display:flex;flex-direction:column;gap:4px;font-size:12px}.fields input{width:100%;min-width:0}.actions{display:flex;flex-wrap:wrap;gap:8px;margin:12px 0}#heroFilter{max-height:160px;overflow:auto;margin-top:8px;display:grid;gap:6px}#heroFilter label{display:flex;align-items:center;gap:7px}#source{overflow-wrap:anywhere}#rateNote{margin-bottom:0}input[type=range]{width:140px}#image{display:none}.kbd{font-weight:600}@media(max-width:640px){header{padding:8px 10px;gap:8px}h1{font-size:15px}.tools{gap:5px}.tools select{max-width:145px}.tools button{padding:7px}.statusbar{padding:6px 10px;align-items:flex-start}.legend{gap:7px;font-size:11px}.legend span{font-size:0}.legend span:before{font-size:14px}.fields{grid-template-columns:1fr 1fr}#status{max-width:calc(100% - 65px)}}
</style></head><body><main>
<header><h1>AceSoftware <span>Radar</span></h1><div class="tools"><select id="map" aria-label="Map"><option value="">Automatic map</option></select><button id="openSettings" aria-controls="drawer" aria-expanded="false">Settings &amp; calibration</button></div></header>
<div class="statusbar"><p id="status" role="status">Connecting…</p><div class="legend"><span class="you">You</span><span class="friend">Teammates</span><span class="enemy">Enemies</span></div></div>
<section id="viewport" aria-label="Radar map area">
<canvas id="radar" width="800" height="800" tabindex="0" aria-label="Player positions on map. Wheel to zoom; drag to pan; plus and minus zoom; zero fits map."></canvas>
<div class="map-tools" aria-label="Map view"><button id="zoomOut" aria-label="Zoom out">−</button><output id="zoomValue">100%</output><button id="zoomIn" aria-label="Zoom in">+</button><button id="fit">Fit</button><button id="fullscreen">Fullscreen</button><button id="openMapSettings" aria-controls="drawer" aria-expanded="false">Settings</button></div>
<p id="notice">Wheel to zoom · drag to pan</p>
<aside id="drawer" aria-label="Radar settings and calibration" hidden>
<div class="drawer-head"><h2>Radar settings</h2><button id="closeSettings" aria-label="Close settings">Close</button></div>
<fieldset><legend>Display</legend><div class="checks"><label><input id="showHero" type="checkbox" checked> Hero names</label><label><input id="showHp" type="checkbox" checked> HP</label><label><input id="showUlt" type="checkbox"> Ultimate</label></div><div class="checks" style="margin-top:12px"><label><input id="showLocal" type="checkbox" checked> You</label><label><input id="showFriendly" type="checkbox" checked> Teammates</label><label><input id="showEnemy" type="checkbox" checked> Enemies</label></div><label class="row"><span>Light map</span><input id="lightMap" type="checkbox" checked></label><label class="row"><span>Marker size</span><input id="markerSize" type="range" min="4" max="12" value="6"></label><p class="small">Unknown HP or ultimate data shows “?”. Ultimate seconds are sampled from the current snapshot; they expire with the source and do not run on the browser clock.</p></fieldset>
<fieldset><legend>Hero filter</legend><label><input id="allHeroes" type="checkbox" checked> All heroes, including unknown</label><div id="heroFilter"></div><p id="heroNote" class="small">Known heroes from the current snapshot appear here. Multiple heroes can be selected.</p></fieldset>
<fieldset><legend>Updates</legend><label class="row"><span>Web request target</span><select id="rate" aria-label="Requested web update rate"><option value="5">5 Hz</option><option value="10">10 Hz</option><option value="20" selected>20 Hz</option><option value="30">30 Hz</option><option value="60">60 Hz</option></select></label><p id="rates" class="small">Web replies: unknown · reader: unknown</p><p id="rateNote" class="small">The web target controls browser requests. Reader Hz is the host’s measured snapshot loop rate, not the web rate. Invalid sources and data older than 250 ms are hidden; reader stalls remain visible as stale data.</p></fieldset>
<details id="calibration"><summary>Map calibration</summary><p class="small">World calibration places coordinates on the image. View zoom and pan do not change these values. Changes are saved in this browser for this map.</p>
<div class="fields"><label>World center X<input id="origin_x" type="number" step="1"></label><label>World center Y<input id="origin_y" type="number" step="1"></label><label>World width<input id="world_width" type="number" min="1" step="1"></label><label>World height<input id="world_height" type="number" min="1" step="1"></label><label>Rotation (degrees)<input id="rotation_degrees" type="number" min="-3600" max="3600" step="1"></label><label>Image center X (0–1)<input id="offset_u" type="number" step="0.001"></label><label>Image center Y (0–1)<input id="offset_v" type="number" step="0.001"></label></div>
<div class="checks"><label><input id="flip_x" type="checkbox"> Flip X</label><label><input id="flip_y" type="checkbox"> Flip Y</label></div>
<div class="actions"><button id="reset">Reset calibration</button><button id="export">Export calibration</button><input id="image" type="file" accept="image/png,image/jpeg,image/webp"><button id="chooseImage" type="button">Use another map image</button></div>
<p id="calibrationError" class="small" role="status"></p><p id="local" class="small">Your world position is unavailable.</p><p id="source" class="small"></p><p class="small">Custom images stay in this browser session. Unlisted maps need their own image and calibration.</p></details>
</aside></section></main><script>
'use strict';
const pin=new URLSearchParams(location.search).get('pin')||'';
const canvas=document.getElementById('radar'),ctx=canvas.getContext('2d'),picker=document.getElementById('map'),viewport=document.getElementById('viewport');
const STALE_MS=250,REQUEST_TIMEOUT_MS=750,allowedRates=[5,10,20,30,60],ratePicker=document.getElementById('rate');
const numericKeys=['origin_x','origin_y','world_width','world_height','rotation_degrees','offset_u','offset_v'];
const defaultTransform={origin_x:0,origin_y:0,world_width:21504,world_height:21504,rotation_degrees:0,flip_x:false,flip_y:true,offset_u:0.5,offset_v:0.5};
let profiles={},lastData={ents:[],attached:false,frame_valid:false,valid:false,map_name:'',age_ms:null},activeId='',profile=null,transform=null,mapImage=null,imageReady=false,imageError='',customUrl='',busy=false;
let refreshHz=20,pollTimer=null,freshnessTimer=null,receivedAt=null,roundTripMs=0,lastNetworkError='',stopped=false,responseTimes=[],sessionEpoch=null;
let view={zoom:1,panX:0,panY:0},drag=null,selectedHeroes=new Set(),allHeroes=true;
const options={showHero:true,showHp:true,showUlt:false,showLocal:true,showFriendly:true,showEnemy:true,lightMap:true,markerSize:6};
try{const saved=Number(localStorage.getItem('aceRadarRefreshHz'));if(allowedRates.includes(saved))refreshHz=saved;}catch{}
ratePicker.value=String(refreshHz);
function elapsedSinceReceipt(){return Number.isFinite(receivedAt)?Math.max(0,performance.now()-receivedAt):Infinity;}
function sourceAge(age){return Number.isFinite(age)&&age>=0?age+roundTripMs+elapsedSinceReceipt():Infinity;}
function currentAge(){return sourceAge(lastData.age_ms);}
function markersFresh(){return lastData.attached===true&&lastData.frame_valid===true&&lastData.valid===true&&currentAge()<=STALE_MS;}
function entityFresh(e){return markersFresh()&&sourceAge(e.source_age_ms)<=STALE_MS;}
function ultimateFresh(e){return entityFresh(e)&&e.ultimate?.valid===true&&sourceAge(e.ultimate.age_ms)<=STALE_MS;}
function measuredWebHz(now){const recent=responseTimes.filter(t=>now-t<=2000);return recent.length>=2?(recent.length-1)*1000/(recent[recent.length-1]-recent[0]):null;}
function rateText(){const web=measuredWebHz(performance.now()),reader=markersFresh()&&Number.isFinite(lastData.reader_hz)&&lastData.reader_hz>0?lastData.reader_hz:null;return 'Web '+(Number.isFinite(web)?web.toFixed(1)+' replies/s':'replies unknown')+' (target '+refreshHz+' Hz) · reader '+(reader===null?'unknown':reader.toFixed(1)+' Hz measured');}
function updateStatus(){const age=currentAge(),ageText=Number.isFinite(age)?'source age ≤'+Math.ceil(age)+' ms':'source age unknown';let text=lastNetworkError||(!lastData.attached?'Waiting for live game data':!lastData.frame_valid?'Invalid snapshot · markers hidden':!markersFresh()?'Stale snapshot · markers hidden':lastData.ents.filter(entityFresh).length+' current entities');document.getElementById('status').textContent=text+' · '+ageText+' · '+rateText();document.getElementById('rates').textContent=rateText();}
function validTransform(t){return !!t&&numericKeys.every(k=>typeof t[k]==='number'&&Number.isFinite(t[k]))&&Math.abs(t.origin_x)<=1e8&&Math.abs(t.origin_y)<=1e8&&t.world_width>0&&t.world_width<=1e8&&t.world_height>0&&t.world_height<=1e8&&Math.abs(t.rotation_degrees)<=3600&&Math.abs(t.offset_u)<=10&&Math.abs(t.offset_v)<=10&&typeof t.flip_x==='boolean'&&typeof t.flip_y==='boolean';}
function worldToMap(x,y,t){const a=t.rotation_degrees*Math.PI/180,c=Math.cos(a),s=Math.sin(a),dx=x-t.origin_x,dy=y-t.origin_y,rx=c*dx-s*dy,ry=s*dx+c*dy;return {u:t.offset_u+(t.flip_x?-rx:rx)/t.world_width,v:t.offset_v+(t.flip_y?-ry:ry)/t.world_height};}
function fitMapRect(width,height,imageWidth,imageHeight){const scale=Math.min(width/imageWidth,height/imageHeight);return {width:imageWidth*scale,height:imageHeight*scale,cx:width/2,cy:height/2};}
function mapToScreen(u,v,rect,state){return {x:rect.cx+state.panX+(u-.5)*rect.width*state.zoom,y:rect.cy+state.panY+(v-.5)*rect.height*state.zoom};}
function screenToMap(x,y,rect,state){return {u:.5+(x-rect.cx-state.panX)/(rect.width*state.zoom),v:.5+(y-rect.cy-state.panY)/(rect.height*state.zoom)};}
function zoomViewAt(state,next,x,y,cx,cy){const zoom=Math.max(1,Math.min(8,next)),ratio=zoom/state.zoom;return {zoom,panX:x-cx-(x-cx-state.panX)*ratio,panY:y-cy-(y-cy-state.panY)*ratio};}
function storageKey(){return 'aceRadarCalibration:'+activeId+':'+(profile?.calibration_key||'custom');}
function resetView(){view={zoom:1,panX:0,panY:0};render();}
function setFields(){for(const k of numericKeys){const input=document.getElementById(k);input.value=transform?transform[k]:'';input.disabled=!transform;}for(const k of ['flip_x','flip_y']){const input=document.getElementById(k);input.checked=!!transform?.[k];input.disabled=!transform;}document.getElementById('source').textContent=profile?.provenance||'Choose a map image to calibrate.';document.getElementById('calibrationError').textContent='';}
function loadImage(url){imageReady=false;imageError='';const image=new Image();mapImage=image;image.onload=()=>{if(mapImage!==image)return;if(!image.naturalWidth||!image.naturalHeight||image.naturalWidth>4096||image.naturalHeight>4096){imageError='Map image must be between 1 and 4096 pixels per side.';render();return;}imageReady=true;render();};image.onerror=()=>{if(mapImage!==image)return;imageError='Map image unavailable. Check the radar image files or PIN.';render();};image.referrerPolicy='no-referrer';image.src=url;}
function chooseMap(){const wanted=picker.value||String(lastData.map_name||'').toLowerCase();if(wanted===activeId)return;activeId=wanted;profile=profiles[wanted]||null;mapImage=null;imageReady=false;imageError='';view={zoom:1,panX:0,panY:0};if(!profile){transform=null;setFields();render();return;}transform={...profile.transform};try{const saved=JSON.parse(localStorage.getItem(storageKey()));if(validTransform(saved))transform=saved;}catch{}setFields();loadImage(wanted==='custom'&&customUrl?customUrl:'/map-image/'+encodeURIComponent(wanted)+'.png?pin='+encodeURIComponent(pin));}
function readFields(){if(!transform)return;const candidate={...transform};for(const k of numericKeys){const input=document.getElementById(k);candidate[k]=input.value.trim()===''?NaN:Number(input.value);}for(const k of ['flip_x','flip_y'])candidate[k]=document.getElementById(k).checked;if(!validTransform(candidate)){document.getElementById('calibrationError').textContent='Enter finite calibration values and a positive width and height.';return;}transform=candidate;document.getElementById('calibrationError').textContent='';try{localStorage.setItem(storageKey(),JSON.stringify(transform));}catch{}render();}
function heroKnown(e){return Number.isInteger(e.hero_id)&&e.hero_id>0&&typeof e.hero_name==='string'&&e.hero_name.length>0;}
function includeEntity(e){return entityFresh(e)&&(e.team==='local'?options.showLocal:e.team==='friendly'?options.showFriendly:e.team==='enemy'?options.showEnemy:false)&&(allHeroes||(heroKnown(e)&&selectedHeroes.has(e.hero_id)));}
function healthText(e){if(!Number.isFinite(e.hp))return 'HP ?';return Math.round(e.hp)+(e.hp_pair_valid===true&&Number.isFinite(e.max_hp)&&e.max_hp>0?'/'+Math.round(e.max_hp):' HP');}
function ultimateText(e){if(!ultimateFresh(e))return 'Ult ?';const u=e.ultimate;if(u.trained===false)return 'Ult untrained';if(u.trained!==true||typeof u.ready!=='boolean')return 'Ult ?';if(u.ready)return 'Ult ready';return Number.isFinite(u.remaining_seconds)&&u.remaining_seconds>0?'Ult '+Math.ceil(u.remaining_seconds)+'s':'Ult ?';}
function entityLabel(e){const parts=[];if(options.showHero)parts.push(heroKnown(e)?e.hero_name:'Unknown hero');if(options.showHp)parts.push(healthText(e));if(options.showUlt)parts.push(ultimateText(e));return parts.join(' · ');}
function refreshHeroFilter(){const container=document.getElementById('heroFilter'),heroes=new Map();for(const e of lastData.ents)if(entityFresh(e)&&heroKnown(e))heroes.set(e.hero_id,e.hero_name);const key=String(sessionEpoch)+':'+allHeroes+':'+[...heroes].sort((a,b)=>a[0]-b[0]).map(([id,name])=>id+':'+name).join('|');if(container.dataset.key===key)return;container.dataset.key=key;container.replaceChildren();for(const [id,name]of [...heroes].sort((a,b)=>a[1].localeCompare(b[1]))){const label=document.createElement('label'),input=document.createElement('input');input.type='checkbox';input.checked=allHeroes||selectedHeroes.has(id);input.disabled=allHeroes;input.onchange=()=>{if(input.checked)selectedHeroes.add(id);else selectedHeroes.delete(id);render();};label.append(input,document.createTextNode(name));container.append(label);}document.getElementById('heroNote').textContent=heroes.size?'Filter uses known hero names in the current snapshot.':'No current known heroes. Unknown identities are shown only with All heroes.';}
function render(){updateStatus();refreshHeroFilter();const bounds=canvas.getBoundingClientRect(),width=Math.max(1,bounds.width),height=Math.max(1,bounds.height),dpr=Math.min(2,window.devicePixelRatio||1);if(canvas.width!==Math.round(width*dpr)||canvas.height!==Math.round(height*dpr)){canvas.width=Math.round(width*dpr);canvas.height=Math.round(height*dpr);}ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,width,height);ctx.fillStyle='#d5e1db';ctx.fillRect(0,0,width,height);document.getElementById('zoomValue').textContent=Math.round(view.zoom*100)+'%';
 const notice=document.getElementById('notice');if(!profile||!transform){notice.textContent=lastData.map_name?'No calibrated image for '+lastData.map_name+'. Choose a map or use a custom image in Settings.':'Map not identified. Choose a map in the top bar.';document.getElementById('local').textContent='Your world position is unavailable.';return;}if(!imageReady){notice.textContent=imageError||'Loading map image…';return;}
 const rect=fitMapRect(width,height,mapImage.naturalWidth,mapImage.naturalHeight),corner=mapToScreen(0,0,rect,view),w=rect.width*view.zoom,h=rect.height*view.zoom;ctx.save();ctx.filter=options.lightMap?'invert(1) grayscale(1) brightness(.8) contrast(2)':'brightness(2.4)';ctx.drawImage(mapImage,corner.x,corner.y,w,h);ctx.restore();ctx.save();ctx.beginPath();ctx.rect(corner.x,corner.y,w,h);ctx.clip();let outside=0,filtered=0;
 for(const e of lastData.ents){if(!entityFresh(e)||!Number.isFinite(e.x)||!Number.isFinite(e.y))continue;if(!includeEntity(e)){filtered++;continue;}const p=worldToMap(e.x,e.y,transform);if(p.u<0||p.u>1||p.v<0||p.v>1){outside++;continue;}const point=mapToScreen(p.u,p.v,rect,view),size=Number(options.markerSize)+(e.team==='local'?2:0),color=e.team==='local'?'#6442ad':e.team==='friendly'?'#087558':'#bb2948';ctx.fillStyle=color;ctx.strokeStyle='#fff';ctx.lineWidth=2;ctx.beginPath();ctx.arc(point.x,point.y,size,0,Math.PI*2);ctx.fill();ctx.stroke();const label=entityLabel(e);if(label){ctx.font='600 12px system-ui,sans-serif';ctx.textBaseline='middle';const textWidth=ctx.measureText(label).width,boxX=point.x+size+5,boxY=point.y-10;ctx.fillStyle='#f7faf6ed';ctx.fillRect(boxX-3,boxY,textWidth+6,20);ctx.fillStyle='#19343f';ctx.fillText(label,boxX,point.y);}if(options.showHp&&e.hp_pair_valid===true&&Number.isFinite(e.hp)&&Number.isFinite(e.max_hp)&&e.max_hp>0){const barWidth=26;ctx.fillStyle='#ffffff';ctx.fillRect(point.x-barWidth/2-1,point.y+size+3,barWidth+2,5);ctx.fillStyle='#087558';ctx.fillRect(point.x-barWidth/2,point.y+size+4,barWidth*Math.max(0,Math.min(1,e.hp/e.max_hp)),3);}}
 ctx.restore();notice.textContent=(picker.value?'Manual map: '+profile.label+' · ':'')+(outside?outside+' outside image · ':'')+(filtered?filtered+' filtered · ':'')+(!markersFresh()?'Markers hidden until a valid fresh snapshot · ':'')+'Wheel to zoom · drag to pan · calibration alignment needs live verification';
 const local=lastData.ents.find(e=>e.team==='local'&&entityFresh(e)&&Number.isFinite(e.x)&&Number.isFinite(e.y));document.getElementById('local').textContent=local?'Your world position: X '+local.x.toFixed(1)+' · Y '+local.y.toFixed(1):'Your world position is unavailable.';
}
function validResponse(j){return !!j&&Array.isArray(j.ents)&&typeof j.attached==='boolean'&&typeof j.frame_valid==='boolean'&&typeof j.valid==='boolean'&&Number.isSafeInteger(j.snapshot_ms)&&j.snapshot_ms>=0&&Number.isSafeInteger(j.server_time_ms)&&j.server_time_ms>=0&&(j.age_ms===null||(Number.isSafeInteger(j.age_ms)&&j.age_ms>=0&&j.snapshot_ms>0&&j.server_time_ms-j.snapshot_ms===j.age_ms))&&typeof j.session_epoch==='string'&&/^[0-9]+$/.test(j.session_epoch)&&j.ents.every(e=>e&&Number.isFinite(e.x)&&Number.isFinite(e.y)&&typeof e.id==='string'&&['local','friendly','enemy'].includes(e.team)&&Number.isSafeInteger(e.source_ms)&&e.source_ms>0&&Number.isSafeInteger(e.source_age_ms)&&e.source_age_ms>=0&&j.server_time_ms-e.source_ms===e.source_age_ms);}
async function fetchJsonDeadline(path){const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),REQUEST_TIMEOUT_MS),started=performance.now();try{const r=await fetch(path,{cache:'no-store',signal:controller.signal});if(!r.ok)throw Error(r.status===403?'Invalid PIN':'Radar request failed ('+r.status+')');const j=await r.json();return {data:j,elapsed:performance.now()-started};}finally{clearTimeout(timer);}}
function armFreshnessExpiry(){clearTimeout(freshnessTimer);if(!markersFresh())return;const ages=[currentAge()];for(const e of lastData.ents){if(entityFresh(e)){ages.push(sourceAge(e.source_age_ms));if(ultimateFresh(e))ages.push(sourceAge(e.ultimate.age_ms));}}freshnessTimer=setTimeout(()=>{render();armFreshnessExpiry();},Math.max(1,STALE_MS-Math.max(...ages)+1));}
function schedulePoll(delay){clearTimeout(pollTimer);if(!stopped)pollTimer=setTimeout(update,Math.max(0,delay));}
async function update(){if(busy||stopped)return;busy=true;const started=performance.now();try{const result=await fetchJsonDeadline('/data?pin='+encodeURIComponent(pin));if(stopped)return;const j=result.data;if(!validResponse(j))throw Error('Invalid radar snapshot response');if(sessionEpoch!==j.session_epoch){sessionEpoch=j.session_epoch;selectedHeroes.clear();allHeroes=true;document.getElementById('allHeroes').checked=true;document.getElementById('heroFilter').dataset.key='';}lastData=j;receivedAt=performance.now();roundTripMs=result.elapsed;responseTimes=responseTimes.filter(t=>receivedAt-t<=2000);responseTimes.push(receivedAt);lastNetworkError='';chooseMap();armFreshnessExpiry();}catch(e){if(!stopped)lastNetworkError=e.name==='AbortError'?'Radar request timed out':e.message;}finally{busy=false;if(!stopped){render();schedulePoll(1000/refreshHz-(performance.now()-started));}}}
function zoomAt(next,x,y){const rect=canvas.getBoundingClientRect();view=zoomViewAt(view,next,x,y,rect.width/2,rect.height/2);render();}
canvas.addEventListener('wheel',e=>{e.preventDefault();const rect=canvas.getBoundingClientRect(),delta=e.deltaY*(e.deltaMode===1?16:e.deltaMode===2?rect.height:1);zoomAt(view.zoom*Math.exp(-Math.max(-1000,Math.min(1000,delta))*.0015),e.clientX-rect.left,e.clientY-rect.top);},{passive:false});
canvas.addEventListener('pointerdown',e=>{if(e.button!==0||drag)return;canvas.focus({preventScroll:true});drag={id:e.pointerId,x:e.clientX,y:e.clientY,panX:view.panX,panY:view.panY};canvas.setPointerCapture(e.pointerId);canvas.classList.add('dragging');});
canvas.addEventListener('pointermove',e=>{if(!drag||drag.id!==e.pointerId)return;view.panX=drag.panX+e.clientX-drag.x;view.panY=drag.panY+e.clientY-drag.y;render();});
function stopDrag(e){if(!drag||drag.id!==e.pointerId)return;drag=null;canvas.classList.remove('dragging');if(canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId);}
for(const name of ['pointerup','pointercancel','lostpointercapture'])canvas.addEventListener(name,stopDrag);
canvas.addEventListener('keydown',e=>{const step=35;if(['+','=','-','0','ArrowLeft','ArrowRight','ArrowUp','ArrowDown'].includes(e.key))e.preventDefault();else return;if(e.key==='0')resetView();else if(e.key==='+'||e.key==='=')zoomAt(view.zoom*1.25,canvas.clientWidth/2,canvas.clientHeight/2);else if(e.key==='-')zoomAt(view.zoom/1.25,canvas.clientWidth/2,canvas.clientHeight/2);else{view.panX+=e.key==='ArrowLeft'?step:e.key==='ArrowRight'?-step:0;view.panY+=e.key==='ArrowUp'?step:e.key==='ArrowDown'?-step:0;render();}});
document.getElementById('zoomIn').onclick=()=>zoomAt(view.zoom*1.25,canvas.clientWidth/2,canvas.clientHeight/2);document.getElementById('zoomOut').onclick=()=>zoomAt(view.zoom/1.25,canvas.clientWidth/2,canvas.clientHeight/2);document.getElementById('fit').onclick=resetView;
document.getElementById('fullscreen').onclick=async()=>{try{if(document.fullscreenElement)await document.exitFullscreen();else if(viewport.requestFullscreen)await viewport.requestFullscreen();}catch{document.getElementById('notice').textContent='Browser fullscreen is unavailable. The map still fills this page.';}};document.addEventListener('fullscreenchange',()=>{document.getElementById('fullscreen').textContent=document.fullscreenElement?'Exit fullscreen':'Fullscreen';render();});
function setDrawer(open){document.getElementById('drawer').hidden=!open;for(const id of ['openSettings','openMapSettings'])document.getElementById(id).setAttribute('aria-expanded',String(open));if(open)document.getElementById('closeSettings').focus({preventScroll:true});else document.getElementById(document.fullscreenElement?'openMapSettings':'openSettings').focus({preventScroll:true});}
for(const id of ['openSettings','openMapSettings'])document.getElementById(id).onclick=()=>setDrawer(document.getElementById('drawer').hidden);document.getElementById('closeSettings').onclick=()=>setDrawer(false);document.getElementById('drawer').addEventListener('keydown',e=>{if(e.key==='Escape')setDrawer(false);});
for(const key of Object.keys(options)){document.getElementById(key).addEventListener('input',e=>{options[key]=key==='markerSize'?Number(e.target.value):e.target.checked;render();});}
document.getElementById('allHeroes').onchange=e=>{if(allHeroes&&!e.target.checked){selectedHeroes=new Set(lastData.ents.filter(e=>entityFresh(e)&&heroKnown(e)).map(e=>e.hero_id));}allHeroes=e.target.checked;document.getElementById('heroFilter').dataset.key='';render();};
ratePicker.onchange=()=>{const selected=Number(ratePicker.value);if(!allowedRates.includes(selected))return;refreshHz=selected;try{localStorage.setItem('aceRadarRefreshHz',String(refreshHz));}catch{}updateStatus();if(!busy)schedulePoll(0);};
document.addEventListener('visibilitychange',()=>{render();if(!document.hidden&&!busy)schedulePoll(0);});window.addEventListener('pagehide',()=>{stopped=true;clearTimeout(pollTimer);clearTimeout(freshnessTimer);if(customUrl)URL.revokeObjectURL(customUrl);});
for(const k of [...numericKeys,'flip_x','flip_y'])document.getElementById(k).addEventListener('input',readFields);picker.addEventListener('change',()=>{activeId='!';chooseMap();});window.addEventListener('resize',render);
document.getElementById('reset').onclick=()=>{if(!profile)return;transform={...profile.transform};try{localStorage.removeItem(storageKey());}catch{}setFields();render();};
document.getElementById('export').onclick=()=>{if(!transform)return;const blob=new Blob([JSON.stringify({map:activeId,transform},null,2)],{type:'application/json'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download='radar-calibration.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
document.getElementById('chooseImage').onclick=()=>document.getElementById('image').click();document.getElementById('image').onchange=e=>{const file=e.target.files?.[0];if(!file)return;if(!['image/png','image/jpeg','image/webp'].includes(file.type)||file.size>4*1024*1024){document.getElementById('calibrationError').textContent='Choose a PNG, JPEG or WebP image below 4 MiB.';return;}if(customUrl)URL.revokeObjectURL(customUrl);customUrl=URL.createObjectURL(file);const id='custom';profiles[id]={label:'Custom map',transform:{...defaultTransform},calibration_key:'custom',provenance:'User-provided image. World calibration must be verified.'};if(![...picker.options].some(o=>o.value===id))picker.add(new Option('Custom map',id));picker.value=id;activeId='!';chooseMap();};
(async()=>{try{const result=await fetchJsonDeadline('/maps?pin='+encodeURIComponent(pin)),j=result.data;profiles=j?.version===1&&j.maps&&typeof j.maps==='object'&&!Array.isArray(j.maps)?j.maps:{};for(const [id,p]of Object.entries(profiles)){if(/^[a-z0-9_-]{1,48}$/.test(id)&&p&&validTransform(p.transform)&&typeof p.label==='string')picker.add(new Option(p.label,id));else delete profiles[id];}}catch(e){lastNetworkError=e.name==='AbortError'?'Map configuration request timed out':e.message;}if(!stopped){render();schedulePoll(0);}})();
</script></body></html>
)HTML";
                send_response(client,"200 OK","text/html; charset=utf-8",html);
            }else send_response(client,"404 Not Found","text/plain","Not found");
            closesocket(client);
        }
        closesocket(listener);WSACleanup();alive=false;
    });
}
}

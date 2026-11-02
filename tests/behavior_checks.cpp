#include "aimbot.cpp"
#include "triggerbot.cpp"
#include "input.cpp"
#include "radar.cpp"
#include <cassert>
#include <cstdio>

static void defaults(){cfg=Settings{};cfg.register_all();cfg.context.clear();inp.panic=false;inp.device_ok=true;}
static void only(int bone){for(int i=0;i<aim_model::bone_count;i++)cfg.find(aim_model::key(i).c_str())->i=i==bone?2:-1;}
static void world(){
    for(auto& value:snap.view)value=0;snap.view[0]=snap.view[6]=.01f;snap.view[15]=1;
    snap.dead=false;snap.in_menu=false;snap.in_scoreboard=false;snap.spectating=false;
    snap.local={};snap.local.base=1;snap.local.team=1;snap.local.alive=true;
    Entity e;e.base=2;e.team=2;e.alive=e.visible=true;e.pos={0,0,-55};e.hp=100;e.max_hp=100;
    snap.enemies={e};snap.friendlies.clear();
}
static std::string get(int port,const std::string& path){
    SOCKET socket_=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(socket_!=INVALID_SOCKET);
    DWORD timeout=1500;setsockopt(socket_,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));
    sockaddr_in endpoint{};endpoint.sin_family=AF_INET;endpoint.sin_port=htons(port);endpoint.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(connect(socket_,(sockaddr*)&endpoint,sizeof(endpoint))==0);
    auto request="GET "+path+" HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    assert(send(socket_,request.data(),(int)request.size(),0)>0);
    std::string result;char buffer[4096];for(;;){int n=recv(socket_,buffer,sizeof(buffer),0);if(n<=0)break;result.append(buffer,n);}closesocket(socket_);return result;
}
static json body(const std::string& response){auto offset=response.find("\r\n\r\n");assert(offset!=std::string::npos);return json::parse(response.substr(offset+4));}
static int free_port(SOCKET* reserved=nullptr){
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(s!=INVALID_SOCKET);sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(bind(s,(sockaddr*)&address,sizeof(address))==0);int size=sizeof(address);assert(getsockname(s,(sockaddr*)&address,&size)==0);
    int port=ntohs(address.sin_port);if(reserved){assert(listen(s,1)==0);*reserved=s;}else closesocket(s);return port;
}
int main(){
    defaults();world();only(0);cfg.find("aim.bone_legs")->i=0;snap.enemies[0].pos.z=-49;
    int heads=0,legs=0;
    for(int n=0;n<1000;n++){auto p=aim_model::weighted_select(snap.enemies[0],1000,1000,(n+.5f)/1000);assert(p.valid());heads+=p.bone==0;legs+=p.bone==6;}
    assert(heads>800&&heads<900&&legs>100); // preference, not an absolute priority rule
    cfg.find("aim.bone_head")->i=2;cfg.find("aim.bone_chest")->i=2;cfg.find("aim.bone_legs")->i=-1;snap.enemies[0].pos.z=-55;
    int chests=0;for(int n=0;n<1000;n++)chests+=aim_model::weighted_select(snap.enemies[0],1000,1000,(n+.5f)/1000).bone==2;
    assert(chests>900);
    assert(aim_model::weighted_select(snap.enemies[0],1000,1000,.99f,1e30f,0).bone==0);
    only(0);cfg.find("aim.bone_neck")->i=0;snap.view[6]=.025f;snap.enemies[0].pos.z=-35;
    assert(aim_model::weighted_select(snap.enemies[0],1000,1000,.2f).bone==1);
    snap.enemies[0].visible=false;assert(!aim_model::weighted_select(snap.enemies[0],1000,1000,.2f).valid());
    world();for(int i=0;i<7;i++)cfg.find(aim_model::key(i).c_str())->i=-1;
    assert(!aim_model::weighted_select(snap.enemies[0],1000,1000,.2f).valid());
    assert(aim_model::correction({100,100},{105,100},9,0,0,0).len()==0);
    assert(aim_model::correction({100,100},{120,100},9,0,0,0).x==-20);
    assert(std::fabs(aim_model::deadzone_radius_pixels(1920,2,1)-16.756f)<.01f);
    for(int curve=0;curve<5;curve++){
        float previous=0;for(int n=0;n<=100;n++){float v=aim_model::ease(curve,n/100.f);assert(v>=previous-.0001f&&v<=1.0001f);previous=v;}
        assert(aim_model::ease(curve,0)==0&&std::fabs(aim_model::ease(curve,1)-1)<.0001f);
    }
    assert(std::fabs(aim_model::smoothing_factor(1)-1)<.001f&&std::fabs(aim_model::smoothing_factor(100)-400)<.001f);
    float prior_factor=0;for(float percent:{1.f,25.f,50.f,75.f,100.f}){float factor=aim_model::smoothing_factor(percent);assert(factor>prior_factor);prior_factor=factor;}
    aim_model::Movement invalid_time;invalid_time.velocity={1,1};assert(aim_model::controlled_motion({5,5},invalid_time,7,1,0,1,0,1,180,1600,200,false,1,5).len()==0&&invalid_time.velocity.len()==0);
    aim_model::Movement fast,slow,weak;
    assert(aim_model::movement({100,0},fast,1,0,.016f).x>aim_model::movement({100,0},slow,20,0,.016f).x);
    assert(aim_model::movement({1000,0},weak,1,1,.016f,.4f).x<=8);
    assert(aim_model::ease(0,.25f)!=aim_model::ease(1,.25f)&&aim_model::ease(1,.25f)!=aim_model::ease(2,.25f)&&aim_model::ease(2,.25f)!=aim_model::ease(3,.25f));
    for(int i=0;i<64;i++)assert(aim_model::sample_delay(0,0)>=1);
    assert(aim_model::hitchance(0,0,0,.8f)==80&&aim_model::hitchance(0,0,0,2)==100&&aim_model::hitchance(0,0,0,0)==0);
    assert(aim_model::armed(0,true,false)&&!aim_model::armed(0,false,true)&&aim_model::armed(1,false,true)&&!aim_model::armed(1,true,false));

    defaults();only(2);world();cfg.find("aim.mode")->i=2;cfg.find("aim.act_delay_min")->f=cfg.find("aim.act_delay_max")->f=0;
    aim::reset();aim::tick(1000,1000);assert(aim::state.base==2&&!aim::active);
    aim::state.engage_at=now_ms();snap.enemies[0].pos.x=10;aim::tick(1000,1000);assert(aim::active&&aim::state.base==2);
    snap.enemies[0].pos.x=0;aim::tick(1000,1000);assert(!aim::active&&aim::state.base==2);
    snap.enemies[0].pos.x=50;aim::tick(1000,1000);assert(aim::state.base==2); // sticky retains a visible target outside acquisition FOV
    cfg.find("aim.hard_lock")->b=false;aim::tick(1000,1000);assert(!aim::active);aim::reset();
    world();snap.enemies[0].vel.x=1000;aim::tick(1000,1000);assert(aim::state.base==0);
    world();cfg.find("aim.act_delay_min")->f=cfg.find("aim.act_delay_max")->f=500;aim::tick(1000,1000);assert(!aim::active&&aim::state.engage_at>now_ms());

    defaults();only(2);world();snap.enemies[0].pos.x=10;cfg.find("aim.mode")->i=2;cfg.find("aim.hard_lock")->b=false;
    aim::reset();aim::tick(1000,1000);aim::state.engage_at=now_ms();aim::state.since=now_ms();
    Entity closer=snap.enemies[0];closer.base=3;closer.pos.x=0;snap.enemies.push_back(closer);cfg.find("aim.retarget_ms")->f=1500;
    aim::tick(1000,1000);assert(aim::state.base==2);cfg.find("aim.retarget_ms")->f=0;aim::tick(1000,1000);assert(aim::state.base==3);

    cfg.find("aim.enabled")->b=false;aim::tick(1000,1000);world();only(2);
    cfg.find("input.mode")->i=2; // Device-disabled mode keeps the timing test output-free.
    cfg.find("trigger.enabled")->b=true;cfg.find("trigger.activation")->i=1;
    cfg.find("trigger.first_shot_delay_min")->f=cfg.find("trigger.first_shot_delay_max")->f=500;
    cfg.find("trigger.followup_delay_min")->f=20;cfg.find("trigger.followup_delay_max")->f=40;
    trig::previous_hero="Deadlock/hero/Unknown";trig::toggled=true;trig::last_key=(GetAsyncKeyState(cfg.I("trigger.key"))&0x8000)!=0;trig::disarm();
    trig::tick(1000,1000);assert(trig::state.armed&&trig::state.last_shot==0&&trig::state.ready_at>now_ms());
    trig::state.ready_at=now_ms();trig::tick(1000,1000);assert(trig::state.last_shot>0);
    u64 shot=trig::state.last_shot;assert(trig::state.ready_at-shot>=20&&trig::state.ready_at-shot<=40);
    trig::tick(1000,1000);assert(trig::state.last_shot==shot);
    snap.enemies[0].pos.x=10;cfg.find("trigger.magnet")->b=true;cfg.find("trigger.magnet_fov")->f=20;cfg.find("trigger.magnet_smooth")->f=100;trig::state.magnet={};
    trig::tick(1000,1000);assert(trig::state.magnet.progress>0&&trig::state.magnet.progress<.1f);
    cfg.find("trigger.magnet")->b=false;trig::tick(1000,1000);assert(trig::state.magnet.progress==0);
    cfg.find("trigger.magnet")->b=true;snap.enemies[0].vel.x=1000;trig::tick(1000,1000);assert(trig::state.magnet_target==0);
    trig::toggled=false;trig::tick(1000,1000);assert(!trig::state.armed);

    defaults();cfg.enable_override("Counter-Strike 2/Global",true);
    {Settings::Scope global("Counter-Strike 2/Global");cfg.find("aim.smooth")->f=3;}
    cfg.enable_override("Counter-Strike 2/weapon/Rifles",true);
    {Settings::Scope weapon("Counter-Strike 2/weapon/Rifles");assert(cfg.F("aim.smooth")==3);cfg.find("aim.smooth")->f=9;}
    assert(cfg.F("aim.smooth")==33);assert(cfg.save("behavior_profile.json"));defaults();assert(cfg.load("behavior_profile.json"));
    {Settings::Scope weapon("Counter-Strike 2/weapon/Rifles");assert(cfg.F("aim.smooth")==9);}
    cfg.enable_override("Counter-Strike 2/weapon/Rifles",false);
    {Settings::Scope weapon("Counter-Strike 2/weapon/Rifles");assert(cfg.F("aim.smooth")==3);}
    {Settings::Scope global("Counter-Strike 2/Global");std::thread worker([]{assert(Settings::context.empty());});worker.join();assert(cfg.F("aim.smooth")==3);}
    {std::ofstream legacy("behavior_legacy.json");legacy<<"{\"aim.hitbox\":2}";}
    assert(cfg.load("behavior_legacy.json")&&cfg.I("aim.bone_chest")==2&&cfg.I("aim.bone_head")==-1);
    {std::ofstream bad("behavior_invalid.json");bad<<"{\"aim.enabled\":false,\"aim.smooth\":\"bad\"}";}
    assert(!cfg.load("behavior_invalid.json")&&cfg.B("aim.enabled"));
    {std::ofstream zero("behavior_zero.json");zero<<"{\"aim.act_delay_min\":0,\"trigger.first_shot_delay_min\":0,\"trigger.followup_delay_min\":0}";}
    assert(cfg.load("behavior_zero.json")&&cfg.F("aim.act_delay_min")==1&&cfg.F("trigger.first_shot_delay_min")==1&&cfg.F("trigger.followup_delay_min")==1);
    assert(cfg.m.find("aim.jitter")==cfg.m.end()&&cfg.export_json().contains("aim.bone_head"));
    assert(!cfg.find("input.automatic_enabled")&&!cfg.find("input.block_output"));
    cfg.find("input.mode")->i=2;assert(hw::output_blocked());cfg.find("input.mode")->i=0;assert(!hw::output_blocked());
    inp.panic=true;assert(hw::output_blocked());inp.panic=false;inp.device_ok=true;

    // Valid visual data must never authorize output without game input state.
    snap.backend_managed=true;snap.action_valid=true;snap.frame_valid=true;snap.in_match=true;snap.local_alive_valid=true;snap.dead=false;snap.in_menu=false;snap.in_scoreboard=false;snap.spectating=false;snap.overlay_menu=false;snap.last_read_ms=now_ms();snap.input_valid=false;
    assert(!game_data_permits_output()&&hw::output_blocked());
    snap.input_valid=true;assert(game_data_permits_output()&&!hw::output_blocked());snap.local_alive_valid=false;assert(!game_data_permits_output()&&game_output_reason()=="local life/current health unavailable");snap.local_alive_valid=true;
    // Retired profile values cannot opt into the new diagnostic pause.
    snap.in_menu=true;snap.raw_ui_flags=3;snap.raw_ui_valid=true;
    cfg.find("input.disable_in_menu")->b=true;
    assert(cfg.find("input.disable_in_menu")->b&&!cfg.B("input.disable_in_menu"));
    assert(!cfg.B("dbg.pause_raw_ui_signal")&&!game_ui_pause_active()&&game_data_permits_output());
    cfg.find("dbg.pause_raw_ui_signal")->b=true;
    assert(game_ui_pause_active()&&!game_data_permits_output()&&game_output_reason()=="diagnostic pause on raw UI signal"&&hw::output_blocked());
    snap.raw_ui_valid=false;assert(!game_ui_pause_active());
    snap.frame_valid=false;assert(!game_data_permits_output()&&game_output_reason()=="no validated game snapshot");snap.frame_valid=true;
    snap.raw_ui_valid=true;snap.raw_ui_flags=4;assert(!game_ui_pause_active()&&game_data_permits_output());
    snap.raw_ui_flags=0;assert(!game_ui_pause_active());snap.raw_ui_flags=3;
    cfg.find("dbg.pause_raw_ui_signal")->b=false;snap.overlay_menu=true;
    assert(!game_data_permits_output()&&game_output_reason()=="ACE menu open");
    cfg.find("input.disable_in_overlay")->b=false;assert(game_data_permits_output()&&!hw::output_blocked());
    cfg.find("dbg.pause_raw_ui_signal")->b=true;assert(!game_data_permits_output()&&game_output_reason()=="diagnostic pause on raw UI signal");
    cfg.find("dbg.pause_raw_ui_signal")->b=false;
    // Optional pauses cannot waive mandatory data, life, match or state gates.
    snap.local_alive_valid=false;assert(!game_data_permits_output());snap.local_alive_valid=true;
    snap.dead=true;assert(!game_data_permits_output());snap.dead=false;
    snap.in_scoreboard=true;assert(!game_data_permits_output()&&game_output_reason()=="scoreboard open");snap.in_scoreboard=false;
    snap.spectating=true;assert(!game_data_permits_output());snap.spectating=false;
    snap.in_match=false;assert(!game_data_permits_output());snap.in_match=true;
    snap.input_valid=false;assert(!game_data_permits_output());snap.input_valid=true;
    snap.frame_valid=false;assert(!game_data_permits_output());snap.frame_valid=true;
    snap.last_read_ms=now_ms()-251;assert(!game_data_permits_output());snap.last_read_ms=now_ms();
    snap.in_menu=false;snap.raw_ui_flags=0;snap.raw_ui_valid=false;snap.overlay_menu=false;cfg.find("input.disable_in_overlay")->b=true;
    {Settings saved;saved.register_all();assert(!saved.B("input.disable_in_menu")&&!saved.B("dbg.pause_raw_ui_signal")&&saved.B("input.disable_in_overlay"));
        assert(saved.import_json(json{{"input.disable_in_menu",true}}));
        assert(saved.find("input.disable_in_menu")->b&&!saved.B("input.disable_in_menu")&&!saved.B("dbg.pause_raw_ui_signal")&&saved.B("input.disable_in_overlay"));
        assert(saved.export_json()["input.disable_in_menu"]==true);
        saved.find("dbg.pause_raw_ui_signal")->b=true;assert(saved.import_json(json{{"input.disable_in_menu",true}})&&!saved.B("dbg.pause_raw_ui_signal"));
        assert(saved.import_json(json{{"input.disable_in_menu",true},{"dbg.pause_raw_ui_signal",true},{"input.disable_in_overlay",false}})&&!saved.B("input.disable_in_menu")&&saved.B("dbg.pause_raw_ui_signal")&&!saved.B("input.disable_in_overlay"));
        Settings shared;shared.register_all();assert(shared.import_json(json::parse(saved.export_json().dump()))&&shared.find("input.disable_in_menu")->b&&!shared.B("input.disable_in_menu")&&shared.B("dbg.pause_raw_ui_signal"));}
    snap.backend_managed=false;snap.frame_valid=false;snap.input_valid=false;snap.in_match=false;

    defaults();WSADATA wsa;assert(WSAStartup(MAKEWORD(2,2),&wsa)==0);
    int port=free_port();cfg.find("radar.port")->i=port;
    {std::lock_guard<std::mutex> guard(snap_mtx);snap.enemies.clear();snap.friendlies.clear();snap.local={};snap.in_match=false;}
    radar::start(true);for(int n=0;n<100&&!radar::running();n++)Sleep(10);assert(radar::running());
    assert(radar::loopback_only()&&radar::current_port()==port);
    std::string pin=radar::current_pin();auto response=get(port,"/data?pin="+pin);assert(response.find("200 OK")!=std::string::npos&&body(response)["ents"].empty());
    assert(get(port,"/data?pin="+pin+"0").find("403 Forbidden")!=std::string::npos);
    {std::lock_guard<std::mutex> guard(snap_mtx);Entity friend_;friend_.base=99;friend_.pos={1,2,3};snap.friendlies.push_back(friend_);}
    assert(body(get(port,"/data?pin="+pin))["ents"].size()==1);
    u64 stop_time=now_ms();radar::stop();assert(now_ms()-stop_time<500);
    port=free_port();cfg.find("radar.port")->i=port;radar::start(false);for(int n=0;n<100&&!radar::running();n++)Sleep(10);assert(radar::running()&&!radar::loopback_only()&&radar::current_port()==port);
    assert(get(port,std::string("/data?pin=")+radar::current_pin()).find("200 OK")!=std::string::npos);radar::stop();
    SOCKET busy;cfg.find("radar.port")->i=free_port(&busy);radar::start(true);
    for(int n=0;n<100&&radar::status_text().find("failed")==std::string::npos;n++)Sleep(10);
    assert(!radar::running()&&radar::status_text().find("failed")!=std::string::npos);radar::stop();closesocket(busy);WSACleanup();
    std::remove("behavior_profile.json");std::remove("behavior_legacy.json");std::remove("behavior_invalid.json");
    std::remove("behavior_zero.json");
    std::puts("PASS: weighted/stable bones, availability, curves, manual hand-off/resume, sticky FOV, velocity gates, trigger timing/toggle, independent magnet, scopes/JSON/migration, output gates, real loopback radar HTTP/error handling");
}

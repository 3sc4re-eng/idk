#include "profile_scope.h"
#include "cs2_weapon_catalog.h"
#include <windows.h>
#include <mmsystem.h>
#include "overlay.h"
#include "cooperative_capture.h"
#include "cooperative_capture_imgui.h"
#include "aceui.h"
#include "menu_layout.h"
#include "hero_art.h"
#include "model_preview.h"
#include "imgui_internal.h"
#include "ability_icons.h"
#include "target_rules.h"
#include "esp_text.h"
#include "world_style_config.h"
#include "hero_catalog.h"
#include "esp_visuals.h"
#include "esp_colors.h"
#include "aim_model.h"
#include "collision.h"
#include "autouse.h"
#include "cleanse_catalog.h"
#include <set>
#include <string_view>
#include <wincodec.h>
#include <filesystem>
#include <shellapi.h>
#include "esp_layout.h"
#include "esp_elements.h"
#include "aimbot.h"
#include "defense.h"
#include "triggerbot.h"
#include "helpers.h"
#include "esp.h"
#include "radar.h"
#include "input.h"
#include "config.h"
#include "profile_lifecycle_policy.h"
#include "profile_selection_index.h"
#include "build_info.h"
#include "dma.h"
#include "offsets.h"
#include "live_backend.h"
#include "game.h"
#include "icon.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <windows.h>
#include <cstdio>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <exception>
#include <map>
extern char g_ace_user[64];
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace overlay{
static std::string listening_key;
static unsigned listen_held_mouse=0;
static int captured_key=0,shortcut_block_key=0;
static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==WM_ACTIVATEAPP&&(GetWindowLongPtrW(h,GWL_EXSTYLE)&WS_EX_TOOLWINDOW))SetWindowPos(h,w?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    if(!listening_key.empty()){
        int key=0;
        if((m==WM_KEYDOWN||m==WM_SYSKEYDOWN)&&!(l&(1LL<<30))){
            key=(int)w;
            if(key==VK_SHIFT)key=(int)MapVirtualKeyW((l>>16)&0xff,MAPVK_VSC_TO_VK_EX);
            if(key==VK_CONTROL)key=(l&(1LL<<24))?VK_RCONTROL:VK_LCONTROL;
            if(key==VK_MENU)key=(l&(1LL<<24))?VK_RMENU:VK_LMENU;
        }
        unsigned released=0;
        if(m==WM_LBUTTONUP){key=VK_LBUTTON;released=1;}
        if(m==WM_RBUTTONUP){key=VK_RBUTTON;released=2;}
        if(m==WM_MBUTTONUP){key=VK_MBUTTON;released=4;}
        if(m==WM_XBUTTONUP){key=GET_XBUTTON_WPARAM(w)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2;released=key==VK_XBUTTON1?8:16;}
        if(released&&(listen_held_mouse&released)){listen_held_mouse&=~released;key=0;}
        if(key&&(!Settings::action_output_binding(listening_key)||Settings::action_output_key_allowed(key)))captured_key=key;
    }
    if(m==WM_INPUT&&ImGui::GetCurrentContext()){
        RAWINPUT raw{};UINT size=sizeof(raw);
        if(GetRawInputData((HRAWINPUT)l,RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER))!=UINT(-1)&&raw.header.dwType==RIM_TYPEMOUSE){
            if(GetForegroundWindow()!=h&&ImGui::GetIO().WantCaptureMouse){
                if(raw.data.mouse.usButtonFlags&RI_MOUSE_WHEEL)ImGui::GetIO().AddMouseWheelEvent(0,(SHORT)raw.data.mouse.usButtonData/(float)WHEEL_DELTA);
                if(raw.data.mouse.usButtonFlags&RI_MOUSE_HWHEEL)ImGui::GetIO().AddMouseWheelEvent(-(SHORT)raw.data.mouse.usButtonData/(float)WHEEL_DELTA,0);
            }
        }
    }
    if(m==WM_MOUSEACTIVATE)return MA_ACTIVATE;
    if(ImGui_ImplWin32_WndProcHandler(h,m,w,l)) return 1;
    if(m==WM_DESTROY){ PostQuitMessage(0); return 0; }
    return DefWindowProcW(h,m,w,l);
}
static void set_clickthrough(HWND hwnd,bool on){
    LONG_PTR st=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
    if(on) st|=WS_EX_TRANSPARENT; else st&=~(LONG_PTR)WS_EX_TRANSPARENT;
    SetWindowLongPtrW(hwnd,GWL_EXSTYLE,st);
}
static void load_font(){ aceui::fonts(); }
static void ace_style(){ aceui::style(); }
static std::string active_slot="default",edit_game="Deadlock";
static bool preview_session=false;
static std::string profile_message,pending_profile_name;
static std::set<std::string> protected_profile_ids;
static bool active_profile_protected();
static bool save_named_profile(const std::string& name,std::string* error=nullptr);
static void request_profile_load(const std::string& name);
static void load_startup_profile();
static std::string profile_prefix(){
    for(auto* game:games::ALL)if(edit_game==game->name)return game->cfg_prefix;
    return games::DEADLOCK.cfg_prefix;
}
static std::string slot_path(){return "configs/"+profile_prefix()+"_"+active_slot+".json";}
static u64 last_save_time=0;static bool last_save_failed=false;
static void autosave_tick(){
    static u64 last=0;
    if(profile_lifecycle_policy::may_autosave(cfg.B("system.autosave"),preview_session,active_profile_protected(),cfg.dirty)&&now_ms()-last>400){
        last=now_ms();std::string error;
        if(!save_named_profile(active_slot,&error))profile_message="Automatic save failed: "+error;
    }
}
static std::string share_code_export(){
    std::string data=cfg.export_json().dump(),hex;char b[3];for(unsigned char ch:data){snprintf(b,3,"%02x",ch);hex+=b;}return "ACE1:"+hex;
}
static void clipboard_set(const std::string& s){
    if(OpenClipboard(0)){ EmptyClipboard();
        HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,s.size()+1);
        if(h){ memcpy(GlobalLock(h),s.c_str(),s.size()+1); GlobalUnlock(h);
            SetClipboardData(CF_TEXT,h); }
        CloseClipboard(); }
}
static std::string local_ip(){
    char hn[256]={};
    if(gethostname(hn,256)!=0) return "127.0.0.1";
    struct hostent* h=gethostbyname(hn);
    if(h&&h->h_addr_list&&h->h_addr_list[0]){
        struct in_addr a; memcpy(&a,h->h_addr_list[0],4);
        char* ip=inet_ntoa(a); if(ip) return std::string(ip);
    }
    return "127.0.0.1";
}
static const char* key_name(int vk){
    static char buf[32];
    if(vk==0)return "Not bound";
    if(vk==0x01) return "Left Mouse";
    if(vk==0x02) return "Right Mouse";
    if(vk==0x04) return "Middle Mouse";
    if(vk==0x05) return "Mouse 4";
    if(vk==0x06) return "Mouse 5";
    if(vk==0xA0||vk==0xA1||vk==0x10) return "Shift";
    if(vk==0xA2||vk==0xA3||vk==0x11) return "Ctrl";
    if(vk==0xA4||vk==0xA5||vk==0x12) return "Alt";
    if(vk>=0x30&&vk<=0x39){ snprintf(buf,32,"%c",'0'+vk-0x30); return buf; }
    if(vk>=0x41&&vk<=0x5A){ snprintf(buf,32,"%c",'A'+vk-0x41); return buf; }
    if(vk>=0x70&&vk<=0x87){ snprintf(buf,32,"F%d",vk-0x6F); return buf; }
    switch(vk){
        case VK_SPACE: return "Space"; case VK_TAB: return "Tab";
        case VK_ESCAPE: return "Esc"; case VK_END: return "End";
        case VK_HOME: return "Home"; case VK_INSERT: return "Ins";
        case VK_DELETE: return "Del"; case VK_RETURN: return "Enter";
        case VK_LEFT: return "Left"; case VK_RIGHT: return "Right";
        case VK_UP: return "Up"; case VK_DOWN: return "Down";
        case VK_OEM_COMMA: return "Comma"; case VK_OEM_PERIOD: return "Period";
        case VK_OEM_1: return "Semicolon"; case VK_OEM_2: return "Slash";
        case VK_OEM_3: return "Grave"; case VK_OEM_4: return "Left bracket";
        case VK_OEM_5: return "Backslash"; case VK_OEM_6: return "Right bracket";
        case VK_OEM_7: return "Apostrophe"; case VK_OEM_MINUS: return "Minus";
        case VK_OEM_PLUS: return "Equals"; case VK_PRIOR: return "Page Up";
        case VK_NEXT: return "Page Down"; case VK_BACK: return "Backspace";
    }
    UINT scan=MapVirtualKeyA((UINT)vk,MAPVK_VK_TO_VSC);
    if(scan&&GetKeyNameTextA((LONG)(scan<<16),buf,sizeof(buf))>0)return buf;
    snprintf(buf,32,"Key %d",vk); return buf;
}
static void esp_options(const char* feature);
static bool gear_button(){
    auto p=ImGui::GetCursorScreenPos();bool clicked=ImGui::InvisibleButton("##gear",{20,20});auto* d=ImGui::GetWindowDrawList();
    ImVec2 c{p.x+10,p.y+10};ImU32 color=ImGui::IsItemHovered()?aceui::purple:aceui::muted;
    d->AddCircle(c,4,color,16,1.3f);d->AddCircle(c,1.3f,color,12,1);
    for(int i=0;i<8;i++){float a=i*3.14159265f/4;d->AddLine({c.x+4*std::cos(a),c.y+4*std::sin(a)},{c.x+6*std::cos(a),c.y+6*std::sin(a)},color,1.4f);}
    return clicked;
}
// Only controls actually exposed by the supported menu panels are indexed.
// Registered legacy/internal settings are deliberately not enumerated.
struct MenuSearchDestination{std::string label,path,terms,name_terms;int tab=0,page=0;};
static std::size_t selected_world_category=0;
static char world_category_filter[96]="";
static std::string menu_search_target;
static bool menu_search_reset_scroll=false;
static ImGuiWindow* menu_scroll_window=nullptr;
static std::string menu_search_lower(std::string value){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return(char)std::tolower(c);});return value;}
static constexpr int menu_search_name_rank(std::string_view name_terms,std::string_view query){return name_terms.find(query)==std::string_view::npos?1:0;}
static_assert(menu_search_name_rank("menu font size","font")<menu_search_name_rank("custom style","font")&&menu_search_name_rank("esp font size","font")<menu_search_name_rank("text size(px)","font"),"Direct font setting names must precede alias-only matches.");
static std::vector<MenuSearchDestination> menu_setting_destinations(){
    Settings::WriteScope settings_edit(cfg);
    std::vector<MenuSearchDestination> result;
    auto add=[&](int tab,int page,const char* section,const char* section_aliases,std::initializer_list<const char*> paths){
        for(auto path:paths){
            const std::string key=path;
            if(!action_pause_policy::regular_search_key(key))continue;
            if((key=="system.width"||key=="system.height")&&!cfg.B("system.custom_resolution"))continue;
            if(key=="system.max_fps"&&cfg.B("system.fps_uncapped"))continue;
            if(key=="input.sensitivity"&&cfg.B("input.auto_sensitivity"))continue;
            if(key=="input.test_key"&&!inp.keyboard_api)continue;
            if(key=="aim.max_speed"&&!cfg.B("aim.skip_fast"))continue;
            if(key=="trigger.max_speed"&&!cfg.B("trigger.skip_fast"))continue;
            const Setting* setting=cfg.find(path);if(!setting)continue;
            std::string aliases;
            if(key=="input.mode")aliases="Makcu KMBox serial input device";
            else if(key=="input.com")aliases="COM serial port";
            else if(key=="system.overlay_vsync")aliases="VSync vertical sync";
            else if(key=="system.max_fps"||key=="system.fps_uncapped")aliases="FPS frame rate limit cap";
            else if(key=="system.autosave")aliases="autosave automatic saving";
            const std::string label=setting->name+" / "+section;
            result.push_back({label,path,menu_search_lower(label+" "+path+" "+setting->help+" "+aliases+" "+section_aliases),menu_search_lower(setting->name),tab,page});
        }
    };
    add(0,0,"Aimbot / General","smooth smoothing curve FOV prediction dead zone",{"aim.enabled","aim.key","aim.mode","aim.fov","aim.fov_circle","aim.prediction","aim.smooth","aim.curve","aim.deadzone_fov","aim.deadzone_draw"});
    add(0,0,"Aimbot / Targeting","target switch reaction delay",{"target.team_check","aim.target_players","aim.priority","aim.hard_lock","aim.max_dist","aim.visible_check","aim.skip_fast","aim.max_speed","aim.start_delay_ms","aim.act_delay_min","aim.act_delay_max","aim.retarget_ms","aim.visible_ms"});
    for(int region=0;region<7;++region)if(native_target::supports_region(region)){
        const auto path=aim_model::key(region);if(const auto* setting=cfg.find(path.c_str())){
            const auto label=setting->name+" / Aimbot / Bone selection";
            result.push_back({label,path,menu_search_lower(label+" "+path+" "+setting->help),menu_search_lower(setting->name),0,0});
        }
    }
    add(1,0,"Triggerbot / General","trigger delay time between shots",{"trigger.enabled","trigger.key","trigger.activation","trigger.prediction","trigger.first_shot_delay_min","trigger.first_shot_delay_max","trigger.followup_delay_min","trigger.followup_delay_max","trigger.hitchance_min","trigger.max_dist","trigger.skip_fast","trigger.max_speed"});
    add(1,0,"Triggerbot / Magnet","magnet smoothing FOV curve",{"trigger.magnet","trigger.magnet_prediction","trigger.magnet_fov","trigger.magnet_visible_ms","trigger.magnet_smooth","trigger.magnet_curve","trigger.magnet_strength","trigger.magnet_deadzone_fov","trigger.magnet_max_dist"});
    add(2,0,"Visuals / Global display","global gameplay visuals ESP world projectile group",{"visuals.enabled","world.enabled"});
    for(int page=0;page<2;++page)add(2,page,page?"Visuals / Teammates":"Visuals / Enemies","player ESP",{"esp.enabled","esp.box","esp.health","esp.tracers","esp.head_circle","esp.name","esp.souls","esp.distance","esp.ultimate","esp.abilities","esp.draw_distance"});
    add(2,2,"Visuals / Minions","neutral bosses troopers",{"minions.enabled","minions.enemy","minions.friendly","minions.troopers","minions.neutral","minions.bosses","minions.box","minions.health","minions.name","minions.distance","minions.distance_limit","minions.color_enemy","minions.color_friendly"});
    add(8,0,"World / Categories","world objects",{"cls.world"});
    if(edit_game=="Deadlock"&&selected_world_category<world_style_catalog::count&&!cfg.B(world_style_config::keys(selected_world_category).custom.c_str()))add(8,0,"World / Shared style","shared world defaults",{"esp.color_world","esp.draw_distance"});
    add(9,0,"Projectiles","projectiles marker name distance color size range",{"cls.projectiles","projectiles.marker","projectiles.name","projectiles.distance","projectiles.marker_size","projectiles.font_size","projectiles.max_dist","esp.color_proj"});
    if(edit_game=="Deadlock")for(std::size_t i=0;i<world_style_catalog::count;++i){
        const auto& category=world_style_catalog::categories[i];const auto& p=world_style_config::keys(i);
        auto add_world=[&](const std::string& path){const auto* setting=cfg.find(path.c_str());if(!setting)return;
            const std::string label=setting->name+" / World / "+category.label;
            result.push_back({label,path,menu_search_lower(label+" "+path+" "+setting->help),menu_search_lower(setting->name),8,0});
        };
        add_world(p.enabled);add_world(p.custom);
        if(cfg.B(p.custom.c_str())){
            add_world(p.label);add_world(p.icon);add_world(p.distance);add_world(p.color);add_world(p.draw_distance);
            if(cfg.B(p.icon.c_str()))add_world(p.icon_size);
            if(cfg.B(p.label.c_str())||cfg.B(p.distance.c_str()))add_world(p.text_size);
        }
    }
    add(4,0,"Radar","web local network PIN port",{"radar.listen","radar.port"});
    add(6,0,"Settings / Display","display FPS VSync refresh frame limit resolution",{"system.overlay_mode","system.custom_resolution","system.width","system.height","system.fps_uncapped","system.max_fps","system.overlay_vsync","ui.menu_size","ui.font_size","esp.font_size"});
    add(6,1,"Settings / Input","Makcu KMBox serial COM baud connection sensitivity mouse device delay pause",{"input.mode","input.com","input.baud","input.autoconnect","input.latency_ms","input.delay_ms","input.disable_in_overlay","input.disable_when_dead","input.disable_in_scoreboard","input.disable_when_spectator","input.auto_sensitivity","input.sensitivity","input.test_key"});
    add(6,2,"Settings / Hotkeys","key binds keyboard mouse shortcut",{"system.menu_key","system.game_menu_hotkey","system.panic_key","input.reconnect_key","system.recache_key","system.scoreboard_key","hud.keybinds"});
    add(6,3,"Settings / Appearance","theme colors save autosave automatic saving",{"ui.accent","ui.background","ui.panel","ui.text","ui.splash","system.autosave"});
    add(6,4,"Settings / DMA","reader Hz scan refresh cache collision map",{"reader.max_hz","reader.discovery_ms","reader.world_ms","system.map"});
    add(7,0,"Misc","debug logs timing",{"dbg.targets","dbg.rays","dbg.timing","dbg.ballistics","dbg.action_log"});
    return result;
}
static void menu_toolbar_next(float required){
    const float next=ImGui::GetItemRectMax().x+ImGui::GetStyle().ItemSpacing.x;
    const float end=ImGui::GetWindowPos().x+ImGui::GetWindowContentRegionMax().x;
    if(next+required<=end)ImGui::SameLine();
}

static void draw_item(const char* path,const char* tip){
    Settings::WriteScope settings_edit(cfg);
    if(action_pause_policy::is_retired_key(path))return;
    Setting* s=cfg.find(path); if(!s)return;
    ImVec2 help_start=ImGui::GetCursorScreenPos();float help_width=ImGui::GetContentRegionAvail().x;
    ImGui::PushID(path);
    if(s->type==SType::Bool){
        float x=ImGui::GetCursorPosX(),y=ImGui::GetCursorPosY();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x-66);
        ImGui::TextUnformatted(s->name.c_str()); ImGui::PopTextWrapPos();
        float bottom=ImGui::GetCursorPosY();
        std::string feature=path;
        bool options=feature=="esp.box"||feature=="esp.health"||feature=="esp.head_circle"||feature=="esp.tracers"||feature=="esp.name"||feature=="esp.souls"||feature=="esp.distance"||feature=="esp.ultimate"||feature=="esp.abilities";
        if(options){
            ImGui::SetCursorPos({ImGui::GetWindowContentRegionMax().x-54,y});if(gear_button())ImGui::OpenPopup("Element settings");
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16,14});ImGui::SetNextWindowSize({320,0},ImGuiCond_Appearing);
            if(ImGui::BeginPopup("Element settings")){esp_options(path+4);ImGui::EndPopup();}ImGui::PopStyleVar();
        }
        ImGui::SetCursorPos({ImGui::GetWindowContentRegionMax().x-28,y});
        if(aceui::toggle("##toggle",s->b)){cfg.mark_dirty();if(std::string(path)=="system.fps_uncapped"&&s->b)if(auto* sync=cfg.find("system.overlay_vsync"))sync->b=false;}
        ImGui::SetCursorPos({x,std::max(bottom,y+28.f)});
    }else{
        const float available=ImGui::GetContentRegionAvail().x;
        const float split_control=available*.48f;
        // At narrow widths, give the value its own full-width row.
        const bool stacked=split_control<std::max(112.f,ImGui::GetFontSize()*8.f);
        const float control=stacked?available:split_control;
        float x=ImGui::GetCursorPosX(),y=ImGui::GetCursorPosY();
        ImGui::PushTextWrapPos(x+(stacked?available:available-control-12));
        if(!stacked)ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(s->name.c_str());ImGui::PopTextWrapPos();
        const float label_bottom=ImGui::GetCursorPosY();
        ImGui::SetCursorPos({stacked?x:x+available-control,stacked?label_bottom:y});ImGui::SetNextItemWidth(control);
        if(s->type==SType::Key){
            bool listening=listening_key==s->path;
            if(ImGui::Button(listening?"Press key...":key_name(s->i),{-1,23})&&!listening){
                listening_key=s->path;captured_key=0;listen_held_mouse=0;
                for(int button=0;button<5;button++)if(ImGui::GetIO().MouseDown[button])listen_held_mouse|=1u<<button;
            }
            if(listening&&captured_key){
                if(captured_key!=VK_ESCAPE){s->i=Settings::normalize_binding(s->path,captured_key);cfg.mark_dirty();}
                shortcut_block_key=captured_key;captured_key=0;listening_key.clear();
            }
        }else{
            const char* aim_modes[]={"Hold key","Toggle","Always on"};
            const char* hitboxes[]={"Head","Neck","Chest"};
            const char* priorities[]={"Crosshair","Distance","Low HP"};
            const char* box_styles[]={"2D outline","Corner","Filled"};
            const char* health_styles[]={"Bar","Number","Bar + number"};
            const char* access_modes[]={"This PC only","Local network"};
            const char* devices[]={"Makcu","KMBox serial","Output disabled"};const char* baud_rates[]={"Automatic","115200","4000000"};
            const char* alt_modes[]={"Modifier + ability key","Dedicated alternate-cast key"};
            const char* curves[]={"Linear","Ease-out","Ease-in-out","Bezier","Bell curve"};
            const char* activations[]={"Hold","Toggle"};
            const char** choices=nullptr;int count=3;
            std::string p=path;
            if(p=="aim.curve"||p=="trigger.magnet_curve"){choices=curves;count=5;}
            if(p=="trigger.activation"){choices=activations;count=2;}
            if(p=="aim.mode")choices=aim_modes;
            if(p=="aim.hitbox")choices=hitboxes;
            if(p=="aim.priority")choices=priorities;
            const char* label_positions[]={"Above","Below","Left","Right"};if(p=="esp.name_position"||p=="esp.souls_position"||p=="esp.distance_position"||p=="esp.ultimate_position"||p=="esp.abilities_position"){choices=label_positions;count=4;}
            if(p=="esp.box_style"){choices=box_styles;count=3;}
            if(p=="esp.health_style"){choices=health_styles;count=3;}
            if(p=="radar.listen"){choices=access_modes;count=2;}
            if(p=="input.mode")choices=devices;if(p=="input.baud")choices=baud_rates;
            const char* collision_maps[]={"Automatic","Midtown","Hero Testing"};const char* overlay_modes[]={"Fuser off","Fuser mode"};if(p=="system.map"){choices=collision_maps;count=3;}if(p=="system.overlay_mode"){choices=overlay_modes;count=2;}
            if(p=="defense.11.alt_mode"){choices=alt_modes;count=2;}
            if(choices){
                int v=s->i<0?0:(s->i>=count?count-1:s->i);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{12,12});
                if(ImGui::BeginCombo("##value",choices[v],ImGuiComboFlags_HeightRegular)){
                    for(int i=0;i<count;i++){
                        if(ImGui::Selectable(choices[i],i==v)){s->i=i;cfg.mark_dirty();}
                        if(i==v)ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopStyleVar();
            }else if(s->type==SType::Float){
                const char* fmt=s->fstep<1?"%.1f":"%.0f";
                float maximum=s->fmax;const bool zone=p=="aim.deadzone_fov"||p=="trigger.magnet_deadzone_fov";
                if(zone){maximum=std::max(s->fmin,std::min(s->fmax,cfg.F(p=="aim.deadzone_fov"?"aim.fov":"trigger.magnet_fov")));float value=clampf(s->f,s->fmin,maximum);if(value!=s->f){s->f=value;cfg.mark_dirty();}}
                if(ImGui::SliderFloat("##value",&s->f,s->fmin,maximum,fmt,zone?ImGuiSliderFlags_AlwaysClamp:ImGuiSliderFlags_None)){if(zone)s->f=clampf(s->f,s->fmin,maximum);cfg.mark_dirty();}
            }else if(s->type==SType::Int){
                if(ImGui::SliderInt("##value",&s->i,(int)s->fmin,(int)s->fmax))cfg.mark_dirty();
            }else if(s->type==SType::Color){
                float v[]={s->c.r,s->c.g,s->c.b,s->c.a};
                if(ImGui::ColorEdit4("##value",v,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_AlphaBar)){
                    s->c={v[0],v[1],v[2],v[3]};cfg.mark_dirty();
                }
            }
        }
        // Reserve the taller of the wrapped label and its control before the separator.
        ImGui::SetCursorPos({x,std::max(label_bottom,ImGui::GetCursorPosY())});
    }
    if(ImGui::IsWindowHovered()&&ImGui::IsMouseHoveringRect(help_start,{help_start.x+help_width,ImGui::GetCursorScreenPos().y+2}))ImGui::SetTooltip("%s",tip&&*tip?tip:s->help.c_str());
    if(menu_scroll_window&&menu_search_target==path){
        ImGui::ScrollToRectEx(menu_scroll_window,ImRect(help_start,{help_start.x+help_width,ImGui::GetCursorScreenPos().y+2}),ImGuiScrollFlags_KeepVisibleEdgeY);
        menu_search_target.clear();
    }
    ImGui::Dummy({0,2});ImGui::PopID();
}
static void section(const char* title){aceui::section(title);}
// Bordered content card. Installed Dear ImGui 1.90.4 supports AutoResizeY.
// EndChild is unconditional, including a clipped BeginChild returning false.
template<class Body> static void control_card(const char* id,const char* title,Body&& body,float width=0.f){
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,8});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,3.f);ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,1.f);
    const bool visible=ImGui::BeginChild(id,{width,0},ImGuiChildFlags_Border|ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    if(visible){
        ImGui::PushFont(aceui::small);ImGui::PushTextWrapPos(0);ImGui::TextDisabled("%s",title);ImGui::PopTextWrapPos();ImGui::PopFont();
        ImGui::Spacing();ImGui::Separator();ImGui::Dummy({0,4});body();
    }
    ImGui::EndChild();ImGui::PopStyleVar(3);ImGui::Dummy({0,6});
}
// Separate scrolling panels keep all existing pages reachable at small sizes.
static void menu_column(const char* id,ImVec2 size,bool boxed){
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,boxed?ImVec2{12,10}:ImVec2{0,0});
    ImGui::BeginChild(id,size,boxed?ImGuiChildFlags_Border:ImGuiChildFlags_None,boxed?ImGuiWindowFlags_None:ImGuiWindowFlags_NoBackground);
    menu_scroll_window=ImGui::GetCurrentWindow();if(menu_search_reset_scroll)ImGui::SetScrollY(0);
    ImGui::PopStyleVar();
}
static std::vector<float> menu_tab_widths(const char* const* labels,int count){
    std::vector<float> widths;widths.reserve(static_cast<std::size_t>(count));
    for(int i=0;i<count;++i)widths.push_back(ImGui::CalcTextSize(labels[i]).x+24.f);
    return widths;
}
static float menu_tab_row_width(const char* const* labels,int count){
    float width=0;for(int i=0;i<count;++i)width+=ImGui::CalcTextSize(labels[i]).x+24.f;return width;
}
static bool menu_tabs(const char* id,const char* const* labels,int count,int& selected,float width){
    bool changed=false;ImGui::PushID(id);const auto origin=ImGui::GetCursorScreenPos();
    const auto plan=menu_layout::tabs(menu_tab_widths(labels,count),width,ImGui::GetTextLineHeight()+12.f);auto* draw=ImGui::GetWindowDrawList();
    for(int i=0;i<count;++i){const auto& cell=plan.cells[static_cast<std::size_t>(i)];const ImVec2 at{origin.x+cell.x,origin.y+cell.y};
        ImGui::SetCursorScreenPos(at);ImGui::PushID(i);
        if(ImGui::InvisibleButton("##tab",{cell.width,plan.row_height})){selected=i;changed=true;}
        const bool active=selected==i;const float inset=std::min(12.f,cell.width*.25f);
        draw->AddRectFilled({at.x+2,at.y+1},{at.x+cell.width-2,at.y+plan.row_height-2},ImGui::GetColorU32(active?ImGuiCol_HeaderActive:ImGui::IsItemHovered()?ImGuiCol_FrameBgHovered:ImGuiCol_ChildBg),5);
        if(active)draw->AddRectFilled({at.x+inset,at.y+plan.row_height-3},{at.x+cell.width-inset,at.y+plan.row_height-1},aceui::purple,1);
        const auto text=ImGui::CalcTextSize(labels[i]);const float text_inset=std::min(8.f,cell.width*.25f);
        draw->PushClipRect({at.x+text_inset,at.y},{at.x+cell.width-text_inset,at.y+plan.row_height},true);
        draw->AddText({at.x+std::max(text_inset,(cell.width-text.x)*.5f),at.y+(plan.row_height-text.y)*.5f},active?aceui::white:aceui::muted,labels[i]);draw->PopClipRect();
        if(text.x>cell.width-text_inset*2&&ImGui::IsItemHovered())ImGui::SetTooltip("%s",labels[i]);ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(origin);ImGui::Dummy({width,plan.height});ImGui::PopID();return changed;
}
static void menu_notice(const std::string& message,float width,float height){
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::BeginChild("##profile_notice",{width,height},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
    ImGui::PushTextWrapPos(0);ImGui::TextUnformatted(message.c_str());ImGui::PopTextWrapPos();
    ImGui::EndChild();ImGui::PopStyleVar();
}
// The title and toggle share one measured item. A narrow heading puts the
// toggle below its wrapped title; no child/table cursor can move the next row up.
static void menu_page_heading(const char* label,const char* master,float width,bool compact=false){
    Settings::WriteScope settings_edit(cfg);
    ImFont* font=compact?aceui::body:aceui::heading;const float padding=compact?6.f:9.f;
    const float inner_width=menu_layout::maximum(1.f,width-24.f);
    ImGui::PushFont(font);
    const bool stacked=master&&inner_width<menu_layout::maximum(120.f,ImGui::GetFontSize()*8.f);
    const float label_width=menu_layout::maximum(1.f,inner_width-(master&&!stacked?40.f:0.f));
    const float label_height=ImGui::CalcTextSize(label,nullptr,false,label_width).y;
    const float content_height=stacked?label_height+6.f+20.f:menu_layout::maximum(master?20.f:0.f,label_height);
    const float height=std::ceil(content_height+padding*2.f);const auto origin=ImGui::GetCursorScreenPos();
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin,{origin.x+width,origin.y+height},ImGui::GetColorU32(ImGuiCol_ChildBg),3.f);
    draw->AddRect(origin,{origin.x+width,origin.y+height},ImGui::GetColorU32(ImGuiCol_Border),3.f);
    ImGui::PushID(label);
    ImGui::SetCursorScreenPos({origin.x+12.f,origin.y+padding+(stacked?0.f:(content_height-label_height)*.5f)});
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+label_width);ImGui::TextUnformatted(label);ImGui::PopTextWrapPos();
    ImGui::PopFont();
    if(master)if(auto* setting=cfg.find(master);setting&&setting->type==SType::Bool){
        ImGui::SetCursorScreenPos({stacked?origin.x+12.f:origin.x+width-40.f,origin.y+padding+(stacked?label_height+6.f:(content_height-20.f)*.5f)});
        ImGui::PushID(master);if(aceui::toggle("##page_master",setting->b))cfg.mark_dirty();ImGui::PopID();
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",setting->help.c_str());
    }
    ImGui::SetCursorScreenPos(origin);ImGui::Dummy({width,height});ImGui::PopID();
}

static void items(std::initializer_list<const char*> paths){for(auto p:paths)draw_item(p,"");}
static void world_category_list(float /*height*/){
    Settings::WriteScope settings_edit(cfg);
    const auto destination=world_style_config::category_from_path(menu_search_target);
    if(destination!=world_style_catalog::missing){selected_world_category=destination;world_category_filter[0]=0;}
    control_card("##world_master","WORLD OBJECTS",[&]{items({"cls.world"});});
    if(edit_game!="Deadlock")return;
    ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##world_category_search","Search categories...",world_category_filter,sizeof(world_category_filter));
    const auto query=menu_search_lower(world_category_filter);std::size_t matches=0;
    // The outer page column owns scrolling. No nested category-list scroll region.
    for(std::size_t i=0;i<world_style_catalog::count;++i){const auto& category=world_style_catalog::categories[i];
        if(menu_search_lower(std::string(category.label)+" "+category.help).find(query)==std::string::npos)continue;
        ++matches;ImGui::PushID(static_cast<int>(i));const auto at=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;
        const bool selected=selected_world_category==i;const auto& key=world_style_config::keys(i);auto* enabled=cfg.find(key.enabled.c_str());
        if(ImGui::Selectable("##category",selected,0,{width,29})){selected_world_category=i;if(enabled){enabled->b=!enabled->b;cfg.mark_dirty();}}
        const bool hovered=ImGui::IsItemHovered();auto* draw=ImGui::GetWindowDrawList();draw->AddText({at.x+8,at.y+7},aceui::white,category.label);
        const bool on=enabled&&enabled->b;draw->AddRectFilled({at.x+width-34,at.y+8},{at.x+width-6,at.y+22},on?aceui::purple:IM_COL32(66,67,72,255),7);draw->AddCircleFilled({at.x+width-(on?13.f:27.f),at.y+15},5,aceui::white,20);
        if(menu_scroll_window&&menu_search_target==key.enabled){ImGui::ScrollToRectEx(menu_scroll_window,ImRect(at,{at.x+width,at.y+29}),ImGuiScrollFlags_KeepVisibleEdgeY);menu_search_target.clear();}
        if(hovered)ImGui::SetTooltip("%s\nClick to toggle this category and open its settings.",category.help);ImGui::PopID();
    }if(!matches)ImGui::TextDisabled("No matching categories");
}
static void world_category_details(){
    if(edit_game!="Deadlock")return;
    selected_world_category=std::min(selected_world_category,world_style_catalog::count-1);
    const auto& category=world_style_catalog::categories[selected_world_category];const auto& p=world_style_config::keys(selected_world_category);
    control_card("##world_category_details",category.label,[&]{
        // Do this before the checkbox: the first interaction may turn an
        // imported custom style off, and later toggling on must retain it.
        world_style_config::prepare_edit(cfg,selected_world_category);
        const bool was_custom=cfg.B(p.custom.c_str());draw_item(p.custom.c_str(),"");
        const bool custom=cfg.B(p.custom.c_str());
        if(!was_custom&&custom&&!cfg.B(p.initialized.c_str()))world_style_config::copy_shared(cfg,selected_world_category);
        if(!custom){ImGui::TextWrapped("Uses shared world style");return;}
        draw_item(p.label.c_str(),"");draw_item(p.icon.c_str(),"");draw_item(p.distance.c_str(),"");draw_item(p.color.c_str(),"");
        if(cfg.B(p.icon.c_str()))draw_item(p.icon_size.c_str(),"");
        if(cfg.B(p.label.c_str())||cfg.B(p.distance.c_str()))draw_item(p.text_size.c_str(),"");
        draw_item(p.draw_distance.c_str(),"");
        if(ImGui::Button("Copy shared style",{-1,28}))world_style_config::copy_shared(cfg,selected_world_category);
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Replace this category's custom appearance with the current shared values. Its enable switch is kept.");
    });
    if(!cfg.B(p.custom.c_str()))control_card("##world_shared","SHARED STYLE",[&]{
        draw_item("esp.color_world","Default color for world categories using shared style. Custom categories keep their own color.");
        draw_item("esp.draw_distance","Shared maximum distance for player ESP and world categories using shared style.");
    });
}
static void esp_options(const char* feature){
    ImGui::Text("%s settings",feature);ImGui::Separator();
    if(std::string(feature)=="name"||std::string(feature)=="souls"||std::string(feature)=="distance"){std::string p=std::string("esp.")+feature;draw_item((p+"_position").c_str(),"");draw_item((p+"_global_font").c_str(),"");if(!cfg.B((p+"_global_font").c_str()))draw_item((p+"_font").c_str(),"");draw_item((p+"_outline").c_str(),"");}
    if(std::string(feature)=="offscreen"){draw_item("esp.offscreen_color","");return;}
    if(std::string(feature)=="ultimate"){draw_item("esp.ultimate_position","");draw_item("esp.ultimate_color","");draw_item("esp.ultimate_global_font","");if(!cfg.B("esp.ultimate_global_font"))draw_item("esp.ultimate_font","");return;}
    if(std::string(feature)=="abilities"){items({"esp.abilities_position","esp.ability_size","esp.ability_seconds"});return;}
    if(std::string(feature)=="box"){draw_item("esp.box_style","");if(cfg.I("esp.box_style")==2){draw_item("esp.box_fill_visible","");draw_item("esp.box_fill_hidden","");}}
    if(std::string(feature)=="health")draw_item("esp.health_style","");
    draw_item((std::string("esp.")+feature+"_visible").c_str(),"Visible players");
    draw_item((std::string("esp.")+feature+"_hidden").c_str(),"Occluded players");
    draw_item((std::string("esp.")+feature+"_width").c_str(),"Line thickness");
    if(std::string(feature)=="health"){
        draw_item("esp.health_gradient","");draw_item("esp.health_low","");draw_item("esp.health_mid","");draw_item("esp.health_high","");
    }
}
static ImVec4 ui_color(const char* key){auto c=cfg.C(key);return {c.r,c.g,c.b,c.a};}
struct HeroChoice{const char* name;int art;};
static const HeroChoice hero_choices[]={{"Abrams",6},{"Apollo",77},{"Baba",88},{"Bebop",15},{"Billy",72},{"Calico",16},{"Celeste",81},{"Deadman Danny",78},{"Drifter",64},{"Dynamo",11},{"Graves",76},{"Grey Talon",17},{"Haze",13},{"Holliday",14},{"Infernus",1},{"Ivy",20},{"Kelvin",12},{"Lady Geist",4},{"Lash",31},{"McGinnis",8},{"Mina",63},{"Mirage",52},{"Mo & Krill",18},{"Nurse Harrow",87},{"Paige",67},{"Paradox",10},{"Pocket",50},{"Rat King",84},{"Rem",79},{"Seven",2},{"Shiv",19},{"Silver",80},{"Sinclair",60},{"Solomon",85},{"The Doorman",69},{"Venator",65},{"Victor",66},{"Vindicta",3},{"Violet",86},{"Viscous",35},{"Vyper",58},{"Warden",25},{"Wraith",7},{"Yamato",27}};
static std::vector<std::string> profile_names;
static std::string selected_profile="default";
static std::string profile_path(const std::string& name);
static std::string edit_hero="Global";
static bool editing_inherited=false;
// Imported enabled override maps may be partial. Materialize only their missing
// scoped keys before any editable header/body can receive a fallback pointer.
static void complete_menu_override(const std::string& scope,const std::vector<std::string>& parents={}){
    Settings::WriteScope settings_edit(cfg);
    auto& values=cfg.overrides[scope];Settings::Scope inherited(scope,parents);
    for(const auto& entry:cfg.m){
        if(!Settings::scoped(entry.first)||values.find(entry.first)!=values.end())continue;
        const auto* inherited_value=cfg.resolved(entry.first.c_str());
        values.emplace(entry.first,inherited_value?*inherited_value:entry.second);cfg.mark_dirty();
    }
}
static void scope_selector(float width){
    const float profile_width=width*.25f,hero_width=width*.34f;
    ImGui::SetNextItemWidth(profile_width);
    if(ImGui::BeginCombo("##active_profile",active_slot.c_str())){
        for(const auto& name:profile_names)if(ImGui::Selectable(name.c_str(),profile_lifecycle_policy::same_name(active_slot,name))){
            request_profile_load(name);
        }ImGui::EndCombo();
    }
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Active saved profile. Successful loads become this game's startup profile, except in Preview. Pending automatic saves must succeed first; Preview or manual-save edits require an explicit choice.");
    ImGui::SameLine(0,8);ImGui::SetNextItemWidth(hero_width);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,8});
    const auto* edited_weapon=edit_game=="Counter-Strike 2"?cs2_weapons::find(std::string_view(edit_hero)):nullptr;
    const char* scope_label=edit_hero=="Global"?"Global":(edited_weapon?edited_weapon->label:edit_hero.c_str());
    ImGui::SetNextWindowSizeConstraints({hero_width,0},{420,480});
    if(ImGui::BeginCombo("##hero_scope",scope_label)){
        if(ImGui::Selectable("Global",edit_hero=="Global"))edit_hero="Global";
        if(edit_game=="Deadlock")for(auto& hero:hero_choices){if(ImGui::Selectable(hero.name,edit_hero==hero.name))edit_hero=hero.name;}
        else if(edit_game=="Counter-Strike 2"||edit_game=="Call of Duty"){
            const char* categories[]={"Pistols","SMGs","Rifles","Snipers","Shotguns","Machine guns"};
            for(auto name:categories)if(ImGui::Selectable(name,edit_hero==name))edit_hero=name;
            if(edit_game=="Counter-Strike 2"){
                ImGui::SeparatorText("Individual weapons");
                for(const auto& weapon:cs2_weapons::catalog){const auto label=std::string(weapon.category)+" / "+weapon.label;
                    if(ImGui::Selectable(label.c_str(),edit_hero==weapon.key))edit_hero=weapon.key;
                }
            }
        }
        ImGui::EndCombo();
    }ImGui::PopStyleVar();cfg.context.clear();editing_inherited=false;
    // Profile loading/saving above must finish before entering the editor lock.
    Settings::WriteScope settings_edit(cfg);
    if(edit_game!="Deadlock"){
        std::string global=edit_game+"/Global";if(cfg.overrides.find(global)==cfg.overrides.end())cfg.enable_override(global,true);complete_menu_override(global);Settings::set_context(global);
    }
    if(edit_hero!="Global"){
        const auto* weapon=edit_game=="Counter-Strike 2"?cs2_weapons::find(std::string_view(edit_hero)):nullptr;
        const std::string scope=edit_game+(edit_game=="Deadlock"?"/hero/":(weapon?"/weapon-item/":"/weapon/"))+edit_hero;
        std::vector<std::string> parents;if(weapon)parents.push_back(edit_game+"/weapon/"+weapon->category);
        bool on=cfg.override_enabled[scope];ImGui::SameLine();
        if(ImGui::Checkbox(weapon?"Override category":"Override global",&on))cfg.enable_override(scope,on,parents);
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Use independent settings for this selection. Otherwise inherit the enabled category and this game's Global settings.");
        if(on)complete_menu_override(scope,parents);
        Settings::set_context(scope,parents);editing_inherited=!on;
    }
}
static ID3D11Device* ui_device=nullptr;
static std::map<int,ID3D11ShaderResourceView*> portrait_textures;
static ImTextureID portrait(int id){
    auto found=portrait_textures.find(id);if(found!=portrait_textures.end())return (ImTextureID)found->second;
    unsigned size=0;auto data=hero_png(id,size);if(!data)return nullptr;
    IWICImagingFactory* factory=nullptr;IWICStream* stream=nullptr;IWICBitmapDecoder* decoder=nullptr;
    IWICBitmapFrameDecode* frame=nullptr;IWICFormatConverter* converter=nullptr;ID3D11Texture2D* texture=nullptr;
    ID3D11ShaderResourceView* view=nullptr;UINT w=0,h=0;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr))hr=factory->CreateStream(&stream);
    if(SUCCEEDED(hr))hr=stream->InitializeFromMemory((BYTE*)data,size);
    if(SUCCEEDED(hr))hr=factory->CreateDecoderFromStream(stream,nullptr,WICDecodeMetadataCacheOnLoad,&decoder);
    if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
    if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&converter);
    if(SUCCEEDED(hr))hr=converter->Initialize(frame,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    if(SUCCEEDED(hr))hr=converter->GetSize(&w,&h);
    if(SUCCEEDED(hr)&&w&&h&&w<=2048&&h<=2048){
        std::vector<BYTE> pixels(w*h*4);hr=converter->CopyPixels(nullptr,w*4,(UINT)pixels.size(),pixels.data());
        if(SUCCEEDED(hr)){
            D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=1;
            desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA src{pixels.data(),w*4,0};hr=ui_device->CreateTexture2D(&desc,&src,&texture);
            if(SUCCEEDED(hr))hr=ui_device->CreateShaderResourceView(texture,nullptr,&view);
        }
    }
    if(texture)texture->Release();if(converter)converter->Release();if(frame)frame->Release();
    if(decoder)decoder->Release();if(stream)stream->Release();if(factory)factory->Release();
    portrait_textures[id]=view;return (ImTextureID)view;
}
static void fitted_portrait(ImTextureID image,int id,float size){
    auto* info=current_hero(id);float ratio=info?(float)info->icon_width/info->icon_height:1;
    float w=ratio>=1?size:size*ratio,h=ratio>=1?size/ratio:size;
    auto p=ImGui::GetCursorScreenPos();ImGui::Dummy({size,size});ImGui::GetWindowDrawList()->AddImage(image,{p.x+(size-w)/2,p.y+(size-h)/2},{p.x+(size+w)/2,p.y+(size+h)/2});
}
static char new_profile[41]="";
static bool safe_profile(const std::string& name){
    if(name.empty()||name.size()>40)return false;
    for(unsigned char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
    return true;
}
static std::string profile_path(const std::string& name){return "configs/"+profile_prefix()+"_"+name+".json";}
static std::string profile_identity(const std::string& name){
    std::string id=profile_prefix()+"/"+name;for(char& c:id)c=profile_lifecycle_policy::fold_ascii(c);return id;
}
static bool active_profile_protected(){return protected_profile_ids.find(profile_identity(active_slot))!=protected_profile_ids.end();}
static void protect_failed_load(const std::string& name){
    std::error_code ec;const bool exists=std::filesystem::exists(profile_path(name),ec);
    if(exists||ec)protected_profile_ids.insert(profile_identity(name));
}
static bool save_named_profile(const std::string& name,std::string* error){
    auto fail=[&](const char* message){if(error)*error=message;if(profile_lifecycle_policy::same_name(name,active_slot))last_save_failed=true;return false;};
    if(!safe_profile(name))return fail("Invalid profile name.");
    if(protected_profile_ids.find(profile_identity(name))!=protected_profile_ids.end())return fail("This file could not be loaded and is protected. Create a new profile to save these settings.");
    std::error_code ec;std::filesystem::create_directories("configs",ec);if(ec)return fail("Could not create or access the configs folder.");
    const bool exists=std::filesystem::exists(profile_path(name),ec);if(ec)return fail("Could not inspect the destination profile.");
    if(exists){Settings validation;validation.register_all();if(!validation.load(profile_path(name))){protected_profile_ids.insert(profile_identity(name));return fail("The destination file could not be loaded and was left unchanged. Create a new profile instead.");}}
    const bool save_active=profile_lifecycle_policy::same_name(name,active_slot);bool saved=false;
    try{saved=cfg.save(profile_path(name),save_active);}catch(...){return fail("Could not write the profile.");}
    if(!saved)return fail("Could not write the profile.");
    if(save_active){last_save_failed=false;last_save_time=now_ms();}
    return true;
}
static bool load_profile_now(const std::string& name){
    if(!safe_profile(name)){profile_message="Invalid profile name. Settings were not changed.";return false;}
    if(!cfg.load(profile_path(name))){protect_failed_load(name);profile_message="Could not load "+name+". Current settings were kept.";if(protected_profile_ids.find(profile_identity(name))!=protected_profile_ids.end())profile_message+=" The unreadable file is protected from writes.";return false;}
    protected_profile_ids.erase(profile_identity(name));active_slot=selected_profile=name;edit_hero="Global";cfg.context.clear();
    last_save_failed=false;last_save_time=0;
    if(profile_selection_index::may_remember(preview_session,true)){
        std::string error;if(profile_selection_index::remember(profile_prefix(),name,error))profile_message="Loaded "+name;
        else profile_message="Loaded "+name+" for this session. Startup selection was not remembered: "+error;
    }else profile_message="Loaded "+name+" for this preview. Startup selection is unchanged.";
    return true;
}
static void request_profile_load(const std::string& name){
    using profile_lifecycle_policy::LoadAction;
    const auto action=profile_lifecycle_policy::before_load(cfg.dirty,cfg.B("system.autosave"),preview_session,active_profile_protected());
    if(action==LoadAction::confirm_discard){pending_profile_name=name;return;}
    if(action==LoadAction::save_current){std::string error;if(!save_named_profile(active_slot,&error)){profile_message="Load cancelled. Current edits were kept: "+error;return;}}
    load_profile_now(name);
}
static void load_startup_profile(){
    // Startup reads metadata only. Neither fallback nor Preview repairs/replaces it.
    const auto index=profile_selection_index::read();const auto remembered=index.selected(profile_prefix());
    std::string notice=index.status==profile_selection_index::ReadStatus::protected_failure?index.error:std::string{};
    auto add_notice=[&](const std::string& text){if(!notice.empty())notice+=" ";notice+=text;};
    active_slot=selected_profile=remembered.empty()?"default":remembered;
    if(cfg.load(slot_path())){protected_profile_ids.erase(profile_identity(active_slot));profile_message=notice;return;}
    protect_failed_load(active_slot);
    if(!remembered.empty()&&!profile_lifecycle_policy::same_name(remembered,"default")){
        active_slot=selected_profile="default";
        if(cfg.load(slot_path())){
            protected_profile_ids.erase(profile_identity(active_slot));add_notice("Could not restore "+remembered+"; loaded default instead.");profile_message=notice;return;
        }
        protect_failed_load(active_slot);add_notice("Could not restore "+remembered+"; using unsaved defaults.");
    }else if(!remembered.empty())add_notice("Could not restore "+remembered+"; using unsaved defaults.");
    if(active_profile_protected())add_notice("The original "+active_slot+" file is protected; create a new profile to save current settings.");
    profile_message=notice;
}
static void refresh_profiles(){
    profile_names.clear();std::error_code ec;std::string prefix=profile_prefix()+"_";
    for(auto& file:std::filesystem::directory_iterator("configs",ec)){
        if(!file.is_regular_file())continue;auto s=file.path().filename().string();
        if(s.rfind(prefix,0)!=0||s.size()<=prefix.size()+5||s.substr(s.size()-5)!=".json")continue;
        auto name=s.substr(prefix.size(),s.size()-prefix.size()-5);if(safe_profile(name))profile_names.push_back(name);
    }
    std::sort(profile_names.begin(),profile_names.end());
}
// Preview switches preserve unsaved edits in memory, independently for each game.
// Settings::register_all alone does not clear overrides or keys from another game.
struct GameProfileState{Settings settings;std::string slot,selection;};
static std::map<std::string,GameProfileState> profile_game_states;
static void switch_profile_game(const std::string& name){
    if(name==edit_game)return;
    if(profile_lifecycle_policy::may_autosave(cfg.B("system.autosave"),preview_session,active_profile_protected(),cfg.dirty)){
        std::string error;if(!save_named_profile(active_slot,&error)){profile_message="Game profile switch cancelled. Current edits were kept: "+error;return;}
    }
    profile_game_states[edit_game]={cfg,active_slot,selected_profile};
    profile_message.clear();edit_game=name;edit_hero="Global";Settings::context.clear();Settings::visual_team=false;
    auto state=profile_game_states.find(name);
    if(state!=profile_game_states.end()){
        cfg=state->second.settings;active_slot=state->second.slot;selected_profile=state->second.selection;
    }else{
        active_slot=selected_profile="default";cfg=Settings{};cfg.register_all();load_startup_profile();
    }
    new_profile[0]=0;last_save_time=0;last_save_failed=false;if(active_profile_protected())profile_message="The active profile file is protected because it could not be loaded. Create a new profile to save current settings.";
    refresh_profiles();
}
static ImU32 priority_color(int p){return p==2?IM_COL32(229,181,75,255):(p==1?IM_COL32(235,235,241,255):(p==0?IM_COL32(123,125,140,255):IM_COL32(54,55,65,140)));}
static const char* priority_name(int p){return p==2?"High":(p==1?"Medium":(p==0?"Low":"Ignore"));}
static void draw_pair(const char* label,const char* lo,const char* hi,float maximum){
    Settings::WriteScope settings_edit(cfg);
    auto* a=cfg.find(lo);auto* b=cfg.find(hi);if(!a||!b)return;ImGui::PushID(lo);
    ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);
    if(ImGui::SliderFloat("##minimum",&a->f,1,maximum,"Minimum: %.0f ms")){a->f=std::min(a->f,b->f);cfg.mark_dirty();}if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\nDrag the slider to set the shortest delay.",a->help.c_str());
    ImGui::SetNextItemWidth(-1);if(ImGui::SliderFloat("##maximum",&b->f,1,maximum,"Maximum: %.0f ms")){b->f=std::max(b->f,a->f);cfg.mark_dirty();}if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\nDrag the slider to set the longest delay.",b->help.c_str());
    ImGui::Separator();ImGui::PopID();
}
static void aim_points(bool heading=true){
    Settings::WriteScope settings_edit(cfg);
    if(heading)section("TARGET POINT PRIORITY");
    const int values[]={2,1,0,-1};const char* priorities[]={"High","Medium","Low","Ignore"};
    for(int bone:{0,1,2,3,4,5,6}){
        if(!native_target::supports_region(bone))continue;
        auto key=aim_model::key(bone);auto* setting=cfg.find(key.c_str());if(!setting)continue;int current=3;for(int i=0;i<4;i++)if(values[i]==setting->i)current=i;
        const auto region_start=ImGui::GetCursorScreenPos();
        ImGui::PushID(bone);ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(aim_model::bone_names[bone]);ImGui::SameLine(ImGui::GetContentRegionAvail().x*.42f);ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##priority",priorities[current])){for(int i=0;i<4;i++)if(ImGui::Selectable(priorities[i],i==current)){setting->i=values[i];cfg.mark_dirty();}ImGui::EndCombo();}
        if(menu_scroll_window&&menu_search_target==key){ImGui::ScrollToRectEx(menu_scroll_window,ImRect(region_start,{region_start.x+ImGui::GetContentRegionAvail().x,ImGui::GetCursorScreenPos().y}),ImGuiScrollFlags_KeepVisibleEdgeY);menu_search_target.clear();}
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Higher priority increases this region's selection weight; distance to your crosshair also matters. Ignore prevents selection. The choice stays stable while tracking.");ImGui::PopID();
    }
}
struct ModelCanvas{game_model::Frame frame;ImVec2 origin{};float width=0,height=0;};
static game_model::View model_views[2];
static std::vector<ImRect> preview_element_bounds;static bool preview_label_capture=false;
static ModelCanvas model_canvas(float height,bool){
    ModelCanvas output;output.width=ImGui::GetContentRegionAvail().x;output.height=height;output.origin=ImGui::GetCursorScreenPos();
    int model=edit_game=="Deadlock"?0:(edit_game=="Counter-Strike 2"?1:-1);
    if(model<0){ImGui::TextWrapped("A model from this game has not been imported.");return output;}
    output.frame=game_model::render(model,(int)output.width,(int)height,model_views[model]);
    if(!output.frame.error.empty()){ImGui::TextWrapped("Model load failed: %s",output.frame.error.c_str());return output;}
    ImGui::InvisibleButton("##esp_model",{output.width,height},ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight);
    bool hovered=ImGui::IsItemHovered();if(hovered)ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,ImGui::GetItemID());auto* d=ImGui::GetWindowDrawList();auto p=output.origin;
    d->AddImage(output.frame.texture,p,{p.x+output.width,p.y+height});
    if(ImGui::IsMouseClicked(1)){preview_label_capture=false;for(const auto& bounds:preview_element_bounds)if(bounds.Contains(ImGui::GetIO().MousePos))preview_label_capture=true;}
    if(!ImGui::GetIO().MouseDown[1])preview_label_capture=false;
    if(ImGui::IsItemActive()&&ImGui::GetIO().MouseDown[1]&&!preview_label_capture){auto delta=ImGui::GetIO().MouseDelta;model_views[model].yaw+=delta.x*.012f;model_views[model].pitch=clampf(model_views[model].pitch+delta.y*.007f,-.6f,.6f);}
    if(hovered&&ImGui::GetIO().MouseWheel)model_views[model].zoom=clampf(model_views[model].zoom+ImGui::GetIO().MouseWheel*.08f,.75f,1.4f);
    aceui::caption(output.frame.label.c_str());aceui::caption("Right-drag: rotate / Wheel: zoom");return output;
}
static void draw_aim_figure(){
    section("FOV / DEAD ZONE");
    const float width=ImGui::GetContentRegionAvail().x,height=210;
    const auto origin=ImGui::GetCursorScreenPos();ImGui::InvisibleButton("##aim_zone_diagram",{width,height});
    auto* d=ImGui::GetWindowDrawList();const float viewport=std::max(1.f,ImGui::GetIO().DisplaySize.x);
    float projection=aim_model::projection_x();const bool reference=!std::isfinite(projection)||projection<.001f;if(reference)projection=1;
    const float fov=cfg.F("aim.fov"),deadzone=aim_model::capped_deadzone_fov(cfg.F("aim.deadzone_fov"),fov);
    const float acquisition=aim_model::fov_radius(viewport,fov,projection);
    const float zone=aim_model::deadzone_radius_pixels(viewport,deadzone,projection);
    const float extent=std::max({acquisition,zone,1.f}),scale=std::max(8.f,std::min(width,height)*.5f-20)/extent;
    const ImVec2 center{origin.x+width*.5f,origin.y+height*.5f};
    const ImVec2 example{center.x+acquisition*scale*.32f,center.y-acquisition*scale*.24f};
    d->AddRectFilled(origin,{origin.x+width,origin.y+height},IM_COL32(26,31,39,255),8);
    d->PushClipRect(origin,{origin.x+width,origin.y+height},true);
    d->AddCircle(center,acquisition*scale,aceui::purple,96,1.5f);
    if(zone>0)d->AddCircle(center,zone*scale,IM_COL32(240,195,95,230),64,1.5f);
    d->AddLine({center.x-5,center.y},{center.x+5,center.y},aceui::white,1.3f);
    d->AddLine({center.x,center.y-5},{center.x,center.y+5},aceui::white,1.3f);
    d->AddLine(center,example,IM_COL32(140,150,170,130),1);d->AddCircleFilled(example,2,aceui::white,12);
    d->PopClipRect();
    ImGui::Text("FOV %.1f deg / Dead zone %.2f deg",fov,deadzone);
    aceui::caption("Purple: acquisition / Gold: manual aim inside the center ring");
    if(ImGui::IsMouseHoveringRect(origin,{origin.x+width,origin.y+height}))
        ImGui::SetTooltip("Both circles stay centered on your crosshair. Assistance stops while the final aim point is inside the gold ring and resumes outside. Both values use full-cone angles.%s",reference?" Diagram uses a reference projection until live camera data is available.":"");
}
static void draw_esp_builder(bool heading=true,float canvas_height=400){
    Settings::WriteScope settings_edit(cfg);
    using namespace esp_elements;if(heading)section("ESP PREVIEW");
    if(ImGui::SmallButton("Reset layout")){for(int i=0;i<Count;i++)if(auto* setting=cfg.find(position_keys[i]))setting->i=default_sides[i];cfg.mark_dirty();}
    menu_toolbar_next(ImGui::GetFrameHeight()+ImGui::GetStyle().ItemInnerSpacing.x+ImGui::CalcTextSize("Visible").x);if(auto* state=cfg.find("esp.preview_visible")){if(ImGui::Checkbox("Visible",&state->b))cfg.mark_dirty();if(ImGui::IsItemHovered())ImGui::SetTooltip("Preview the visible-player style. This changes only the model preview.");}
    const bool visible=cfg.B("esp.preview_visible");
    auto canvas=model_canvas(canvas_height,false);if(!canvas.frame.texture)return;auto* d=ImGui::GetWindowDrawList();auto origin=canvas.origin;
    float x1=origin.x+canvas.frame.minimum.x,x2=origin.x+canvas.frame.maximum.x,y1=origin.y+canvas.frame.minimum.y,y2=origin.y+canvas.frame.maximum.y;
    d->PushClipRect(origin,{origin.x+canvas.width,origin.y+canvas.height},true);
    esp_box(d,x1,y1,x2,y2,visible);esp_health(d,x1,y1,x2,y2,75,100,visible);
    if(cfg.B("esp.tracers"))d->AddLine({origin.x+canvas.width/2,origin.y+canvas.height},{(x1+x2)/2,y2},esp_rgba(esp_element_color("tracers",visible)),cfg.F("esp.tracers_width"));
    if(cfg.B("esp.head_circle"))for(auto& joint:canvas.frame.joints)if(joint.group==0)d->AddCircle({origin.x+joint.screen.x,origin.y+joint.screen.y},12,esp_rgba(esp_element_color("head_circle",visible)),24,cfg.F("esp.head_circle_width"));
    const char* text[]={canvas.frame.label.c_str(),"12,000 souls","24m","ULT READY"};std::array<bool,Count> enabled{};std::array<ImVec2,Count> sizes{};
    std::array<ability_tracking::View,4> states{};for(int slot=0;slot<4;slot++){if(auto* definition=ability_data::signature(edit_game=="Deadlock"?25:0,slot))states[slot].token=definition->token;states[slot].state=slot==1?ability_tracking::State::Cooldown:ability_tracking::State::Ready;states[slot].seconds=slot==1?8:0;}
    auto row=ability_row(states,cfg.F("esp.ability_size"),cfg.B("esp.ability_seconds"));
    for(int i=0;i<Count;i++){enabled[i]=cfg.B(enabled_keys[i]);sizes[i]=i==Abilities?row.size:ImGui::GetFont()->CalcTextSizeA(esp_text_size(features[i]),1e10f,0,text[i]);}
    auto blocks=place(enabled,sizes,x1,y1,x2,y2);fit_preview(blocks,{origin.x+4,origin.y+4},{origin.x+canvas.width-4,origin.y+canvas.height-4});preview_element_bounds.clear();static int dragging=-1;
    for(int i=0;i<Count;i++)if(blocks[i].enabled){auto& b=blocks[i];preview_element_bounds.push_back(ImRect(b.position,{b.position.x+b.size.x,b.position.y+b.size.y}));
        bool hover=ImGui::IsMouseHoveringRect(origin,{origin.x+canvas.width,origin.y+canvas.height})&&ImGui::IsMouseHoveringRect(b.position,{b.position.x+b.size.x,b.position.y+b.size.y});if(hover&&ImGui::IsMouseClicked(0))dragging=i;
        if(dragging==i&&ImGui::IsMouseDown(0)){auto mouse=ImGui::GetIO().MousePos;float dx=(mouse.x-(x1+x2)*.5f)/std::max(1.f,(x2-x1)*.5f),dy=(mouse.y-(y1+y2)*.5f)/std::max(1.f,(y2-y1)*.5f);int side=std::fabs(dx)>std::fabs(dy)?(dx<0?2:3):(dy<0?0:1);if(side!=b.side){cfg.find(position_keys[i])->i=side;cfg.mark_dirty();}}
        if(i==Abilities)draw_abilities(d,b.position,row);else esp_text(d,features[i],b.position,hover?aceui::purple:(i==Ultimate?esp_rgba(cfg.C("esp.ultimate_color")):esp_rgba(esp_element_color(features[i],visible))),text[i]);
        if(hover)d->AddRect({b.position.x-2,b.position.y-2},{b.position.x+b.size.x+2,b.position.y+b.size.y+2},aceui::purple,2);
        ImGui::PushID(features[i]);if(hover&&ImGui::IsMouseClicked(1))ImGui::OpenPopup("Preview element");ImGui::SetNextWindowSize({360,0},ImGuiCond_Appearing);if(ImGui::BeginPopup("Preview element")){esp_options(features[i]);if(ImGui::Button("Hide this element")){cfg.find(enabled_keys[i])->b=false;cfg.mark_dirty();ImGui::CloseCurrentPopup();}ImGui::EndPopup();}ImGui::PopID();
        if(hover)ImGui::SetTooltip("Drag to place %s above, below, left or right. Right-click for settings.",titles[i]);
    }
    d->PopClipRect();if(!ImGui::IsMouseDown(0))dragging=-1;ImGui::PushFont(aceui::small);ImGui::PushTextWrapPos(0);ImGui::TextDisabled("Drag any label or the ability row to place it. Right-click for settings.");ImGui::PopTextWrapPos();ImGui::PopFont();
    ImGui::TextDisabled("Hidden elements");bool first_hidden=true;
    for(int i=0;i<Count;i++)if(!enabled[i]){
        if(!first_hidden)menu_toolbar_next(ImGui::CalcTextSize(titles[i]).x+ImGui::GetStyle().FramePadding.x*2);
        ImGui::PushID(features[i]);if(ImGui::SmallButton(titles[i])){cfg.find(enabled_keys[i])->b=true;cfg.mark_dirty();}ImGui::PopID();first_hidden=false;
    }

}
static void output_binding(const char* path){
    Settings::WriteScope settings_edit(cfg);
    auto* setting=cfg.find(path);if(!setting)return;
    int normalized=Settings::normalize_binding(path,setting->i);if(normalized!=setting->i){setting->i=normalized;cfg.mark_dirty();}
    if(inp.keyboard_ok){draw_item(path,"Choose a keyboard key, Middle Mouse, Mouse 4 or Mouse 5 bound to this action in the game. Left and Right Mouse are reserved for activation and shooting.");return;}
    ImGui::PushID(path);ImGui::TextUnformatted(setting->name.c_str());
    const int values[]={0,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2};const char* names[]={"Not bound","Mouse 3 (middle)","Mouse 4 (side)","Mouse 5 (side)"};
    constexpr int count=sizeof(values)/sizeof(values[0]);int selected=-1;for(int i=0;i<count;i++)if(setting->i==values[i])selected=i;ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##output",selected>=0?names[selected]:"Keyboard binding unavailable")){for(int i=0;i<count;i++)if(ImGui::Selectable(names[i],selected==i)){setting->i=values[i];cfg.mark_dirty();}ImGui::EndCombo();}
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Choose Middle Mouse, Mouse 4 or Mouse 5 bound to this action in the game. Keyboard delivery must be verified before keyboard bindings are available.");ImGui::PopID();
}
static void input_binding_help(){if(!inp.keyboard_ok)ImGui::TextWrapped("Bind this action to Middle Mouse, Mouse 4 or Mouse 5 in the game, then select it above.");}
static void threat_list(const std::string& prefix){
    Settings::WriteScope settings_edit(cfg);
    ImGui::PushID(prefix.c_str());static char filter[96]="";ImGui::InputTextWithHint("##threat_filter","Search heroes, abilities or items",filter,sizeof(filter));
    const bool counterspell=prefix=="defense.counterspell.";
    auto matches=[&](std::string value){std::string needle=filter;std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return (char)std::tolower(c);});std::transform(needle.begin(),needle.end(),needle.begin(),[](unsigned char c){return (char)std::tolower(c);});return needle.empty()||value.find(needle)!=std::string::npos;};
    auto supported=[&](const ability_data::Entry& entry){return (counterspell?defense::counterspell_observable(entry):defense::observable(entry))&&cfg.find(defense::selection(prefix,entry.token).c_str());};
    bool any=false;
    for(auto& hero:current_heroes){bool shown=false;for(auto& entry:ability_data::entries)if(entry.hero==hero.id&&supported(entry)&&matches(std::string(hero.name)+" "+entry.title)){shown=true;break;}if(!shown)continue;
        section(hero.name);for(auto& entry:ability_data::entries)if(entry.hero==hero.id&&supported(entry)&&matches(std::string(hero.name)+" "+entry.title)){
            draw_item(defense::selection(prefix,entry.token).c_str(),counterspell?defense::counterspell_observation(entry):defense::observation(entry));any=true;
        }
    }
    bool items_shown=false;for(auto& entry:ability_data::entries)if(entry.item&&supported(entry)&&matches(entry.title)){items_shown=true;break;}
    if(items_shown){section("Enemy active items");for(auto& entry:ability_data::entries)if(entry.item&&supported(entry)&&matches(entry.title)){
        draw_item(defense::selection(prefix,entry.token).c_str(),counterspell?defense::counterspell_observation(entry):defense::observation(entry));any=true;
    }}
    if(!any)ImGui::TextDisabled("No matching supported abilities.");ImGui::PopID();
}
static bool hero_has_action(unsigned id){
    if(id==11||id==77||id==63)return true; // Actual defense::choose implementations.
    for(const auto& rule:autouse::rules())if(rule.hero==id&&id!=0)return true;
    return false;
}
static void normalize_helper_selection(int& selected){
    const int count=(int)(sizeof(hero_choices)/sizeof(hero_choices[0]));
    if(selected>0&&selected<=count&&hero_has_action((unsigned)hero_choices[selected-1].art))return;
    unsigned current_hero=0;{std::lock_guard<std::mutex> lock(snap_mtx);current_hero=snap.local.hero;}
    selected=0;
    for(int i=0;i<count;++i)if(hero_has_action((unsigned)hero_choices[i].art)){
        if(!selected)selected=i+1;
        if((unsigned)hero_choices[i].art==current_hero){selected=i+1;break;}
    }
}
static void normalize_item_selection(u32& selected){
    u32 fallback=0;bool available=false;
    for(const auto& entry:ability_data::entries)if(std::string(entry.key)=="upgrade_active_reload"){fallback=entry.token;available|=selected==entry.token;}
    for(const auto& rule:autouse::rules())if(rule.hero==0&&rule.kind!=autouse::Cleanse){if(!fallback)fallback=rule.token;available|=selected==rule.token;}
    if(!available)selected=fallback;
}
static void hero_defense(unsigned id){
    auto p=defense::prefix(id);bool supported=id==11||id==77||id==63;
    if(!supported)return;
    section("DEFENSIVE ABILITY");draw_item((p+"enabled").c_str(),"");output_binding((p+"key").c_str());input_binding_help();draw_item("defense.lead_ms","");
    if(id==11){draw_item((p+"protect_team").c_str(),"");draw_item((p+"alt_mode").c_str(),"");output_binding((p+"alt_key").c_str());}
    section("SELECT THREATS");threat_list(p);
}
static void cleanse_effects(){
    Settings::WriteScope settings_edit(cfg);
    section("EFFECTS TO CLEANSE");static char filter[80]="";static std::set<u32> observed;
    {std::lock_guard<std::mutex> lock(snap_mtx);for(auto token:snap.local.removable_debuffs)observed.insert(token);}
    ImGui::InputTextWithHint("##cleanse_filter","Search effect or source",filter,sizeof(filter));std::string needle=filter;
    std::transform(needle.begin(),needle.end(),needle.begin(),[](unsigned char c){return (char)std::tolower(c);});
    for(const auto& entry:ability_data::entries){auto* proof=cleanse_data::find(entry.token);if(!entry.enemy||entry.passive||(!proof&&!observed.count(entry.token)))continue;
        std::string label=std::string(entry.title)+(proof?std::string(" (")+proof->effects+")":"");std::string lower=label;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return (char)std::tolower(c);});if(!needle.empty()&&lower.find(needle)==std::string::npos)continue;
        std::string key="cleanse.watch."+std::to_string(entry.token);auto* setting=cfg.find(key.c_str());if(!setting)continue;
        ImGui::PushID((int)entry.token);ImGui::BeginGroup();if(ImGui::Checkbox("##selected",&setting->b))cfg.mark_dirty();ImGui::SameLine(0,7);ImGui::TextWrapped("%s",label.c_str());ImGui::EndGroup();if(ImGui::IsItemHovered())ImGui::SetTooltip("Use a configured cleanse only while this source has an active removable enemy effect on you.");ImGui::PopID();
    }
}
static void automatic_usage(const autouse::Rule& rule){
    auto prefix=autouse::prefix(rule.token);section(rule.title);draw_item((prefix+"enabled").c_str(),"");output_binding((prefix+"key").c_str());input_binding_help();
    if(rule.kind==autouse::Cleanse){if(rule.upgrade_bits)ImGui::Text("Requires ability upgrade tier %d",rule.upgrade_bits==8?3:2);ImGui::TextWrapped("Requires ownership, readiness and a selected removable debuff.");return;}
    if(rule.kind==autouse::Targeted){
        items({(prefix+"health").c_str(),(prefix+"range").c_str(),(prefix+"soul_lead_only").c_str()});if(cfg.B((prefix+"soul_lead_only").c_str()))draw_item((prefix+"soul_lead").c_str(),"");
        if(std::string(rule.key)=="ability_ult_combo"){
            items({(prefix+"friends").c_str(),(prefix+"friend_range").c_str()});
            ImGui::TextWrapped("Combo requires a visible enemy under your crosshair within its effective range and the selected number of living teammates close to that enemy. Greater Expansion and a confirmed Mystic Expansion imbue extend the counted range. Other range modifiers are not yet read. Requires a working quick-cast binding. Damage-based execute is not implemented.");
        }else ImGui::TextWrapped("Requires the selected hero under your crosshair, line of sight, item readiness and an in-game quick-cast binding. No aiming or memory write is performed.");
        draw_item((prefix+"all_heroes").c_str(),"");if(!cfg.B((prefix+"all_heroes").c_str())){ImGui::TextWrapped("No selected heroes means no automatic casts.");section("SELECT TARGET HEROES");ImGui::BeginChild("##hero_whitelist",{0,150},true);for(const auto& hero:current_heroes)draw_item((prefix+"hero."+std::to_string(hero.id)).c_str(),"Only explicitly selected heroes are eligible.");ImGui::EndChild();}
        if(std::string(rule.key)!="ability_ult_combo")draw_item((prefix+"movement_ready").c_str(),"");if(std::string(rule.key)!="ability_ult_combo"&&cfg.B((prefix+"movement_ready").c_str())){section("MOVEMENT MUST BE READY");ImGui::TextWrapped("Use only if at least one checked movement ability is ready. Skip targets while all checked movement abilities are on cooldown.");ImGui::BeginChild("##movement_ready",{0,180},true);for(const auto& hero:current_heroes){bool heading=false;for(const auto& a:ability_data::entries)if(a.hero==hero.id&&target_rules::movement(a.token)){if(!heading){ImGui::TextUnformatted(hero.name);heading=true;}draw_item((prefix+"watch."+std::to_string(a.token)).c_str(),"Requires fresh, valid ability data and ready state.");}}ImGui::EndChild();}return;
    }
    items({(prefix+"health").c_str(),(prefix+"range").c_str(),(prefix+"enemies").c_str(),(prefix+"friends").c_str(),(prefix+"friend_range").c_str()});
    if(rule.kind==autouse::Area)ImGui::TextWrapped("Hit counting starts from the asset base radius (%.1f m) and includes known expansion items. The range slider can cap it. Other live range modifiers are not yet read.",world_units::metres(rule.radius));
}
static ImVec2 keybind_pos{0,0},keybind_size{0,0};static bool keybind_visible=false;
static void keybind_hud(bool editing){
    if(!cfg.B("hud.keybinds"))return;Entity local;{std::lock_guard<std::mutex> lock(snap_mtx);local=snap.local;}profile_scope::ActiveScope hero(local);
    ImGui::SetNextWindowPos({18,68},ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags=ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar;
    if(!editing)flags|=ImGuiWindowFlags_NoInputs;
    ImGui::Begin("Keybinds",nullptr,flags);keybind_pos=ImGui::GetWindowPos();keybind_size=ImGui::GetWindowSize();keybind_visible=true;
    auto row=[&](const char* title,const char* key,int mode,bool enabled,bool active){if(!enabled)return;bool held=false,known=game_key_state(cfg.I(key),held);ImGui::TextColored(active?ImVec4{.35f,1,.5f,1}:ImVec4{.65f,.65f,.72f,1},"%s  |  %s  |  %s  |  %s",title,key_name(cfg.I(key)),mode==1?"Toggle":mode==2?"Always":"Hold",!known?"Input unknown":active?"ACTIVE":held?(std::string(key)=="aim.key"?aim::wait_reason.c_str():"Held / waiting"):"Idle");};
    row("Aim","aim.key",cfg.I("aim.mode"),cfg.B("aim.enabled"),aim::active);row("Trigger","trigger.key",cfg.I("trigger.activation"),cfg.B("trigger.enabled"),trig::is_armed());
    ImGui::End();
}
static void debug_overlay(u64 width,u64 height){
    if(cfg.B("dbg.ballistics")){Entity local;{std::lock_guard<std::mutex> lock(snap_mtx);local=snap.local;}const auto& shot=local.current_shot;char text[240];if(shot.valid)snprintf(text,sizeof(text),"%s | %.1f m/s | estimated flight %.0f ms",shot.name.c_str(),world_units::metres(shot.speed),aim::flight_seconds*1000);else snprintf(text,sizeof(text),"Projectile lead: %s",shot.reason.c_str());ImGui::GetForegroundDrawList()->AddText({20,220},aceui::white,text);}

    if(!cfg.B("dbg.targets")&&!cfg.B("dbg.rays")&&!cfg.B("dbg.timing"))return;
    Entity local;Vec3 camera;bool camera_valid=false;std::vector<Entity> enemies;{std::lock_guard<std::mutex> lock(snap_mtx);local=snap.local;camera=snap.camera;camera_valid=snap.camera_valid;enemies=snap.enemies;}
    auto* draw=ImGui::GetForegroundDrawList();
    for(auto& enemy:enemies){auto point=world_to_screen(enemy.pos+Vec3{0,0,90},width,height);if(!aim_model::on_screen(point,(float)width,(float)height))continue;
        const char* reason=(snap.backend_managed&&!enemy.alive_valid)?"Life/current HP unknown":!enemy.alive?"Dead":enemy.bullet_invuln?"Bullet immune / Metal Skin":enemy.invuln?"Invulnerable":!enemy.occlusion_valid?"Occlusion unknown":!enemy.visible?"No visible shot":local.disarmed?"Local disarmed":local.reloading?"Reloading":"";
        if(cfg.B("dbg.targets")&&*reason)draw->AddText({point.x,point.y},IM_COL32(255,160,80,255),reason);
        if(cfg.B("dbg.rays")&&camera_valid){
            for(int region=0;region<2;++region){Vec3 to;if(!native_target::point(enemy,region,to,snap.native_binding_epoch.load(),mem.attachment_epoch(),now_ms()))continue;
                const auto projected=world_to_screen(to,width,height);if(!aim_model::on_screen(projected,(float)width,(float)height))continue;
                auto ray=collision::trace_any(camera,to,collision::Vision|collision::Bullet);
                auto end=world_to_screen(ray.known?ray.hit:to,width,height);if(!aim_model::on_screen(end,(float)width,(float)height))continue;
                const auto color=!ray.known?IM_COL32(255,180,80,255):ray.clear?IM_COL32(90,220,150,255):IM_COL32(250,80,90,255);
                draw->AddLine({width*.5f,height*.5f},{end.x,end.y},color,1.5f);draw->AddCircleFilled({end.x,end.y},3,color);
            }
        }
    }
    if(cfg.B("dbg.timing")){char text[180];snprintf(text,sizeof(text),"Reader %d Hz | poll %llu ms | publish %llu ms | last scan %llu ms | age %llu ms",snap.read_hz.load(),(unsigned long long)snap.poll_ms.load(),(unsigned long long)snap.publish_ms.load(),(unsigned long long)snap.scan_ms.load(),(unsigned long long)(now_ms()-snap.last_read_ms.load()));draw->AddText({18,42},IM_COL32(235,235,245,255),text);}
}
// Keep the software cap independent of the process-wide timer tick.
struct RenderTimings{double work_ms=0,present_ms=0,wait_ms=0;HRESULT present_result=S_OK;};
static RenderTimings render_timings;
class FramePacer{
    HANDLE timer_=nullptr;bool warned_=false;
public:
    FramePacer(){
        // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION (Windows 10 1803+).
        timer_=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_MODIFY_STATE|SYNCHRONIZE);
        if(timer_)plat_log("Renderer frame limiter: high-resolution waitable timer");
        else{
            DWORD high_resolution_error=GetLastError();
            timer_=CreateWaitableTimerExW(nullptr,nullptr,0,TIMER_MODIFY_STATE|SYNCHRONIZE);
            plat_log("Renderer frame limiter: high-resolution timer unavailable (error %lu); %s",(unsigned long)high_resolution_error,timer_?"standard waitable timer":"thread sleep fallback");
        }
    }
    ~FramePacer(){if(timer_)CloseHandle(timer_);}
    void wait_until(std::chrono::steady_clock::time_point deadline){
        auto remaining=deadline-std::chrono::steady_clock::now();
        if(remaining<=std::chrono::steady_clock::duration::zero())return;
        if(timer_){
            LARGE_INTEGER due{};due.QuadPart=-std::max<LONGLONG>(1,std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count()/100);
            DWORD timeout=(DWORD)std::clamp<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count()+50,50,250);
            DWORD wait_result=WAIT_FAILED,wait_error=0;
            if(SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE)){wait_result=WaitForSingleObject(timer_,timeout);if(wait_result==WAIT_OBJECT_0)return;if(wait_result==WAIT_FAILED)wait_error=GetLastError();}
            else wait_error=GetLastError();
            if(!warned_){warned_=true;plat_log("Renderer frame limiter: arm/wait failed (wait %lu, error %lu); using thread sleep",(unsigned long)wait_result,(unsigned long)wait_error);}
        }
        if(std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_until(deadline);
    }
};
static bool make_overlay_swap_chain(HWND hwnd,int& W,int& H,IDXGISwapChain** swap,ID3D11Device** dev,ID3D11DeviceContext** ctx){
    static const D3D_FEATURE_LEVEL FLV[]={
        D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,D3D_FEATURE_LEVEL_9_3,D3D_FEATURE_LEVEL_9_1};
    RECT client{};GetClientRect(hwnd,&client);
    int w=client.right-client.left,h=client.bottom-client.top;
    if(w<8) w=1920; if(h<8) h=1080;
    W=w; H=h;
    plat_log("Overlay: creating device...");
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount=2;
    scd.BufferDesc.Width=w; scd.BufferDesc.Height=h;
    scd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator=0; scd.BufferDesc.RefreshRate.Denominator=0;
    scd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; scd.OutputWindow=hwnd;
    scd.SampleDesc.Count=1; scd.SampleDesc.Quality=0;
    scd.Windowed=TRUE; scd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD; scd.Flags=0;
    HRESULT hr=E_FAIL; D3D_FEATURE_LEVEL fl{};
    hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,
        FLV,6,D3D11_SDK_VERSION,&scd,swap,dev,&fl,ctx);
    if(FAILED(hr)){
        plat_log("Overlay: hardware failed (0x%08X) - trying WARP",(unsigned)hr);
        hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
            FLV,6,D3D11_SDK_VERSION,&scd,swap,dev,&fl,ctx);
    }
    if(FAILED(hr)){
        char msg[160]; snprintf(msg,sizeof(msg),"ACE: overlay graphics failed (0x%08X)",(unsigned)hr);
        MessageBoxA(nullptr,msg,"ACE",MB_OK|MB_ICONERROR);
        return false;
    }
    plat_log("Overlay: D3D OK (Flvl 0x%04X)",(unsigned)fl);
    return true;
}
static std::atomic<bool> dma_thread_run{false},dma_restart_requested{false};
static void dma_worker(){
    auto failed=[](const char* detail){
        dma_thread_run=false;
        {std::lock_guard<std::mutex> lock(snap_mtx);snap.backend_status=std::string("Attachment worker stopped: ")+detail;}
        plat_log("Attachment worker stopped: %s",detail);
    };
    try{
    if(!dma_thread_run)return;
    for(const char* name:{"vmm.dll","leechcore.dll","ftd3xx.dll"})plat_log("Runtime file %s: %s",name,GetFileAttributesA(name)==INVALID_FILE_ATTRIBUTES?"MISSING":"present");
    plat_stage("attach","waiting for game");
    // A slow or missing input device must not delay an already validated reader.
    if(off.resolved)entity_thread_start(); // An existing validated attachment may resume; menu launch alone never sets this flag.
    if(cfg.B("input.autoconnect")&&cfg.I("input.mode")!=2&&!inp.device_ok)hw::init();
    u64 last_try=0;
    while(dma_thread_run){
        if(dma_restart_requested.exchange(false)){plat_stage("attach","stopping reader for DMA restart");entity_thread_stop();off.invalidate("DMA restart requested");live_backend::live.reset();mem.disconnect();last_try=0;plat_log("Recovery: DMA stopped; reconnecting");}
        if(!off.resolved){
            if(entity_reader_active())entity_thread_stop();
            if(consume_attach_recovery())last_try=0;
            if(now_ms()-last_try>5000){
                last_try=now_ms();
                if(!mem.ready()){
                    if(mem.init_vmm())plat_log("Overlay: DMA card connected");
                    else{std::lock_guard<std::mutex> lock(snap_mtx);snap.backend_status=mem.status.text;}
                }
                if(mem.ready()&&dma_thread_run){
                    consume_attach_recovery();bool matched=off.resolve(games::selected);auto status=off.status_text();
                    {std::lock_guard<std::mutex> lock(snap_mtx);snap.backend_status=status;}
                    if(matched&&dma_thread_run){toast("Game layout validated. Waiting for live data.",{0.3f,1,0.5f,1});entity_thread_start();}
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    }catch(const std::exception& error){failed(error.what());}
    catch(...){failed("unknown exception; see Log");}
}

int run(bool preview){
    struct TimerResolution{
        bool active=false;TimerResolution(){MMRESULT result=timeBeginPeriod(1);active=result==TIMERR_NOERROR;plat_log("Renderer timer resolution: %s (result %u)",active?"1 ms requested":"request failed",(unsigned)result);}
        ~TimerResolution(){if(active)timeEndPeriod(1);}
    } timer_resolution;
    FramePacer frame_pacer;
    preview_session=preview;edit_game=games::selected->name;active_slot=selected_profile="default";
    profile_game_states.clear();Settings::context.clear();Settings::visual_team=false;
    cfg=Settings{};cfg.register_all();protected_profile_ids.clear();pending_profile_name.clear();profile_message.clear();load_startup_profile();
    // ACE_LAUNCHER_WAIT_MENU_V1: publish waiting text before the worker exists;
    // keep all attachment/readiness flags under their existing backend owners.
    if(!preview){snap.backend_managed=true;if(!off.resolved){std::lock_guard<std::mutex> lock(snap_mtx);snap.backend_status="Waiting for game; background attachment has not completed";}}
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(WNDCLASSEXW);
    wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=WndProc;
    wc.hInstance=GetModuleHandleW(nullptr);
    wc.lpszClassName=L"ACEOverlay";
    wc.hCursor=LoadCursorA(nullptr,IDC_ARROW);
    HICON ic=ensure_ace_icon();
    if(ic){ wc.hIcon=ic; wc.hIconSm=ic; }
    if(!RegisterClassExW(&wc)){ MessageBoxA(nullptr,"ACE: overlay window class failed","ACE",MB_OK|MB_ICONERROR); return 1; }
    DWORD ex=preview?0:WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TOOLWINDOW;
    DWORD style=preview?WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX:WS_POPUP;
    int screenw=GetSystemMetrics(SM_CXSCREEN),screenh=GetSystemMetrics(SM_CYSCREEN);
    int ww=preview?(int)std::fmin(1160.f,(float)screenw-32):screenw;
    int wh=preview?(int)std::fmin(840.f,(float)screenh-64):screenh;
    if(!preview&&cfg.B("system.custom_resolution")){ww=cfg.I("system.width");wh=cfg.I("system.height");}
    HWND hwnd=CreateWindowExW(ex,wc.lpszClassName,preview?L"AceSoftware - Menu Preview":L"AceSoftware",
        style,(screenw-ww)/2,(screenh-wh)/2,ww,wh,nullptr,nullptr,wc.hInstance,nullptr);
    if(!hwnd){ MessageBoxA(nullptr,"ACE: could not create overlay window","ACE",MB_OK|MB_ICONERROR); return 1; }
    if(!preview)SetLayeredWindowAttributes(hwnd,RGB(0,0,0),255,LWA_ALPHA);
    MARGINS margins{-1,-1,-1,-1};
    if(!preview)DwmExtendFrameIntoClientArea(hwnd,&margins);
    ShowWindow(hwnd,SW_SHOW);
    UpdateWindow(hwnd);
    int W=0, H=0;
    IDXGISwapChain* swap=nullptr; ID3D11Device* dev=nullptr; ID3D11DeviceContext* ctx=nullptr;
    if(!make_overlay_swap_chain(hwnd,W,H,&swap,&dev,&ctx)) return 1;
    ID3D11RenderTargetView* rtv=nullptr; ID3D11Texture2D* back=nullptr;
    swap->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&back);
    if(!back||FAILED(dev->CreateRenderTargetView(back,nullptr,&rtv))){
        MessageBoxA(nullptr,"ACE: overlay render target failed","ACE",MB_OK|MB_ICONERROR); return 1; }
    back->Release();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    load_font();
    ace_style();
    ImGui_ImplWin32_Init(hwnd);
    RAWINPUTDEVICE mouse{0x01,0x02,RIDEV_INPUTSINK,hwnd};
    RegisterRawInputDevices(&mouse,1,sizeof(mouse));
    ImGui_ImplDX11_Init(dev,ctx);
    HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);ui_device=dev;refresh_profiles();game_model::initialize(dev,ctx);ability_icons::initialize(dev);
    cooperative_capture::Service framebuffer_capture;std::uint64_t capture_frame_id=0;
    if(!preview&&!framebuffer_capture.initialize(build_info::id,[](const char* message){plat_log("%s",message);}))plat_log("Cooperative capture initialization unavailable");
    ctx->OMSetRenderTargets(1,&rtv,nullptr);
    D3D11_VIEWPORT vp{0,0,(FLOAT)W,(FLOAT)H,0.f,1.f};
    ctx->RSSetViewports(1,&vp);
    bool menu_open=true, done=false;
    u64 splash_started=now_ms();
    int tab=0,aim_page=0,trigger_page=0,visual_page=0,settings_page=0;std::string input_test_status;int applied_mode=-1;
    ImVec2 menu_pos{0,0}, menu_size{0,0};
    plat_log("Overlay started. Menu open by default. Menu key: Left Ctrl. Panic: End.");
    dma_thread_run=!preview;
    std::thread dma_w;
    if(!preview){
        try{dma_w=std::thread(dma_worker);}
        catch(const std::exception& error){
            dma_thread_run=false;{std::lock_guard<std::mutex> lock(snap_mtx);snap.backend_status="Attachment worker could not start; close ACE and retry";}
            plat_log("Attachment worker could not start: %s",error.what());
        }
    }
    while(!done){
        auto frame_begin=std::chrono::steady_clock::now();
        MSG msg;
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT){ done=true; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        if(done) break;
        if(!preview){
            int mode=cfg.I("system.overlay_mode");if(mode!=applied_mode){LONG_PTR extended=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);if(mode==0)extended|=WS_EX_LAYERED;else extended&=~WS_EX_LAYERED;SetWindowLongPtrW(hwnd,GWL_EXSTYLE,extended);if(mode==0)SetLayeredWindowAttributes(hwnd,RGB(0,0,0),255,LWA_ALPHA);MARGINS margin=mode==0?MARGINS{-1,-1,-1,-1}:MARGINS{0,0,0,0};DwmExtendFrameIntoClientArea(hwnd,&margin);applied_mode=mode;}
            int desired_w=cfg.B("system.custom_resolution")?cfg.I("system.width"):screenw,desired_h=cfg.B("system.custom_resolution")?cfg.I("system.height"):screenh;
            if(desired_w!=W||desired_h!=H){ctx->OMSetRenderTargets(0,nullptr,nullptr);rtv->Release();rtv=nullptr;SetWindowPos(hwnd,HWND_TOPMOST,0,0,desired_w,desired_h,SWP_NOACTIVATE);HRESULT resized=swap->ResizeBuffers(0,desired_w,desired_h,DXGI_FORMAT_UNKNOWN,0);ID3D11Texture2D* buffer=nullptr;if(FAILED(resized)||FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&buffer)))||FAILED(dev->CreateRenderTargetView(buffer,nullptr,&rtv))){if(buffer)buffer->Release();toast("Display resize failed");done=true;break;}buffer->Release();W=desired_w;H=desired_h;ctx->OMSetRenderTargets(1,&rtv,nullptr);D3D11_VIEWPORT viewport{0,0,(FLOAT)W,(FLOAT)H,0,1};ctx->RSSetViewports(1,&viewport);}
        }
        ImGui::GetIO().FontGlobalScale=cfg.F("ui.font_size")/15.f;
        if(shortcut_block_key&&!(GetAsyncKeyState(shortcut_block_key)&0x8000))shortcut_block_key=0;
        bool mk=(GetAsyncKeyState(cfg.I("system.menu_key"))&0x8000)!=0||(cfg.B("system.game_menu_hotkey")&&game_key_down(cfg.I("system.menu_key")));
        static bool mk_last=false;
        if(listening_key.empty()&&!shortcut_block_key&&!ImGui::GetIO().WantTextInput&&mk&&!mk_last){menu_open=!menu_open;if(menu_open&&!preview){SetWindowPos(hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);SetForegroundWindow(hwnd);}}
        mk_last=mk;
        bool pk=game_key_down(cfg.I("system.panic_key"))||(GetAsyncKeyState(cfg.I("system.panic_key"))&0x8000)!=0;
        static bool pk_last=false;
        if(listening_key.empty()&&!shortcut_block_key&&pk&&!pk_last) inp.panic=!inp.panic;
        pk_last=pk;
        if(!preview&&(GetAsyncKeyState(cfg.I("input.reconnect_key"))&0x8000)&&!inp.device_ok){
            static u64 t=0; if(now_ms()-t>2000){ t=now_ms(); hw::reconnect(); }
        }
        bool refresh_key=game_key_down(cfg.I("system.recache_key"))||(GetAsyncKeyState(cfg.I("system.recache_key"))&0x8000);static bool refresh_previous=false;if(!preview&&refresh_key&&!refresh_previous&&listening_key.empty())request_recache();refresh_previous=refresh_key;
        plat_stage("renderer","updating frame",false);
        // Normalize loaded/imported display combinations before any autosave.
        {Settings::WriteScope settings_edit(cfg);if(cfg.B("system.fps_uncapped"))if(auto* sync=cfg.find("system.overlay_vsync"))if(sync->b){sync->b=false;cfg.mark_dirty();}}
        hw::update_diagnostics();snap.overlay_menu=menu_open;
        if(!preview){
        aim::tick((u64)W,(u64)H);
        trig::tick((u64)W,(u64)H);
        helpers::tick((u64)W,(u64)H);
        autosave_tick();
        static u64 last_action_report=0;if(now_ms()-last_action_report>5000){last_action_report=now_ms();char target_state[512];{std::lock_guard<std::mutex> lock(snap_mtx);unsigned alive=0,visible=0,occlusion=0,immune=0;for(auto& e:snap.enemies){alive+=e.alive;visible+=e.visible;occlusion+=e.occlusion_valid;immune+=e.bullet_invuln||e.invuln;}snprintf(target_state,sizeof(target_state),"Target state: total=%u alive=%u visible=%u occlusion_known=%u immune=%u | local valid=%d alive=%d reloading=%d disarmed=%d weapon_valid=%d",(unsigned)snap.enemies.size(),alive,visible,occlusion,immune,snap.local.valid,snap.local.alive,snap.local.reloading,snap.local.disarmed,snap.local.weapon_valid);}plat_log("%s",target_state);plat_log("DMA state: %s",mem.diagnostics().c_str());plat_log("Render state: %.1f FPS, cap=%d (0=uncapped), VSync=%s, mode=%d",ImGui::GetIO().Framerate,cfg.B("system.fps_uncapped")?0:cfg.I("system.max_fps"),(!cfg.B("system.fps_uncapped")&&cfg.B("system.overlay_vsync"))?"on":"off",cfg.I("system.overlay_mode"));plat_log("Render timing: work %.2f ms, Present %.2f ms, limiter %.2f ms, Present HRESULT 0x%08X",render_timings.work_ms,render_timings.present_ms,render_timings.wait_ms,(unsigned)render_timings.present_result);plat_log("Action state: aim=%s target_index=%d trigger=%s output_gate=%s | %s | prediction=%s aim_reason=%s",aim::active?"active":"idle",aim::target_idx,trig::is_armed()?"armed":"idle",game_output_reason().c_str(),hw::diagnostic_summary().c_str(),aim::prediction_status.c_str(),aim::wait_reason.c_str());}
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        cfg.context.clear();Settings::visual_team=false;
        aceui::theme(ui_color("ui.accent"),ui_color("ui.background"),ui_color("ui.panel"),ui_color("ui.text"));
        ImGui::NewFrame();
        // All menu widgets still submit normally; only owned draw lists are filtered below.
        // The real menu bool, snap.overlay_menu, input gates and input stay unchanged.
        const bool capture_overlay_only=!preview&&framebuffer_capture.prepare_frame(menu_open);
        ImDrawList* dl=ImGui::GetBackgroundDrawList();
        if(!preview&&!inp.panic){
            char wm[128];
            snprintf(wm,128,"AceSoftware | %s | render %.0f fps | reader %.1f Hz",
                g_ace_user,ImGui::GetIO().Framerate,snap.read_hz_exact.load());
            ImVec2 tsz=ImGui::CalcTextSize(wm);
            dl->AddRectFilled({(float)W-tsz.x-26.f,8.f},{(float)W-8.f,tsz.y+16.f},IM_COL32(10,10,14,180),4.f);
            dl->AddText({(float)W-tsz.x-17.f,12.f},aceui::purple,wm);
        }
        if(!inp.panic&&(bool)snap.in_match){
            esp::draw((u64)W,(u64)H);
        }
        if(!preview&&!inp.panic){
            profile_subject::Subject actual_subject;{std::lock_guard<std::mutex> guard(snap_mtx);actual_subject=snap.local.settings_subject;}profile_scope::ActiveScope current_hero(actual_subject);
            if(cfg.B("aim.deadzone_draw")&&cfg.B("aim.enabled")){
                float zone=aim_model::deadzone_radius_pixels((float)W,aim_model::capped_deadzone_fov(cfg.F("aim.deadzone_fov"),cfg.F("aim.fov")));
                ImVec2 center{W*.5f,H*.5f};
                if(zone>0)dl->AddCircle(center,zone,IM_COL32(240,195,95,180),64,1.f);
            }
            if(cfg.B("aim.fov_circle")&&cfg.B("aim.enabled")){
            float r=aim_model::fov_radius((float)W,cfg.F("aim.fov"));
            dl->AddCircle({(float)W/2.f,(float)H/2.f},r,IM_COL32(141,123,250,80),96,1.f);
            }
        }
        if(!inp.panic){
            float ty=H*0.28f;
            std::lock_guard<std::mutex> lk(log_mtx());
            for(auto&t:toast_queue()){
                u64 age=now_ms()-t.t; if(age>3000) continue;
                dl->AddText({W/2.f-90.f,ty},IM_COL32((int)(t.c.r*255),(int)(t.c.g*255),(int)(t.c.b*255),255),t.text.c_str());
                ty+=20.f;
            }
        }
        keybind_visible=false;if(!preview&&!inp.panic){keybind_hud(menu_open);debug_overlay((u64)W,(u64)H);}
        if(menu_open){
            if(tab==3)tab=0; // Retired Heroes route; keep other page IDs stable.
            const auto menu_dimensions=menu_layout::size(static_cast<float>(W),static_cast<float>(H),cfg.F("ui.menu_size"),preview);
            const float mw=menu_dimensions.width,mh=menu_dimensions.height;
            ImGui::SetNextWindowSize({mw,mh});
            ImVec2 wanted_position{(W-mw)*.5f,(H-mh)*.5f};
            if(!preview)if(const auto* existing=ImGui::FindWindowByName("##AceSoftware"))wanted_position=existing->Pos;
            const auto position=menu_layout::origin(wanted_position.x,wanted_position.y,menu_dimensions,static_cast<float>(W),static_cast<float>(H));
            ImGui::SetNextWindowPos({position.x,position.y},ImGuiCond_Always);
            ImGui::Begin("##AceSoftware",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoBringToFrontOnFocus);
            auto p=ImGui::GetWindowPos();menu_pos=p;menu_size=ImGui::GetWindowSize();
            auto* d=ImGui::GetWindowDrawList();const float rail=mw<760.f?132.f:156.f,gap=12.f,cw=std::max(1.f,mw-rail-32.f),content_x=rail+16.f;
            const auto background=ImGui::GetStyleColorVec4(ImGuiCol_WindowBg),panel_color=ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
            const ImVec4 rail_color{background.x*.82f+panel_color.x*.18f,background.y*.82f+panel_color.y*.18f,background.z*.82f+panel_color.z*.18f,1};
            d->AddRectFilled(p,{p.x+rail,p.y+mh},ImGui::ColorConvertFloat4ToU32(rail_color),4,ImDrawFlags_RoundCornersLeft);
            d->AddLine({p.x+rail,p.y+10},{p.x+rail,p.y+mh-10},ImGui::GetColorU32(ImGuiCol_Border));
            aceui::logo(d,{p.x+14,p.y+18},24);d->PushClipRect({p.x+45,p.y+12},{p.x+rail-10,p.y+49},true);aceui::text(d,aceui::small,{p.x+45,p.y+23},aceui::white,"AceSoftware");d->PopClipRect();
            const char* names[]={"Aimbot","Triggerbot","Visuals","","Radar","Config","Settings","Misc","World","Projectiles"};
            ImGui::SetCursorPos({10,64});ImGui::BeginChild("##navigation",{rail-20,std::max(1.f,mh-176.f)},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
            auto navigate_rail=[&](int page){if(aceui::nav(page,names[page],"",tab==page,ImGui::GetContentRegionAvail().x)){tab=page;listening_key.clear();menu_search_reset_scroll=true;}};
            aceui::caption("COMBAT");ImGui::Dummy({0,3});navigate_rail(0);navigate_rail(1);ImGui::Dummy({0,10});
            aceui::caption("VISUALS");ImGui::Dummy({0,3});navigate_rail(2);navigate_rail(8);navigate_rail(9);navigate_rail(4);ImGui::Dummy({0,10});
            aceui::caption("SYSTEM");ImGui::Dummy({0,3});navigate_rail(6);navigate_rail(7);navigate_rail(5);ImGui::EndChild();
            d->AddLine({p.x+12,p.y+mh-104},{p.x+rail-12,p.y+mh-104},ImGui::GetColorU32(ImGuiCol_Border));
            aceui::text(d,aceui::small,{p.x+14,p.y+mh-94},aceui::muted,"ACTIVE PROFILE");
            d->PushClipRect({p.x+14,p.y+mh-79},{p.x+rail-12,p.y+mh-38},true);aceui::text(d,aceui::body,{p.x+14,p.y+mh-78},aceui::white,active_slot.c_str());aceui::text(d,aceui::small,{p.x+14,p.y+mh-57},aceui::muted,edit_game.c_str());d->PopClipRect();
            ImGui::SetCursorPos({14,mh-31});if(ImGui::SmallButton("Log")){SetWindowPos(hwnd,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);plat_show_console();}ImGui::SameLine();if(ImGui::SmallButton("Exit"))done=true;
            const char* visual_tabs[]={"Enemies","Teammates","Minions"};
            const char* settings_tabs[]={"Display","Input","Hotkeys","Appearance","DMA"};
            // Title/search and subnavigation have separate, measured rows.
            const bool stacked_header=cw<400.f;
            const float search_width=stacked_header?cw:std::min(236.f,cw*.40f);
            const float title_width=stacked_header?cw:cw-search_width-16.f;
            ImGui::SetCursorPos({content_x,12});ImGui::PushFont(aceui::heading);ImGui::PushTextWrapPos(content_x+title_width);ImGui::TextUnformatted(names[tab]);ImGui::PopTextWrapPos();ImGui::PopFont();
            // Reuse the existing subtitle row; clip long status at large font
            // sizes without changing title/search/tab vertical geometry.
            ImGui::PushFont(aceui::small);std::string subtitle=edit_game;
            if(!preview){
                const auto phase=off.phase();const bool attached=off.resolved.load();
                subtitle=attached?(snap.frame_valid.load()?"Live data ready":"Waiting for live data"):
                    !dma_thread_run.load()?"Attachment worker unavailable":phase==AttachPhase::UpdateRequired?"Update required":
                    phase==AttachPhase::FindingModules||phase==AttachPhase::Validating?"Checking game":"Waiting for game";
            }
            const auto subtitle_pos=ImGui::GetCursorScreenPos();const float subtitle_height=ImGui::GetTextLineHeight();
            ImGui::InvisibleButton("##game_attachment_state",{title_width,subtitle_height});
            d->PushClipRect(subtitle_pos,{subtitle_pos.x+title_width,subtitle_pos.y+subtitle_height},true);
            aceui::text(d,aceui::small,subtitle_pos,ImGui::GetColorU32(ImGuiCol_TextDisabled),subtitle.c_str());d->PopClipRect();
            if(ImGui::IsItemHovered()){
                if(preview)ImGui::SetTooltip("%s",edit_game.c_str());
                else ImGui::SetTooltip("%s | %s\n%s\n%s",edit_game.c_str(),subtitle.c_str(),off.status_text().c_str(),backend_status().c_str());
            }
            ImGui::PopFont();
            const float title_bottom=ImGui::GetCursorPosY(),search_y=stacked_header?title_bottom+6.f:18.f;
            const float tabs_y=std::max(title_bottom,search_y+ImGui::GetFrameHeight())+8.f,tabs_width=cw;
            ImGui::SetCursorPos({stacked_header?content_x:mw-16-search_width,search_y});ImGui::SetNextItemWidth(search_width);
            static char setting_search[80]="";static bool setting_search_open=false;
            bool search_changed=ImGui::InputTextWithHint("##setting_search","Find a setting...",setting_search,sizeof(setting_search));
            bool search_active=ImGui::IsItemActive(),search_hovered=ImGui::IsItemHovered();
            ImVec2 search_min=ImGui::GetItemRectMin(),search_max=ImGui::GetItemRectMax();
            if(search_changed||search_active)setting_search_open=setting_search[0]!=0;
            if(ImGui::IsKeyPressed(ImGuiKey_Escape))setting_search_open=false;
            if(setting_search_open&&setting_search[0]){
                const float search_width=std::min(cw,440.f);ImGui::SetNextWindowPos({p.x+mw-16-search_width,search_max.y+5});
                ImGui::SetNextWindowSize({search_width,0});ImGui::SetNextWindowSizeConstraints({search_width,0},{search_width,330});
                // An ordinary no-focus window keeps the text field active as results change.
                ImGuiWindowFlags result_flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNavFocus;
                ImGui::Begin("##setting_search_results",nullptr,result_flags);
                bool results_hovered=ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);int matches=0;
                struct Destination{const char* label;int tab,page;};const Destination entries[]={{"Aimbot / activation and FOV",0,0},{"Aimbot / smoothing and curve",0,0},{"Aimbot / dead zone",0,0},{"Aimbot / target selection and delay",0,0},{"Triggerbot / activation and timing",1,0},{"Triggerbot / magnet aim",1,0},{"Visuals / master switch and enemies",2,0},{"Visuals / teammates",2,1},{"Visuals / minions",2,2},{"World / categories and style",8,0},{"Projectiles / rendering",9,0},{"Radar",4,0},{"Configs / save and load",5,0},{"Settings / display",6,0},{"Settings / input device tests",6,1},{"Settings / ACE menu pause",6,1},{"Settings / DMA status and recovery",6,4},{"Settings / hotkeys",6,2},{"Settings / menu colors and saving",6,3},{"Misc / live log",7,0}};
                const std::string query=menu_search_lower(setting_search);
                auto navigate=[&](int target,int page,const std::string& path){tab=target;if(tab==0)aim_page=page;if(tab==1)trigger_page=page;if(tab==2)visual_page=page;if(tab==6)settings_page=page;listening_key.clear();menu_search_target=path;menu_search_reset_scroll=true;setting_search[0]=0;setting_search_open=false;};
                auto destinations=menu_setting_destinations();
                std::stable_partition(destinations.begin(),destinations.end(),[&](const auto& entry){return menu_search_name_rank(entry.name_terms,query)==0;});
                for(const auto& entry:destinations){
                    if(entry.terms.find(query)==std::string::npos)continue;++matches;ImGui::PushID(entry.tab);ImGui::PushID(entry.page);ImGui::PushID(entry.path.c_str());
                    const auto start=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;const float height=std::max(ImGui::GetTextLineHeightWithSpacing(),ImGui::CalcTextSize(entry.label.c_str(),nullptr,false,width-12).y+6);
                    if(ImGui::Selectable("##setting",false,0,{width,height}))navigate(entry.tab,entry.page,entry.path);
                    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{start.x+4,start.y+3},ImGui::GetColorU32(ImGuiCol_Text),entry.label.c_str(),nullptr,width-12);
                    ImGui::PopID();ImGui::PopID();ImGui::PopID();
                }
                for(const auto& entry:entries){if(menu_search_lower(entry.label).find(query)==std::string::npos)continue;++matches;
                    if(ImGui::Selectable(entry.label))navigate(entry.tab,entry.page,"");
                }
                if(!matches)ImGui::TextDisabled("No matching sections");
                if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&!search_hovered&&!results_hovered)setting_search_open=false;
                ImGui::End();
            }
            ImGui::SetCursorPos({rail+16,tabs_y});
            if(tab==2)menu_tabs("##visual_tabs",visual_tabs,3,visual_page,tabs_width);
            else if(tab==6)menu_tabs("##settings_tabs",settings_tabs,5,settings_page,tabs_width);
            else if(tab==0||tab==1)scope_selector(cw);
            else ImGui::Dummy({cw,0});
            float body_y=ImGui::GetCursorPosY()+12.f;
            ImGui::SetCursorPos({rail+16,body_y});
            const char* page_title=tab==2?(visual_page==0?"Enemy ESP":visual_page==1?"Teammate ESP":"Minion ESP"):names[tab];
            const char* page_master=tab==0?"aim.enabled":tab==1?"trigger.enabled":tab==2?(visual_page<2?"esp.enabled":"minions.enabled"):nullptr;
            Settings::visual_team=tab==2&&visual_page==1;
            // Aim/Trigger enable switches live in their Activation groupbox;
            // their top row is reserved for the actual profile/category selectors.
            if(tab==2){menu_page_heading(page_title,page_master,cw,true);body_y=ImGui::GetCursorPosY()+6;}
            if(!profile_message.empty()&&(tab==0||tab==1)){
                ImGui::SetCursorPos({rail+16,body_y});
                const float notice_content=ImGui::CalcTextSize(profile_message.c_str(),nullptr,false,std::max(1.f,cw-ImGui::GetStyle().ScrollbarSize)).y;
                const float notice_limit=std::max(ImGui::GetTextLineHeight(),mh-body_y-56.f-72.f-6.f-ImGui::GetStyle().ItemSpacing.y);
                menu_notice(profile_message,cw,std::min(notice_content,notice_limit));body_y=ImGui::GetCursorPosY()+6;
            }
            Settings::visual_team=tab==2&&visual_page==1;
            const float ch=std::max(48.f,mh-body_y-56);
            const float left=tab==2?(cw-gap)*.55f:(cw-gap)*.5f,right=cw-left-gap;
            const bool scoped_frame=tab==0||tab==1;if(scoped_frame)ImGui::BeginDisabled(editing_inherited);
            const int route_page=tab==2?visual_page:tab==6?settings_page:0;ImGui::PushID(tab);ImGui::PushID(route_page);
            ImGui::SetCursorPos({rail+16,body_y});menu_column("##left",{left,ch},tab>=4&&tab<=7);

            if(tab==0){
                control_card("##aim_settings","ACTIVATION",[&]{items({"aim.enabled","aim.key","aim.mode","aim.fov","aim.fov_circle"});});
                control_card("##aim_targeting","TARGETING",[&]{items({"aim.target_players","aim.max_dist","aim.priority","aim.hard_lock","aim.visible_check","aim.skip_fast"});if(cfg.B("aim.skip_fast"))items({"aim.max_speed"});});
                control_card("##aim_global_filter","FILTERS",[&]{draw_item("target.team_check","Global filter for every hero in the active profile. Hero overrides do not change this filter.");});
            }else if(tab==1){
                control_card("##trigger_settings","ACTIVATION",[&]{items({"trigger.enabled","trigger.key","trigger.activation","trigger.prediction","trigger.hitchance_min","trigger.max_dist","trigger.skip_fast"});if(cfg.B("trigger.skip_fast"))items({"trigger.max_speed"});});
                control_card("##trigger_timing","FIRING TIMING",[&]{draw_pair("Trigger delay","trigger.first_shot_delay_min","trigger.first_shot_delay_max",600);draw_pair("Time between shots","trigger.followup_delay_min","trigger.followup_delay_max",400);});
            }else if(tab==2){
                // Shared display prerequisites appear once, below navigation,
                // in the scrolling column. Category pages edit only cls.*.
                control_card("##visual_global_display","GLOBAL DISPLAY",[&]{items({"visuals.enabled","world.enabled"});});
                if(visual_page<2){
                    control_card("##player_rendering","RENDERING",[&]{items({"esp.box","esp.health","esp.tracers","esp.head_circle"});});
                    control_card("##player_information","PLAYER INFORMATION",[&]{items({"esp.name","esp.souls","esp.distance"});});
                    control_card("##player_indicators","INDICATORS",[&]{items({"esp.ultimate","esp.abilities"});});
                    control_card("##player_range","RANGE",[&]{items({"esp.draw_distance"});});
                }else{
                    control_card("##minion_types","MINION TYPES",[&]{items({"minions.enemy","minions.friendly","minions.troopers","minions.neutral","minions.bosses"});});
                    control_card("##minion_rendering","RENDERING",[&]{items({"minions.box","minions.health","minions.name","minions.distance"});});
                }
            }else if(tab==8){world_category_list(ch);
            }else if(tab==9){
                control_card("##projectile_rendering","PROJECTILE RENDERING",[&]{items({"cls.projectiles"});});
                control_card("##projectile_elements","DISPLAY ELEMENTS",[&]{items({"projectiles.marker","projectiles.name","projectiles.distance"});});
            }else if(tab==4){
                section("WEB RADAR");ImGui::BeginDisabled(radar::running());draw_item("radar.listen","");draw_item("radar.port","Stop the radar before changing access or port.");ImGui::EndDisabled();
                ImGui::BeginDisabled(preview);
                if(!radar::running()){if(ImGui::Button("Start radar",{-1,38}))radar::start(cfg.I("radar.listen")==0);}
                else{ImGui::Text("Access PIN: %s",radar::current_pin());
                    if(ImGui::Button("Copy PIN",{-1,36}))clipboard_set(radar::current_pin());
                    std::string url="http://"+(radar::loopback_only()?std::string("127.0.0.1"):local_ip())+":"+std::to_string(radar::current_port())+"?pin="+radar::current_pin();
                    if(ImGui::Button("Copy radar link",{-1,36}))clipboard_set(url);
                    if(ImGui::Button("Stop radar",{-1,36}))radar::stop();}ImGui::EndDisabled();
            }else if(tab==5){
                section("Configs");
                for(auto& name:profile_names){if(ImGui::Selectable(name.c_str(),profile_lifecycle_policy::same_name(selected_profile,name)))selected_profile=name;if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\nSelects the target for Load, Save selected and Delete. It does not change the active profile.",name.c_str());}
                ImGui::Spacing();section("NEW PROFILE");ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##newprofile","Profile name",new_profile,sizeof(new_profile));
                if(ImGui::Button("Create",{-1,30})){
                    std::string name=new_profile;
                    if(!safe_profile(name))profile_message="Use 1-40 letters, numbers, - or _.";
                    else{std::error_code ec;const bool exists=std::filesystem::exists(profile_path(name),ec);
                        if(ec)profile_message="Could not inspect the profile path.";
                        else if(exists)profile_message="That profile already exists.";
                        else{std::string error;if(save_named_profile(name,&error)){selected_profile=name;new_profile[0]=0;refresh_profiles();profile_message="Created copy "+name+". Active profile remains "+active_slot;}else profile_message="Create failed: "+error;}
                    }
                }
            }else if(tab==6){
                if(settings_page==0){
                    section("DISPLAY");items({"system.overlay_mode","system.custom_resolution"});
                    if(cfg.B("system.custom_resolution"))items({"system.width","system.height"});
                    draw_item("system.fps_uncapped","");if(!cfg.B("system.fps_uncapped"))draw_item("system.max_fps","");ImGui::TextDisabled("Measured renderer: %.1f FPS",ImGui::GetIO().Framerate);
                    ImGui::BeginDisabled(cfg.B("system.fps_uncapped"));items({"system.overlay_vsync"});ImGui::EndDisabled();if(cfg.B("system.overlay_vsync")&&!cfg.B("system.fps_uncapped"))ImGui::TextWrapped("VSync limits FPS to the overlay display refresh rate.");section("MENU & TEXT SIZE");items({"ui.menu_size","ui.font_size","esp.font_size"});
                }else if(settings_page==1){
                    section("INPUT DEVICE");items({"input.mode","input.com","input.baud","input.autoconnect"});
                    ImGui::TextWrapped("%s",inp.device_text.c_str());
                    section("INPUT TIMING");items({"input.latency_ms","input.delay_ms"});
                    section("PAUSE AUTOMATIC INPUT");items({"input.disable_in_overlay","input.disable_when_dead","input.disable_in_scoreboard","input.disable_when_spectator"});
                    section("SENSITIVITY");items({"input.auto_sensitivity"});if(!cfg.B("input.auto_sensitivity"))items({"input.sensitivity"});
                    float sensitivity=0;bool local_valid=false;{std::lock_guard<std::mutex> lock(snap_mtx);sensitivity=snap.sensitivity;local_valid=snap.local.valid;}if(local_valid&&std::isfinite(sensitivity)&&sensitivity>0)ImGui::Text("Game sensitivity: %.3f",sensitivity);else ImGui::TextDisabled("Unavailable until local player data is valid");
                    if(ImGui::TreeNode("Advanced input calibration")){items({"input.yaw","input.pitch"});ImGui::TreePop();}
                }else if(settings_page==2){
                    section("HOTKEYS");items({"system.menu_key","system.game_menu_hotkey","system.panic_key","input.reconnect_key","system.recache_key","system.scoreboard_key"});
                }else if(settings_page==4){
                    section("DMA STATUS");auto hardware=mem.diagnostics();for(size_t pos=0;(pos=hardware.find(" | ",pos))!=std::string::npos;)hardware.replace(pos,3,"\n");ImGui::TextUnformatted(hardware.c_str());ImGui::TextWrapped("%s",backend_status().c_str());
                    section("READER TIMING");draw_item("reader.max_hz","");
                    ImGui::TextDisabled("Measured: %.2f Hz / poll: %llu ms",snap.read_hz_exact.load(),(unsigned long long)snap.poll_ms.load());
                    draw_item("reader.discovery_ms","");
                    ImGui::TextDisabled("Last scan: %llu ms / actual interval: %llu ms",(unsigned long long)snap.scan_ms.load(),(unsigned long long)snap.discovery_interval_ms.load());
                    u64 scan_completed=snap.discovery_completed_ms.load();if(scan_completed)ImGui::TextDisabled("Time since scan: %llu ms",(unsigned long long)(now_ms()-scan_completed));else ImGui::TextDisabled("No completed discovery scan");
                    ImGui::TextDisabled("Validated identities: %u / pawns: %u / controllers: %u",(unsigned)snap.discovery_accepted.load(),(unsigned)snap.discovery_pawns.load(),(unsigned)snap.discovery_controllers.load());
                    ImGui::TextDisabled("Discovery pages: %u read / %u unavailable",(unsigned)snap.discovery_pages_read.load(),(unsigned)snap.discovery_pages_unavailable.load());
                    draw_item("reader.world_ms","");ImGui::TextDisabled("Publish: %llu ms",(unsigned long long)snap.publish_ms.load());
                }else{
                    section("MENU APPEARANCE");items({"ui.accent","ui.background","ui.panel","ui.text","ui.splash"});
                    if(ImGui::Button("Reset menu theme",{-1,30})){Settings::WriteScope settings_edit(cfg);cfg.find("ui.accent")->c={.55f,.48f,.98f,1};cfg.find("ui.background")->c={.055f,.058f,.075f,1};cfg.find("ui.panel")->c={.078f,.083f,.105f,1};cfg.find("ui.text")->c={.92f,.92f,.96f,1};cfg.mark_dirty();}
                }
            }else{section("LIVE LOG");if(ImGui::Button("Show console")){SetWindowPos(hwnd,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);plat_show_console();}ImGui::SameLine();if(ImGui::Button("Copy log")){std::string text;for(auto& line:plat_recent_logs())text+=line+"\n";clipboard_set(text);}ImGui::BeginChild("##live_log",{0,0},true,ImGuiWindowFlags_HorizontalScrollbar);for(auto& line:plat_recent_logs())ImGui::TextUnformatted(line.c_str());if(ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-30)ImGui::SetScrollHereY(1.f);ImGui::EndChild();
            }
            ImGui::EndChild();menu_scroll_window=nullptr;ImGui::SameLine(0,gap);
            menu_column("##right",{right,ch},tab>=4&&tab<=7);
            if(tab==0){
                control_card("##aim_smoothing","SMOOTHING",[&]{items({"aim.smooth","aim.curve","aim.prediction"});});
                control_card("##aim_deadzone","DEAD ZONE",[&]{items({"aim.deadzone_fov","aim.deadzone_draw"});});
                control_card("##aim_timing","TIMING",[&]{items({"aim.start_delay_ms"});draw_pair("Reaction to new target","aim.act_delay_min","aim.act_delay_max",1000);items({"aim.retarget_ms","aim.visible_ms"});});
                control_card("##aim_regions","TARGET POINTS",[&]{aim_points(false);});
            }else if(tab==1){
                control_card("##magnet_settings","MAGNET AIM",[&]{items({"trigger.magnet","trigger.magnet_prediction","trigger.magnet_fov","trigger.magnet_max_dist","trigger.magnet_visible_ms"});});
                control_card("##magnet_smoothing","SMOOTHING",[&]{items({"trigger.magnet_smooth","trigger.magnet_curve","trigger.magnet_strength","trigger.magnet_deadzone_fov"});});
            }else if(tab==2){
                if(visual_page<2)control_card("##esp_layout","ESP PREVIEW",[&]{draw_esp_builder(false,std::clamp(ch-100.f,240.f,520.f));});
                else control_card("##minion_range","RANGE & COLORS",[&]{items({"minions.distance_limit","minions.color_enemy","minions.color_friendly"});});
            }else if(tab==8){world_category_details();
            }else if(tab==9){
                control_card("##projectile_style","PROJECTILE APPEARANCE",[&]{items({"esp.color_proj","projectiles.marker_size","projectiles.font_size","projectiles.max_dist"});});
            }else if(tab==4){
                section("CONNECTION");ImGui::TextWrapped("%s",radar::status_text().c_str());aceui::status(radar::running()?"Radar is running":"Radar is stopped",radar::running());
                ImGui::TextWrapped("Local network mode shares with devices on the same network. Sharing over the internet still requires reachable hosting or a relay; neither is configured.");
            }else if(tab==5){
                section("PROFILE ACTIONS");ImGui::TextWrapped("Selected: %s",selected_profile.c_str());
                if(ImGui::IsItemHovered())ImGui::SetTooltip("Target for Load, Save selected and Delete. Selecting a row does not load it.");
                ImGui::PushTextWrapPos(0);ImGui::TextDisabled("Active: %s",active_slot.c_str());ImGui::PopTextWrapPos();
                if(ImGui::IsItemHovered())ImGui::SetTooltip("Current settings belong to this profile. Automatic saving and Save settings now write this file.");
                if(preview_session)ImGui::TextWrapped("Preview: automatic saving and startup selection updates are off. Create and Save explicitly write profile files.");
                if(ImGui::Button("Load",{-1,30}))request_profile_load(selected_profile);
                if(ImGui::IsItemHovered())ImGui::SetTooltip("Load the selected profile. Successful loads become the startup profile for this game. Preview loads leave startup selection unchanged. Pending automatic saves must succeed first; otherwise choose how to handle unsaved edits.");
                if(ImGui::Button("Save selected",{-1,30})){std::string error;profile_message=save_named_profile(selected_profile,&error)?"Saved "+selected_profile+". Active profile remains "+active_slot:"Save failed: "+error;}
                if(ImGui::IsItemHovered())ImGui::SetTooltip("Write current settings to the selected file. Saving a copy preserves pending edits for the active profile.");
                if(ImGui::Button("Delete...",{-1,30}))ImGui::OpenPopup("Delete profile?");
                if(ImGui::IsItemHovered())ImGui::SetTooltip("Delete the selected file after confirmation. The active profile cannot be deleted, including a differently cased name for the same file.");
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{18,18});
                if(ImGui::BeginPopupModal("Delete profile?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
                    ImGui::Text("Delete %s?",selected_profile.c_str());
                    ImGui::BeginDisabled(!profile_lifecycle_policy::may_delete(selected_profile,active_slot));
                    if(ImGui::Button("Delete")){
                        std::error_code ec;bool removed=std::filesystem::remove(profile_path(selected_profile),ec);
                        if(removed&&!ec){protected_profile_ids.erase(profile_identity(selected_profile));profile_message="Profile deleted.";}else profile_message="Delete failed.";refresh_profiles();selected_profile=active_slot;ImGui::CloseCurrentPopup();
                    }ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
                }ImGui::PopStyleVar();
                if(ImGui::Button("Refresh",{-1,30}))refresh_profiles();
                if(ImGui::Button("Open config folder",{-1,30})){
                    auto path=std::filesystem::absolute("configs").wstring();ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
                }
                if(ImGui::Button("Copy share code",{-1,30}))clipboard_set(share_code_export());
                if(ImGui::IsItemHovered())ImGui::SetTooltip("Copy current in-memory settings, including unsaved edits. This does not load or export the selected file from disk.");
                if(!profile_message.empty())ImGui::TextWrapped("%s",profile_message.c_str());

            }else if(tab==6){
                if(settings_page==0){
                    section("LIVE PERFORMANCE");ImGui::Text("Renderer: %.1f FPS",ImGui::GetIO().Framerate);
                    ImGui::Text("Frame work: %.2f ms",render_timings.work_ms);ImGui::Text("Present: %.2f ms",render_timings.present_ms);ImGui::Text("Frame limiter: %.2f ms",render_timings.wait_ms);
                    ImGui::Text("Reader: %.2f Hz",snap.read_hz_exact.load());
                    ImGui::Text("Poll: %llu ms",(unsigned long long)snap.poll_ms.load());ImGui::Text("Publish: %llu ms",(unsigned long long)snap.publish_ms.load());ImGui::Text("Last scan: %llu ms",(unsigned long long)snap.scan_ms.load());
                    u64 last_read=snap.last_read_ms.load();if(last_read)ImGui::Text("Data age: %llu ms",(unsigned long long)(now_ms()-last_read));else ImGui::TextDisabled("No data received");

                }else if(settings_page==1){
                    section("DEVICE CAPABILITIES");ImGui::TextWrapped("%s",hw::physical_mouse_status().c_str());
                    ImGui::TextWrapped("Output: %s",inp.keyboard_ok?"Mouse and verified keyboard input":"Mouse buttons only");ImGui::TextWrapped("%s",hw::keyboard_status().c_str());
                    section("DEVICE TESTS");ImGui::BeginDisabled(preview);
                    if(ImGui::Button("Reconnect input device",{-1,36}))hw::reconnect();if(ImGui::Button("Disconnect input device",{-1,30}))hw::disconnect();
                    if(inp.keyboard_api){draw_item("input.test_key","Choose an unused key on the game PC.");if(ImGui::Button("Test keyboard delivery",{-1,30})){input_test_status=hw::test_keyboard();plat_log("Input test: %s",input_test_status.c_str());}}
                    if(ImGui::Button("Check input connection",{-1,30})){input_test_status=hw::test_connection();plat_log("Input test: %s",input_test_status.c_str());}
                    if(ImGui::Button("Test mouse movement",{-1,30})){input_test_status=hw::test_mouse();plat_log("Input test: %s",input_test_status.c_str());}
                    if(ImGui::Button("Test left mouse click",{-1,30})){input_test_status=hw::test_click();plat_log("Input test: %s",input_test_status.c_str());}
                    ImGui::EndDisabled();ImGui::TextWrapped("%s",input_test_status.c_str());
                    section("RAW UI DIAGNOSTIC");const unsigned ui=snap.raw_ui_flags.load(),input=snap.raw_input_enabled.load();
                    draw_item(action_pause_policy::diagnostic_key,"");
                    ImGui::TextWrapped("Diagnostic opt-in only. This byte does not establish game-menu visibility and may stay active during play.");
                    ImGui::Text("Raw UI byte: 0x%02X / read: %s",ui,snap.raw_ui_valid?"valid":"unavailable");
                    ImGui::Text("Bit 0: %u / Bit 1: %u / readable signal active: %s",ui&1,(ui>>1)&1,action_pause_policy::raw_signal_active(snap.raw_ui_valid.load(),ui)?"yes":"no");
                    ImGui::Text("Input byte: 0x%02X / read: %s",input,snap.raw_input_enabled_valid?"valid":"unavailable");
                    ImGui::TextDisabled("Legacy saved game-menu pause is ignored.");
                    ImGui::TextWrapped("Input gate: %s",game_output_reason().c_str());
                }else if(settings_page==2){
                    section("KEYBIND DISPLAY");items({"hud.keybinds"});
                }else if(settings_page==4){
                    section("RECOVERY");ImGui::BeginDisabled(preview);
                    if(ImGui::Button("Rescan entities",{-1,32}))request_entity_rescan();
                    if(ImGui::Button("Refresh memory caches",{-1,32}))request_recache();
                    if(ImGui::Button("Restart DMA connection",{-1,32})){dma_restart_requested=true;plat_log("Recovery: DMA restart queued; waiting for current vendor call");toast("DMA restart requested");}
                    ImGui::EndDisabled();ImGui::TextWrapped("%s",recache_status().c_str());section("MAP COLLISION");items({"system.map"});ImGui::TextWrapped("%s",map_detection_status().c_str());ImGui::TextWrapped("%s",collision::status().c_str());
                }else{
                    section("BUILD");ImGui::TextWrapped("%s",build_info::display);ImGui::TextWrapped("ID: %s",build_info::id);if(ImGui::Button("Copy build information",{-1,28}))clipboard_set(std::string(build_info::display)+"\nID: "+build_info::id+"\nUTC: "+build_info::utc);
                    section("SAVING");if(preview_session)ImGui::TextWrapped("Preview: automatic saving and startup selection updates are disabled. Save settings now explicitly writes the active profile.");ImGui::BeginDisabled(preview_session);items({"system.autosave"});ImGui::EndDisabled();
                    if(last_save_failed)ImGui::TextColored({1,.35f,.35f,1},"Save failed");else if(cfg.dirty)ImGui::TextDisabled("Unsaved changes");else if(last_save_time)ImGui::TextColored({.4f,.9f,.5f,1},"Settings saved");
                    if(ImGui::Button("Save settings now",{-1,30})){std::string error;profile_message=save_named_profile(active_slot,&error)?"Saved active profile "+active_slot:"Save failed: "+error;}
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Write current settings to the active profile, even in Preview. An unreadable profile file is protected; create a new profile instead.");
                    if(!profile_message.empty())ImGui::TextWrapped("%s",profile_message.c_str());
                }
            }else{section("DEBUG DRAWINGS");items({"dbg.targets","dbg.rays","dbg.timing","dbg.ballistics","dbg.action_log"});section("READER & ACTION STATE");ImGui::TextWrapped("Backend: %s",backend_status().c_str());ImGui::TextWrapped("Input gate: %s",game_output_reason().c_str());ImGui::TextWrapped("%s",collision::status().c_str());
            }
            ImGui::EndChild();menu_scroll_window=nullptr;ImGui::PopID();ImGui::PopID();menu_search_reset_scroll=false;menu_search_target.clear();
            if(scoped_frame)ImGui::EndDisabled();
            if(!pending_profile_name.empty())ImGui::OpenPopup("Unsaved profile changes");
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{18,16});
            if(ImGui::BeginPopupModal("Unsaved profile changes",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+440);ImGui::TextWrapped("Active profile %s has unsaved edits. Load %s?",active_slot.c_str(),pending_profile_name.c_str());
                if(active_profile_protected())ImGui::TextWrapped("The active file could not be loaded and is protected. Keep editing and create a new profile to save a copy.");
                else if(preview_session)ImGui::TextWrapped("Preview does not save automatically. Save and load explicitly writes the active profile.");
                if(!profile_message.empty())ImGui::TextWrapped("%s",profile_message.c_str());ImGui::PopTextWrapPos();
                ImGui::BeginDisabled(active_profile_protected());
                if(ImGui::Button("Save and load")){std::string error;if(save_named_profile(active_slot,&error)){const auto next=pending_profile_name;pending_profile_name.clear();load_profile_now(next);ImGui::CloseCurrentPopup();}else profile_message="Load cancelled. Current edits were kept: "+error;}
                ImGui::EndDisabled();ImGui::SameLine();
                if(ImGui::Button("Discard and load")){const auto next=pending_profile_name;pending_profile_name.clear();load_profile_now(next);ImGui::CloseCurrentPopup();}
                ImGui::SameLine();if(ImGui::Button("Keep editing")){pending_profile_name.clear();ImGui::CloseCurrentPopup();}
                ImGui::EndPopup();
            }ImGui::PopStyleVar();
            d->AddLine({p.x+content_x,p.y+mh-55},{p.x+mw-16,p.y+mh-55},ImGui::GetColorU32(ImGuiCol_Border));
            char hint[80];snprintf(hint,sizeof(hint),"%s  menu",key_name(cfg.I("system.menu_key")));
            const float hint_width=aceui::small->CalcTextSizeA(aceui::small->FontSize*ImGui::GetIO().FontGlobalScale,1000,0,hint).x;
            const float metadata_width=std::max(1.f,cw-hint_width-14.f);
            d->PushClipRect({p.x+content_x,p.y+mh-49},{p.x+content_x+metadata_width,p.y+mh-31},true);
            const std::string build_label=std::string(build_info::display)+" | "+build_info::id;aceui::text(d,aceui::small,{p.x+content_x,p.y+mh-47},aceui::muted,build_label.c_str());d->PopClipRect();
            d->PushClipRect({p.x+content_x+metadata_width+8,p.y+mh-49},{p.x+mw-16,p.y+mh-31},true);aceui::text(d,aceui::small,{p.x+mw-16-hint_width,p.y+mh-47},aceui::muted,hint);d->PopClipRect();
            ImGui::SetCursorPos({content_x,mh-29});
            if(preview)aceui::status("Preview",false);else aceui::status(mem.ready()?"DMA connected":"DMA disconnected",mem.ready());
            ImGui::SameLine();ImGui::TextDisabled("  /  %s",inp.device_ok?"Input connected":"Input disconnected");
            cfg.context.clear();Settings::visual_team=false;
            ImGui::End();
        }
        if(menu_open){
            POINT cp; GetCursorPos(&cp);ScreenToClient(hwnd,&cp);
            bool over=(cp.x>=menu_pos.x&&cp.x<=menu_pos.x+menu_size.x&&
                      cp.y>=menu_pos.y&&cp.y<=menu_pos.y+menu_size.y);
            if(keybind_visible)over|=cp.x>=keybind_pos.x&&cp.x<=keybind_pos.x+keybind_size.x&&cp.y>=keybind_pos.y&&cp.y<=keybind_pos.y+keybind_size.y;
            if(!preview)set_clickthrough(hwnd,!over);
        } else {
            if(!preview)set_clickthrough(hwnd,true);
        }
        if(cfg.B("ui.splash")&&now_ms()-splash_started<1100){
            auto* d=ImGui::GetForegroundDrawList();float a=1.f-clampf(((float)(now_ms()-splash_started)-750.f)/350.f,0,1);
            d->AddRectFilled({W*.5f-150,H*.5f-96},{W*.5f+150,H*.5f+80},IM_COL32(30,36,44,(int)(235*a)),12);d->AddRect({W*.5f-150,H*.5f-96},{W*.5f+150,H*.5f+80},IM_COL32(225,180,80,(int)(110*a)),12);
            aceui::logo(d,{W*.5f-32,H*.5f-60},64);
            auto size=aceui::heading->CalcTextSizeA(aceui::heading->FontSize,1000,0,"AceSoftware");
            aceui::text(d,aceui::heading,{W*.5f-size.x*.5f,H*.5f+20},aceui::white,"AceSoftware");
        }
        ImGui::Render();
        bool fuser=!preview&&cfg.I("system.overlay_mode")==1;float bg[4]={0,0,0,fuser?1.f:0.f};
        ctx->ClearRenderTargetView(rtv,bg);
        cooperative_capture::MenuDrawData capture_draw_data;ImDrawData* submitted_draw_data=ImGui::GetDrawData();
        if(capture_overlay_only&&menu_open){
            if(capture_draw_data.without_menu(submitted_draw_data))submitted_draw_data=&capture_draw_data.data;
            else framebuffer_capture.reject_prepared("Menu-owned draw-list exclusion could not be proven; normal UI rendering retained");
        }
        ImGui_ImplDX11_RenderDrawData(submitted_draw_data);
        if(!preview)framebuffer_capture.after_draw(dev,ctx,swap,hwnd,fuser,++capture_frame_id);
        auto present_begin=std::chrono::steady_clock::now();
        plat_stage("renderer","presenting frame",false);HRESULT presented=swap->Present(!cfg.B("system.fps_uncapped")&&cfg.B("system.overlay_vsync")?1:0,0);
        auto present_end=std::chrono::steady_clock::now();
        if(!cfg.B("system.fps_uncapped")){auto deadline=frame_begin+std::chrono::microseconds(1000000/std::max(30,cfg.I("system.max_fps")));
            plat_stage("renderer","frame limiter",false);frame_pacer.wait_until(deadline);}
        auto frame_end=std::chrono::steady_clock::now();
        render_timings.work_ms=std::chrono::duration<double,std::milli>(present_begin-frame_begin).count();
        render_timings.present_ms=std::chrono::duration<double,std::milli>(present_end-present_begin).count();
        render_timings.wait_ms=std::chrono::duration<double,std::milli>(frame_end-present_end).count();render_timings.present_result=presented;
        if(presented==DXGI_ERROR_DEVICE_REMOVED||presented==DXGI_ERROR_DEVICE_RESET){plat_log("Renderer device lost: Present HRESULT 0x%08X; removal reason 0x%08X",(unsigned)presented,(unsigned)dev->GetDeviceRemovedReason());done=true;}
    }
    framebuffer_capture.shutdown(); // Render-thread GPU teardown; join CPU writer before COM/device shutdown.
    dma_thread_run=false;
    if(dma_w.joinable())dma_w.join();
    if(profile_lifecycle_policy::may_autosave(cfg.B("system.autosave"),preview_session,active_profile_protected(),cfg.dirty)){std::string error;if(!save_named_profile(active_slot,&error))plat_log("Profile exit save failed: %s",error.c_str());}
    radar::stop();
    if(!preview)entity_thread_stop();
    ability_icons::shutdown();game_model::shutdown();
    for(auto& kv:portrait_textures)if(kv.second)kv.second->Release();portrait_textures.clear();
    if(SUCCEEDED(com))CoUninitialize();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    return 0;
}
}

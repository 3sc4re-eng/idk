#include "loaderui.h"
#include "aceui.h"
#include "config.h"
#include "build_info.h"
#include "game.h"
#include "game_icons.h"
#include "icon.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>
#include <direct.h>
#include <string>
#include <cstdio>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cctype>
extern char g_ace_user[64];
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace loaderui{
// Presentation bounds are independent of overlay composition and runtime state.
static RECT loader_work_area(HWND hwnd=nullptr){
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTOPRIMARY),&monitor))return monitor.rcWork;
    RECT work{};if(SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0))return work;
    return RECT{0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
}
static RECT loader_minimum_rect(){
    RECT minimum{0,0,600,400};AdjustWindowRectEx(&minimum,WS_OVERLAPPEDWINDOW,FALSE,0);return minimum;
}
static RECT loader_initial_rect(HWND hwnd=nullptr){
    RECT work=loader_work_area(hwnd),minimum=loader_minimum_rect();
    LONG workw=std::max(1L,work.right-work.left),workh=std::max(1L,work.bottom-work.top);
    LONG width=std::min(workw,std::max(minimum.right-minimum.left,std::min(940L,workw-48)));
    LONG height=std::min(workh,std::max(minimum.bottom-minimum.top,std::min(660L,workh-48)));
    LONG x=work.left+(workw-width)/2,y=work.top+(workh-height)/2;return RECT{x,y,x+width,y+height};
}
static LRESULT CALLBACK LdWndProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==WM_GETMINMAXINFO){
        auto* limits=reinterpret_cast<MINMAXINFO*>(l);RECT minimum=loader_minimum_rect(),work=loader_work_area(h);
        limits->ptMinTrackSize.x=std::min(minimum.right-minimum.left,std::max(1L,work.right-work.left));
        limits->ptMinTrackSize.y=std::min(minimum.bottom-minimum.top,std::max(1L,work.bottom-work.top));return 0;
    }
    if(ImGui_ImplWin32_WndProcHandler(h,m,w,l))return 1;
    if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,m,w,l);
}
static void fatal_box(const char* what){plat_log("FATAL: %s",what);MessageBoxA(nullptr,what,"ACE",MB_OK|MB_ICONERROR);}
static void load_font(){aceui::fonts();}
static void ace_style(){aceui::style();auto color=[](const char* key){auto c=cfg.C(key);return ImVec4(c.r,c.g,c.b,c.a);};aceui::theme(color("ui.accent"),color("ui.background"),color("ui.panel"),color("ui.text"));}
static bool make_swap_chain(HWND hwnd,int& bw,int& bh,IDXGISwapChain** swap,ID3D11Device** dev,ID3D11DeviceContext** ctx){
    static const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0,D3D_FEATURE_LEVEL_9_3,D3D_FEATURE_LEVEL_9_1};
    RECT client{};if(!GetClientRect(hwnd,&client)||client.right-client.left<8||client.bottom-client.top<8){RECT bounds=loader_initial_rect(hwnd);SetWindowPos(hwnd,nullptr,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE);UpdateWindow(hwnd);GetClientRect(hwnd,&client);}
    UINT width=(UINT)(client.right-client.left),height=(UINT)(client.bottom-client.top);bw=(int)width;bh=(int)height;
    plat_log("Graphics: creating device (buffer %ux%u)...",width,height);DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Width=width;desc.BufferDesc.Height=height;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferDesc.RefreshRate.Numerator=0;desc.BufferDesc.RefreshRate.Denominator=0;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=hwnd;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL level{};HRESULT result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,6,D3D11_SDK_VERSION,&desc,swap,dev,&level,ctx);
    if(FAILED(result)){plat_log("Graphics: hardware failed (0x%08X) - trying WARP",(unsigned)result);result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,levels,6,D3D11_SDK_VERSION,&desc,swap,dev,&level,ctx);}
    if(FAILED(result)){char text[160];snprintf(text,sizeof(text),"ACE: graphics failed on this PC (0x%08X)",(unsigned)result);MessageBoxA(nullptr,text,"ACE",MB_OK|MB_ICONERROR);return false;}
    plat_log("Graphics: OK (Flvl 0x%04X)",(unsigned)level);return true;
}
static void game_art(int index,ImVec2 box){
    ImVec2 p=ImGui::GetCursorScreenPos();auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(p,{p.x+box.x,p.y+box.y},ImGui::GetColorU32(ImGuiCol_FrameBg),8);
    if(auto image=game_icons::get(index)){unsigned iw=0,ih=0;game_icons::dimensions(index,iw,ih);if(iw&&ih){float scale=std::min(box.x/iw,box.y/ih),width=iw*scale,height=ih*scale;ImVec2 a{p.x+(box.x-width)*.5f,p.y+(box.y-height)*.5f};draw->AddImage(image,a,{a.x+width,a.y+height});}}ImGui::Dummy(box);
}
bool run(bool preview,bool automatic_deadlock_menu){
    if(preview&&automatic_deadlock_menu)return false;
    // ACE_LAUNCHER_WAIT_MENU_V1: selecting a menu is independent of attachment.
    // Unsupported entries stay unavailable; the existing overlay worker still
    // validates the selected game's backend before any reader can start.
    if(automatic_deadlock_menu){
        if(!games::DEADLOCK.live)return false;
        games::selected=&games::DEADLOCK;snprintf(g_ace_user,64,"Local session");
        plat_log("Automatic menu request: Deadlock; opening menu, waiting for game.");
        return true;
    }
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=LdWndProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"ACELoader";wc.hCursor=LoadCursorA(nullptr,IDC_ARROW);
    HICON ic=preview?LoadIconA(nullptr,IDI_APPLICATION):ensure_ace_icon();if(ic){wc.hIcon=ic;wc.hIconSm=ic;}
    if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){fatal_box("ACE: could not register window class");return false;}
    RECT bounds=loader_initial_rect();
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,preview?L"AceSoftware - Loader Preview":L"AceSoftware",WS_OVERLAPPEDWINDOW,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,wc.hInstance,nullptr);
    if(!hwnd){fatal_box("ACE: could not create the loader window");return false;}ShowWindow(hwnd,SW_SHOW);UpdateWindow(hwnd);
    IDXGISwapChain* swap=nullptr;ID3D11Device* dev=nullptr;ID3D11DeviceContext* ctx=nullptr;int bw=0,bh=0;if(!make_swap_chain(hwnd,bw,bh,&swap,&dev,&ctx)){DestroyWindow(hwnd);return false;}
    ID3D11RenderTargetView* rtv=nullptr;ID3D11Texture2D* back=nullptr;
    auto target=[&](){HRESULT result=swap->GetBuffer(0,IID_PPV_ARGS(&back));if(SUCCEEDED(result))result=dev->CreateRenderTargetView(back,nullptr,&rtv);if(back){back->Release();back=nullptr;}return SUCCEEDED(result);};
    if(!target()){fatal_box("ACE: could not create render target");swap->Release();ctx->Release();dev->Release();DestroyWindow(hwnd);return false;}
    IMGUI_CHECKVERSION();ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;load_font();ace_style();ImGui_ImplWin32_Init(hwnd);ImGui_ImplDX11_Init(dev,ctx);game_icons::initialize(dev);
    bool launch=false,done=false;int selected=0;snprintf(g_ace_user,64,"Local session");
    u64 opened=now_ms();
    while(!done){
        MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){done=true;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}if(done)break;
        RECT client{};GetClientRect(hwnd,&client);int nextw=client.right,nexth=client.bottom;if(nextw<8||nexth<8){Sleep(25);continue;}
        if(nextw!=bw||nexth!=bh){ctx->OMSetRenderTargets(0,nullptr,nullptr);rtv->Release();rtv=nullptr;HRESULT result=swap->ResizeBuffers(0,nextw,nexth,DXGI_FORMAT_UNKNOWN,0);if(FAILED(result)||!target()){plat_log("Loader resize failed (0x%08X)",(unsigned)result);done=true;break;}bw=nextw;bh=nexth;}
        ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({(float)bw,(float)bh});
        ImGui::Begin("##AceSoftwareLauncher",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        auto* d=ImGui::GetWindowDrawList();const auto& backdrop=ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);d->AddRectFilled({0,0},{(float)bw,(float)bh},ImGui::GetColorU32(ImVec4{backdrop.x,backdrop.y,backdrop.z,1}));
        float margin=bw<800?18.f:24.f,header=76.f,footer=72.f,gap=16.f;float left=std::clamp(bw*.30f,210.f,280.f),rightx=margin+left+gap,rightw=std::max(180.f,bw-rightx-margin),bodyh=std::max(100.f,bh-header-footer-margin);
        float intro=clampf((now_ms()-opened)/450.f,0,1);aceui::logo(d,{margin,20},34);aceui::text(d,aceui::brand,{margin+46,23},ImGui::GetColorU32(ImVec4{aceui::accent.x,aceui::accent.y,aceui::accent.z,aceui::accent.w*intro}),"AceSoftware");
        ImGui::SetCursorPos({rightx,24});float build_width=std::max(1.f,bw-margin-88.f-rightx);ImVec2 build_pos=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##build_metadata",{build_width,24});d->PushClipRect(build_pos,{build_pos.x+build_width,build_pos.y+24},true);
        aceui::text(d,aceui::body,build_pos,ImGui::GetColorU32(ImGuiCol_TextDisabled),build_info::display);d->PopClipRect();
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\nBuild ID: %s",build_info::display,build_info::id);
        ImGui::SetCursorPos({(float)bw-margin-76,23});if(ImGui::Button("Log",{76,30}))plat_show_console();d->AddLine({margin,header-8},{(float)bw-margin,header-8},ImGui::GetColorU32(ImGuiCol_Separator));
        ImGui::SetCursorPos({margin,header});ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,12});ImGui::BeginChild("##game_library",{left,bodyh},true);ImGui::PopStyleVar();ImGui::TextUnformatted("Games");ImGui::Dummy({0,6});
        for(int i=0;i<5;i++){
            ImGui::PushID(i);ImVec2 p=ImGui::GetCursorScreenPos();float width=ImGui::GetContentRegionAvail().x;bool clicked=ImGui::InvisibleButton("##game",{width,66});auto* card=ImGui::GetWindowDrawList();
            card->AddRectFilled(p,{p.x+width,p.y+66},ImGui::GetColorU32(selected==i?ImGuiCol_HeaderActive:ImGui::IsItemHovered()?ImGuiCol_FrameBgHovered:ImGuiCol_FrameBg),8);if(selected==i)card->AddRect(p,{p.x+width,p.y+66},aceui::purple,8,0,1.5f);
            float artw=std::min(82.f,width*.34f),arth=48.f;if(auto icon=game_icons::get(i)){unsigned iw=0,ih=0;game_icons::dimensions(i,iw,ih);if(iw&&ih){float scale=std::min(artw/iw,arth/ih);float w=iw*scale,h=ih*scale;ImVec2 a{p.x+8+(artw-w)/2,p.y+9+(arth-h)/2};card->AddImage(icon,a,{a.x+w,a.y+h});}}
            card->PushClipRect({p.x+artw+18,p.y+8},{p.x+width-8,p.y+58},true);aceui::text(card,aceui::body,{p.x+artw+18,p.y+24},aceui::white,games::ALL[i]->name);card->PopClipRect();
            float label_width=std::max(0.f,width-artw-26.f),label_size=aceui::body->FontSize*ImGui::GetIO().FontGlobalScale;
            if(aceui::body->CalcTextSizeA(label_size,1000.f,0.f,games::ALL[i]->name).x>label_width&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",games::ALL[i]->name);
            if(clicked)selected=i;ImGui::PopID();
        }ImGui::EndChild();
        ImGui::SetCursorPos({rightx,header});ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{22,20});ImGui::BeginChild("##game_panel",{rightw,bodyh},true);ImGui::PopStyleVar();float available=ImGui::GetContentRegionAvail().x;
        ImGui::TextDisabled("Local session");ImGui::Dummy({0,12});
        game_art(selected,{available,std::clamp(bodyh*.42f,100.f,190.f)});ImGui::Dummy({0,12});ImGui::PushFont(aceui::heading);ImGui::TextUnformatted(games::ALL[selected]->name);ImGui::PopFont();
        if(!games::ALL[selected]->live)ImGui::TextDisabled("Unavailable");
        else{ImGui::Dummy({0,12});ImGui::TextWrapped("Open the menu now. ACE waits for the game and checks its backend in the background.");}
        ImGui::EndChild();
        float footy=bh-footer+8;ImGui::SetCursorPos({margin,footy+8});if(!preview){ImGui::PushTextWrapPos(margin+left);ImGui::TextDisabled("Ready to open the menu");ImGui::PopTextWrapPos();}
        ImGui::SetCursorPos({rightx,footy});bool unavailable=!games::ALL[selected]->live;ImGui::BeginDisabled(preview||unavailable);
        std::string action=std::string("Launch ")+games::ALL[selected]->name;
        if(ImGui::Button(action.c_str(),{rightw,44})&&now_ms()-opened>450){
            games::selected=games::ALL[selected];launch=true;done=true;
            plat_log("Menu selected: %s; opening menu, waiting for game.",games::selected->name);
        }ImGui::EndDisabled();ImGui::End();
        ImGui::Render();float bg[]={backdrop.x,backdrop.y,backdrop.z,1};ctx->OMSetRenderTargets(1,&rtv,nullptr);ctx->ClearRenderTargetView(rtv,bg);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());swap->Present(1,0);if(launch)done=true;
    }
    game_icons::shutdown();ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();if(rtv)rtv->Release();swap->Release();ctx->Release();dev->Release();if(IsWindow(hwnd))DestroyWindow(hwnd);
    MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}return launch;
}
}

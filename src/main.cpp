#include "config.h"
#include "build_info.h"
#include "dma.h"
#include "offsets.h"
#include "entities.h"
#include "collision.h"
#include "input.h"
#include "overlay.h"
#include "loaderui.h"
#include "launch_request.h"
#include "game.h"
#include "utils.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <direct.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <cstring>
#include <cstdio>
char g_ace_user[64]="guest";
int main(int argc,char** argv){
    wchar_t executable[32768]{};DWORD count=GetModuleFileNameW(nullptr,executable,32768);if(count&&count<32768){for(DWORD i=count;i>0;i--)if(executable[i-1]==L'\\'){executable[i-1]=0;SetCurrentDirectoryW(executable);break;}}
    const auto request=launch_request::parse(argc,argv);
    if(request==launch_request::Mode::Invalid){std::fputs("Invalid ACE startup request. Use no arguments, --preview, --loader-preview, or --launch-menu deadlock.\n",stderr);return 2;}
    if(request==launch_request::Mode::LoaderPreview){cfg.register_all();return loaderui::run(true)?0:0;}
    if(request==launch_request::Mode::Preview){
        cfg.register_all();
        return overlay::run(true);
    }
    plat_init_console();
    plat_log("Loader starting, ACE entity-reader recovery 2026-11-01 (DMA-only)");plat_log("%s | Build ID %s | UTC %s",build_info::display,build_info::id,build_info::utc);plat_stage("startup","loader");
    _mkdir("configs");
    cfg.register_all();
    if(!loaderui::run(false,request==launch_request::Mode::DeadlockMenu)){hw::shutdown_output();hw::disconnect();plat_log("Loader window closed.");plat_shutdown_diagnostics();return 0;}
    // ACE_LAUNCHER_WAIT_MENU_V1: menu selection does not assert attachment.
    plat_stage("startup","overlay");plat_log("Menu opening for %s; waiting for game. Backend attachment and validation run in the background.",games::selected->name);
    int rc=overlay::run();
    plat_log("Overlay closed by user. Saving config and exiting.");
    // The overlay saves the active named profile before returning.
    collision::shutdown();hw::shutdown_output();hw::disconnect();plat_shutdown_diagnostics();return rc;
}

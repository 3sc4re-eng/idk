#include "game.h"
namespace games{
GameModule DEADLOCK={"Deadlock",{L"deadlock.exe",L"citadel.exe",nullptr,nullptr},"deadlock",true,
    "\x44\x8b\x0d\x00\x00\x00\x00\x45\x85\xc9\x74\x2a","xxx????xxxxx",
    "\x0f\x10\x05\x00\x00\x00\x00\x0f\x11\x44\x24","xxx????xxxx",
    "\x48\x8b\x05\x00\x00\x00\x00","xxx????"};
GameModule COD={"Call of Duty",{L"cod.exe",L"cod22.exe",nullptr,nullptr},"cod",false,
    nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
GameModule OVERWATCH={"Overwatch",{L"Overwatch.exe",nullptr,nullptr,nullptr},"overwatch",false,
    nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
GameModule MARATHON={"Marathon",{L"marathon.exe",nullptr,nullptr,nullptr},"marathon",false,
    nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
GameModule CS2={"Counter-Strike 2",{L"cs2.exe",nullptr,nullptr,nullptr},"cs2",false,
    nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
GameModule* ALL[5]={&DEADLOCK,&COD,&MARATHON,&CS2,&OVERWATCH};
GameModule* selected=&DEADLOCK;
GameModule* from_process(const wchar_t* exe){
    for(auto* g:ALL){
        for(int i=0;i<4;i++){ if(!g->exes[i]) break;
            if(_wcsicmp(exe,g->exes[i])==0) return g; }
    }
    return nullptr;
}
}

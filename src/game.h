#pragma once
#include "utils.h"
struct GameModule{
    const char* name;
    const wchar_t* exes[4];
    const char* cfg_prefix;
    bool live;
    const char* sig_entlist; const char* msk_entlist;
    const char* sig_view;     const char* msk_view;
    const char* sig_local;    const char* msk_local;
};
namespace games{
    extern GameModule DEADLOCK, COD, OVERWATCH, MARATHON, CS2;
    extern GameModule* ALL[5];
    extern GameModule* selected;
    GameModule* from_process(const wchar_t* exe);
}

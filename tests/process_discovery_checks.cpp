#include "vmm_api.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
    const char* names[]={"deadlock.exe","citadel.exe",nullptr,""};unsigned calls=0;
    auto selected=vmm_api::first_pid(names,4,[&](const char* name,PDWORD pid){++calls;assert(!std::strcmp(name,"deadlock.exe"));*pid=1234;return TRUE;});
    assert(selected==1234&&calls==1); // A valid PID does not depend on any EXE module-map result or alias fallback.
    calls=0;selected=vmm_api::first_pid(names,4,[&](const char* name,PDWORD pid){++calls;if(!std::strcmp(name,"deadlock.exe")){*pid=9999;return FALSE;}*pid=4321;return TRUE;});
    assert(selected==4321&&calls==2); // Failed calls cannot publish their output PID.
    calls=0;selected=vmm_api::first_pid(names,4,[&](const char*,PDWORD pid){++calls;*pid=0;return TRUE;});assert(selected==0&&calls==2);
    calls=0;selected=vmm_api::first_pid(names,4,[&](const char*,PDWORD){++calls;return FALSE;});assert(selected==0&&calls==2);
    std::puts("PASS: process selection retains the first valid PID independently of executable modules");
}

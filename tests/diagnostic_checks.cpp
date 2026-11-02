#include "utils.h"
#include <windows.h>
#include <cassert>
#include <filesystem>
int main(){
    plat_init_console();ShowWindow(GetConsoleWindow(),SW_HIDE);
    DWORD mode=0;if(GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&mode))assert(!(mode&ENABLE_QUICK_EDIT_MODE));
    plat_stage("self-test","simulated blocked vendor call");
    Sleep(5600);
    bool found=false;for(auto& line:plat_recent_logs())if(line.find("Health")!=std::string::npos&&line.find("self-test=simulated blocked vendor call")!=std::string::npos)found=true;
    assert(found&&!plat_log_path().empty()&&std::filesystem::exists(plat_log_path()));
    for(int i=0;i<550;i++)plat_log("bounded-ring test %d",i);assert(plat_recent_logs().size()==500);
    plat_shutdown_diagnostics();return 0;
}

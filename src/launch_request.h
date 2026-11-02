#pragma once
// Host startup intent only. This never starts a game or bypasses preflight.
namespace launch_request {
enum class Mode { Manual, Preview, LoaderPreview, DeadlockMenu, Invalid };
constexpr bool equal(const char* a,const char* b) noexcept {
    if(!a||!b)return false;
    while(*a&&*a==*b){++a;++b;}
    return *a==*b;
}
constexpr Mode parse(int count,const char* const* arguments) noexcept {
    if(count<1||!arguments||!arguments[0])return Mode::Invalid;
    if(count==1)return Mode::Manual;
    if(count==2&&equal(arguments[1],"--preview"))return Mode::Preview;
    if(count==2&&equal(arguments[1],"--loader-preview"))return Mode::LoaderPreview;
    if(count==3&&equal(arguments[1],"--launch-menu")&&equal(arguments[2],"deadlock"))return Mode::DeadlockMenu;
    return Mode::Invalid;
}
}

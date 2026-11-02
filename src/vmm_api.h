#pragma once
#include <windows.h>
#include <cstddef>
struct tdVMM_HANDLE;
namespace vmm_api{
using Handle=tdVMM_HANDLE*;
using Initialize=Handle(*)(DWORD,LPCSTR[]);
using Read=BOOL(*)(Handle,DWORD,ULONG64,PBYTE,DWORD,PDWORD,ULONG64);
using Write=BOOL(*)(Handle,DWORD,ULONG64,PBYTE,DWORD);
using Configure=BOOL(*)(Handle,ULONG64,ULONG64);
using Pid=BOOL(*)(Handle,LPCSTR,PDWORD);
using Module=ULONG64(*)(Handle,DWORD,LPCSTR);
inline constexpr ULONG64 refresh_fast=0x2001040000000000ULL;
inline constexpr ULONG64 refresh_process=0x2002000300000000ULL;
// Process selection does not depend on an executable's module-map entry.
template<class Lookup> DWORD first_pid(const char* const* names,std::size_t count,Lookup&& lookup){
    for(std::size_t i=0;i<count;i++){if(!names[i]||!*names[i])continue;DWORD candidate=0;if(lookup(names[i],&candidate)&&candidate)return candidate;}
    return 0;
}
}

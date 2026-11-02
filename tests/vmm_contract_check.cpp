#include "vmm_api.h"
#define _Check_return_opt_
#define _Post_ptr_invalid_
#define _Frees_ptr_opt_
#include "vmmdll.h"
#include <type_traits>
#include <cstdio>
static_assert(std::is_same_v<vmm_api::Read,decltype(&VMMDLL_MemReadEx)>);
static_assert(std::is_same_v<vmm_api::Write,decltype(&VMMDLL_MemWrite)>);
static_assert(std::is_same_v<vmm_api::Pid,decltype(&VMMDLL_PidGetFromName)>);
static_assert(std::is_same_v<vmm_api::Module,decltype(&VMMDLL_ProcessGetModuleBaseU)>);
static_assert(std::is_same_v<vmm_api::Initialize,decltype(&VMMDLL_Initialize)>);
static_assert(vmm_api::refresh_fast==VMMDLL_OPT_REFRESH_FREQ_FAST);
static_assert(vmm_api::refresh_process==VMMDLL_OPT_REFRESH_SPECIFIC_PROCESS);
int main(){std::puts("PASS: all four dynamic VMM contracts match the bundled vendor SDK exactly");}

// Syntax-only ABI checks against the bundled vendor SDK. No runtime calls.
#include "vmm_api.h"
// Bundled MinGW SAL headers omit these annotation-only spellings.
#ifndef _Check_return_opt_
#define _Check_return_opt_
#endif
#ifndef _Post_ptr_invalid_
#define _Post_ptr_invalid_
#endif
#ifndef _Frees_ptr_opt_
#define _Frees_ptr_opt_
#endif
#include "vmmdll.h"
#include <type_traits>
static_assert(std::is_same<decltype(&VMMDLL_Scatter_Initialize),void*(*)(vmm_api::Handle,DWORD,DWORD)>::value,"scatter initialize ABI");
static_assert(std::is_same<decltype(&VMMDLL_Scatter_PrepareEx),BOOL(*)(void*,ULONG64,DWORD,PBYTE,PDWORD)>::value,"scatter prepare ABI");
static_assert(std::is_same<decltype(&VMMDLL_Scatter_ExecuteRead),BOOL(*)(void*)>::value,"scatter execute ABI");
static_assert(std::is_same<decltype(&VMMDLL_Scatter_CloseHandle),void(*)(void*)>::value,"scatter close ABI");

// Compiler-only fixture proposal. Compile with /c (or -fsyntax-only), never link
// or execute. Resolve this include from the transformed scratch source dictionary.
// This translation unit makes no device, file, thread, or application calls.
#include "asset_loader_retry.h"

using asset_loader_retry::Phase;
using asset_loader_retry::State;

constexpr bool failure_backoff_and_success_dedup(){
    State state;
    if(!state.accepts(false,100))return false;
    state.begin(true);
    if(state.phase()!=Phase::loading||state.accepts(true,100000))return false;
    state.finish(false,1000);
    if(state.phase()!=Phase::failed)return false;
    if(state.accepts(true,999)||state.accepts(true,1000)||state.accepts(true,2999))return false;
    if(!state.accepts(true,3000))return false;
    state.begin(true);
    if(state.accepts(true,100000))return false;
    state.finish(true,3001);
    return state.phase()==Phase::ready&&!state.accepts(true,100000)&&state.accepts(false,3001);
}
static_assert(failure_backoff_and_success_dedup());

constexpr bool worker_launch_failure_uses_same_backoff(){
    State state;
    state.begin(true);
    // The constructor catch path calls finish(false,now), just like an active
    // failed load; there is no permanently latched same-key request.
    state.finish(false,0);
    if(state.accepts(true,1999)||!state.accepts(true,2000))return false;
    state.begin(true);
    state.finish(false,2000);
    return !state.accepts(true,3999)&&state.accepts(true,4000);
}
static_assert(worker_launch_failure_uses_same_backoff());

constexpr bool changed_key_bypasses_failed_key_backoff(){
    State state;
    state.begin(true);state.finish(false,7000);
    if(state.accepts(true,7001)||!state.accepts(false,7001))return false;
    state.begin(true);
    // The owning loader's generation/key check discards the old job before
    // finish(). This check models the accepted new job's state at that boundary.
    return state.phase()==Phase::loading&&!state.accepts(true,100000);
}
static_assert(changed_key_bypasses_failed_key_backoff());

constexpr bool unavailable_and_shutdown_are_deduplicated(){
    State state;
    state.begin(false);
    if(state.phase()!=Phase::unavailable||state.accepts(true,100000)||!state.accepts(false,1))return false;
    state.stop();state.begin(true);state.finish(false,1);
    return state.phase()==Phase::stopped&&!state.accepts(true,100000)&&!state.accepts(false,100000);
}
static_assert(unavailable_and_shutdown_are_deduplicated());

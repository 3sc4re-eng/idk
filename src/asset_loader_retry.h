#pragma once
#include <cstdint>

// Generic request deduplication and failure retry policy. The owner serializes
// access, supplies monotonic milliseconds, and rejects obsolete completions.
namespace asset_loader_retry {
enum class Phase : unsigned char { idle, loading, ready, failed, unavailable, stopped };
class State {
    Phase phase_=Phase::idle;
    std::uint64_t failed_at_ms_=0;
public:
    // Same-key failures may start at most once per two seconds after completion.
    // Ready/loading/unregistered requests are independently deduplicated.
    static constexpr std::uint64_t backoff_ms=2000;
    constexpr Phase phase()const noexcept{return phase_;}
    constexpr bool accepts(bool same_key,std::uint64_t now_ms)const noexcept{
        if(phase_==Phase::stopped)return false;
        if(!same_key)return true;
        return phase_==Phase::failed&&now_ms>=failed_at_ms_&&now_ms-failed_at_ms_>=backoff_ms;
    }
    constexpr void begin(bool registered)noexcept{
        if(phase_==Phase::stopped)return;
        phase_=registered?Phase::loading:Phase::unavailable;failed_at_ms_=0;
    }
    constexpr void finish(bool success,std::uint64_t now_ms)noexcept{
        if(phase_!=Phase::loading)return;
        phase_=success?Phase::ready:Phase::failed;failed_at_ms_=success?0:now_ms;
    }
    constexpr void stop()noexcept{phase_=Phase::stopped;failed_at_ms_=0;}
};
}

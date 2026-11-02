#pragma once
#include "utils.h"
#include "game.h"
#include <string>
#include <mutex>
#include <atomic>
enum class AttachPhase{Idle,FindingProcess,FindingModules,Validating,Ready,UpdateRequired,Error};
struct Offsets{
    GameModule* g=nullptr;u64 game_base=0,game_size=0,entity_list=0,view_matrix=0,local_ent=0;u32 bound_pid=0;
    std::atomic<bool> resolved{false};std::string report="not attached";mutable std::mutex mutex;
    bool resolve(GameModule* game);bool validate();
    void invalidate(const std::string& reason);
    std::string status_text()const{std::lock_guard<std::mutex> lock(mutex);return report;}
    AttachPhase phase()const{std::lock_guard<std::mutex> lock(mutex);return phase_;}
private:
    AttachPhase phase_=AttachPhase::Idle;std::mutex operation_mutex_;
    void report_status(AttachPhase phase,const std::string& message);
};
extern Offsets off;

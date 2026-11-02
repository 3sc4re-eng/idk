#pragma once
#include "entities.h"
namespace aim{
    extern std::string wait_reason;
    extern float flight_seconds;extern std::string prediction_status;
    void tick(u64 sw,u64 sh);
    extern bool active; extern int target_idx; extern Vec2 target_screen;
    extern u64 override_target;
}

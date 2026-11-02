#pragma once
#include <atomic>
#include <cmath>
#include <limits>
namespace world_units {
inline constexpr float per_metre=39.37007874f;
inline std::atomic<float> active_per_metre{per_metre};
inline bool valid_scale(float value){return std::isfinite(value)&&value>0&&value<=1000000;}
inline void set_scale(float value){active_per_metre.store(valid_scale(value)?value:0);}
inline float metres(float value){const auto scale=active_per_metre.load();return valid_scale(scale)?value/scale:std::numeric_limits<float>::infinity();}
inline float units(float value){const auto scale=active_per_metre.load();return valid_scale(scale)?value*scale:0;}
}

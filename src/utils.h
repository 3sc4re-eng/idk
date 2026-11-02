#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <numbers>
#include <cstdint>
#include <string>
#include <vector>
#include <random>
#include <deque>
#include <mutex>
#include <chrono>
#include <map>
#include <algorithm>
#include <cstring>
#include <cstdio>
using u8=uint8_t; using u32=uint32_t; using u64=uint64_t;
using i32=int32_t; using i64=int64_t;
inline uint64_t now_ms(){
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
struct Vec2{ float x=0,y=0;
    Vec2 operator+(Vec2 o)const{return{x+o.x,y+o.y};}
    Vec2 operator-(Vec2 o)const{return{x-o.x,y-o.y};}
    Vec2 operator*(float s)const{return{x*s,y*s};}
    float len()const{return std::sqrt(x*x+y*y);}
};
struct Vec3{ float x=0,y=0,z=0;
    Vec3 operator+(Vec3 o)const{return{x+o.x,y+o.y,z+o.z};}
    Vec3 operator-(Vec3 o)const{return{x-o.x,y-o.y,z-o.z};}
    Vec3 operator*(float s)const{return{x*s,y*s,z*s};}
    float len()const{ return std::sqrt(x*x+y*y+z*z);}
    float len2()const{ return x*x+y*y+z*z;}
    float dist(Vec3 o)const{ return (*this-o).len();}
};
struct Color{ float r=1,g=1,b=1,a=1; };
void plat_init_console();
void plat_log(const char* fmt,...);
// Repeated calls refresh the current operation's start time; only changed details
// are announced. Pass false for high-frequency poll/publish phase transitions.
void plat_stage(const std::string& component,const std::string& detail,bool announce=true);
std::vector<std::string> plat_recent_logs();
std::string plat_log_path();
void plat_show_console();
void plat_shutdown_diagnostics();
inline float rad2deg(float r){ return r*180.f/std::numbers::pi_v<float>; }
inline float deg2rad(float d){ return d*std::numbers::pi_v<float>/180.f; }
inline float rnd(){ thread_local std::mt19937 rng{(u32)std::random_device{}()};
    return std::uniform_real_distribution<float>(0.f,1.f)(rng); }
inline float rnd_rng(float lo,float hi){ return lo+rnd()*(hi-lo); }
inline bool rnd_chance(float pct){ return rnd()*100.f < pct; }
inline float clampf(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }
inline float gaussf(float mean,float sd){
    thread_local std::mt19937 rng{(u32)std::random_device{}()};
    return mean+sd*std::normal_distribution<float>()(rng);
}
struct LogEvent{ uint64_t t; std::string text; Color c; };
inline std::mutex& log_mtx(){ static std::mutex m; return m; }
inline std::deque<LogEvent>& toast_queue(){ static std::deque<LogEvent> t; return t; }
inline void toast(const std::string& s, Color c={1,0.9f,0.4f,1}){
    std::lock_guard<std::mutex> lk(log_mtx());
    auto& t=toast_queue(); t.push_back({now_ms(),s,c});
    if(t.size()>8) t.pop_front();
}
inline std::deque<LogEvent>& action_log(){ static std::deque<LogEvent> l; return l; }
inline void action_note(const std::string& s, Color c={0.6f,0.9f,1,1}){
    std::lock_guard<std::mutex> lk(log_mtx());
    auto& l=action_log(); l.push_back({now_ms(),s,c});
    if(l.size()>200) l.pop_front();
}

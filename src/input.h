#pragma once
#include "utils.h"
#include <string>
#include <atomic>
struct InputState{
    std::atomic<bool> device_ok{false},keyboard_ok{false},keyboard_api{false}; std::string device_text="disconnected";
    int com_port=-1,baud_rate=0;
    u64 last_user_input=0;
    std::atomic<bool> panic{false};
};
extern InputState inp;
namespace aim_model{struct Movement;}
namespace hw{
    Vec2 guided_move(Vec2 error,aim_model::Movement& state,u64 width,u64 height,float dt,bool magnet);
    Vec2 screen_to_counts(Vec2 pixels,u64 width,u64 height,float dt=0,float max_turn=0);std::string test_connection();std::string test_mouse();std::string test_click();
    bool can_send(int key);std::string test_keyboard();void update_diagnostics();std::string keyboard_status();
    std::string diagnostic_summary();bool physical_mouse_state(int virtual_key,bool& held);std::string physical_mouse_status();
    bool init();
    void mouse_move(int dx,int dy);
    void mouse_down(int b);
    void mouse_up(int b);
    void mouse_click(int b,int ms);
    void key_tap(int vk,int ms=30);
    void reconnect();void disconnect();
    bool key_chord(int modifier,int key,int ms=35);
}

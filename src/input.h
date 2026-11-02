#pragma once
#include "utils.h"
#include <string>
#include <atomic>
#include <memory>
#include "output_queue_policy.h"
struct InputState{
    std::atomic<bool> device_ok{false},keyboard_ok{false},keyboard_api{false};
private:
    std::atomic<std::shared_ptr<const std::string>> device_text_{std::make_shared<const std::string>("disconnected")};
public:
    void set_device_text(std::string text){device_text_.store(std::make_shared<const std::string>(std::move(text)),std::memory_order_release);}
    std::string device_status()const{const auto text=device_text_.load(std::memory_order_acquire);return text?*text:std::string("disconnected");}
    int com_port=-1,baud_rate=0;
    u64 last_user_input=0;
    std::atomic<bool> panic{false};
};
extern InputState inp;
namespace aim_model{struct Movement;}
namespace hw{
    using Origin=output_queue::Origin;
    Vec2 guided_move(Vec2 error,aim_model::Movement& state,u64 width,u64 height,float dt,bool magnet);
    Vec2 screen_to_counts(Vec2 pixels,u64 width,u64 height,float dt=0,float max_turn=0);std::string test_connection();std::string test_mouse();std::string test_click();
    bool can_send(int key);std::string test_keyboard();void update_diagnostics();std::string keyboard_status();
    std::string diagnostic_summary();bool physical_mouse_state(int virtual_key,bool& held);std::string physical_mouse_status();
    bool init();
    // Automatic output is bounded/asynchronous. Acceptance never promises
    // delivery; key_chord returns queue acceptance, not a serial-write result.
    void mouse_move(int dx,int dy,Origin origin=Origin::General);
    void mouse_down(int b,Origin origin=Origin::General);
    void mouse_up(int b,Origin origin=Origin::General);
    void mouse_click(int b,int ms,Origin origin=Origin::General);
    void key_tap(int vk,int ms=30,Origin origin=Origin::General);
    void reconnect();void disconnect();
    bool key_chord(int modifier,int key,int ms=35,Origin origin=Origin::General);
    void set_activity(Origin origin,bool active);
    void set_activity(bool aim_active,bool trigger_active);
    void cancel(Origin origin);void cancel_all();void shutdown_output();
    std::string automatic_output_status();
}

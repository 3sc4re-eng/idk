#pragma once
// Consumers of local native weapon/selected-ability flight data. A saved
// prediction toggle alone never implies an enabled consumer.
namespace ballistics_demand {
struct Consumers {
    bool aim_enabled=false,aim_prediction=false;
    bool trigger_enabled=false,trigger_prediction=false;
    bool magnet_enabled=false,magnet_prediction=false;
    bool debug_display=false;
};
constexpr bool required(Consumers value){
    return value.debug_display||
        (value.aim_enabled&&value.aim_prediction)||
        (value.trigger_enabled&&(value.trigger_prediction||
            (value.magnet_enabled&&value.magnet_prediction)));
}
}

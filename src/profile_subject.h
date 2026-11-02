#pragma once
#include <string>

// A backend observation, not a menu selection or a fabricated equipped item.
// game_id uses the registered GameModule::cfg_prefix (deadlock/cs2/etc.). A
// producer sets verified only after reading and identifying the live subject.
namespace profile_subject {
enum class Kind {Global,Hero,Weapon};
struct Subject {
    Kind kind=Kind::Global;
    std::string game_id,name,category;
    bool verified=false;
};
}

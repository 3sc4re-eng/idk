#pragma once
#include <string_view>

// Behavioral pauses are configurable. Snapshot/input freshness and active-match
// admission remain independent requirements for managed live output.
namespace action_pause_policy {
inline constexpr char legacy_key[]="input.disable_in_menu";
inline constexpr char diagnostic_key[]="dbg.pause_raw_ui_signal";
inline constexpr bool diagnostic_default=false;
constexpr bool is_retired_key(std::string_view key) noexcept { return key==legacy_key; }
constexpr bool operative_bool(std::string_view key,bool stored) noexcept {
    return !is_retired_key(key)&&stored;
}
constexpr bool raw_signal_active(bool readable,unsigned raw) noexcept {
    return readable&&(raw&3u)!=0;
}
constexpr bool diagnostic_pause(bool enabled,bool readable,unsigned raw) noexcept {
    return enabled&&raw_signal_active(readable,raw);
}
constexpr bool ace_pause(bool enabled,bool actual_menu_open) noexcept {
    return enabled&&actual_menu_open;
}
constexpr bool regular_search_key(std::string_view key) noexcept {
    return !is_retired_key(key)&&key!=diagnostic_key;
}
struct Preferences {
    bool pause_in_ace_menu=true,pause_when_dead=true,pause_in_scoreboard=true;
    bool pause_when_spectator=true,diagnostic_raw_ui=false;
};
struct Observations {
    bool managed=false,snapshot_valid=false,input_valid=false,fresh=false,in_match=false;
    bool life_known=false,dead=false,scoreboard=false,spectator=false;
    bool raw_ui_readable=false;unsigned raw_ui_flags=0;bool ace_menu_open=false;
};
enum class Reason {Ready,InvalidSnapshot,InvalidInput,StaleSnapshot,InactiveMatch,
                   UnknownLife,Dead,RawUiSignal,Scoreboard,Spectator,AceMenu};
constexpr Reason evaluate(const Preferences& p,const Observations& s) noexcept {
    if(s.managed){
        if(!s.snapshot_valid)return Reason::InvalidSnapshot;
        if(!s.input_valid)return Reason::InvalidInput;
        if(!s.fresh)return Reason::StaleSnapshot;
        if(!s.in_match)return Reason::InactiveMatch;
        if(p.pause_when_dead&&!s.life_known)return Reason::UnknownLife;
        if(p.pause_when_dead&&s.dead)return Reason::Dead;
    }
    if(diagnostic_pause(p.diagnostic_raw_ui,s.raw_ui_readable,s.raw_ui_flags))return Reason::RawUiSignal;
    if(s.managed&&p.pause_in_scoreboard&&s.scoreboard)return Reason::Scoreboard;
    if(s.managed&&p.pause_when_spectator&&s.spectator)return Reason::Spectator;
    if(ace_pause(p.pause_in_ace_menu,s.ace_menu_open))return Reason::AceMenu;
    return Reason::Ready;
}
constexpr const char* reason_text(Reason reason) noexcept {
    switch(reason){
        case Reason::InvalidSnapshot:return "no validated game snapshot";
        case Reason::InvalidInput:return "game key data unavailable";
        case Reason::StaleSnapshot:return "game snapshot older than 250 ms";
        case Reason::InactiveMatch:return "match inactive";
        case Reason::UnknownLife:return "life data unavailable for pause-while-dead check";
        case Reason::Dead:return "local player dead";
        case Reason::RawUiSignal:return "diagnostic pause on raw UI signal";
        case Reason::Scoreboard:return "scoreboard open";
        case Reason::Spectator:return "spectating";
        case Reason::AceMenu:return "ACE menu open";
        default:return "ready";
    }
}
} // namespace action_pause_policy

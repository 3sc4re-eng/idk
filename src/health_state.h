#pragma once
#include <cstdint>

// Field/source validation only. Identity and pose are validated independently
// by the reader; no maximum-health result may reject an otherwise valid pose.
namespace health_state {
enum class Source {Unavailable,BaseEntity,ControllerGlobal};
enum class Reason {Ready,GlobalUnreadable,MaximumOutOfRange,CurrentOutOfRange,AboveMaximum,AliveWithZeroHealth,CurrentUnreadable,LifeUnreadable};
struct PlayerMaximum {bool readable=false;std::int32_t maximum=0,controller_health=0;bool controller_readable=false;};
struct State {
    std::int32_t current=0,maximum=0;Source source=Source::Unavailable;Reason reason=Reason::Ready;
    bool current_valid=false,maximum_valid=false,pair_valid=false,life_valid=false,alive_valid=false,alive=false;
};
inline const char* source_name(Source source){switch(source){case Source::BaseEntity:return "base_entity";case Source::ControllerGlobal:return "controller_player_global";default:return "unavailable";}}
inline const char* reason_name(Reason reason){switch(reason){case Reason::Ready:return "health valid";case Reason::GlobalUnreadable:return "player maximum unavailable";case Reason::MaximumOutOfRange:return "maximum out of range";case Reason::CurrentOutOfRange:return "current health out of range";case Reason::AboveMaximum:return "current exceeds reported maximum";case Reason::AliveWithZeroHealth:return "alive life-state with zero health";case Reason::CurrentUnreadable:return "current health unreadable";default:return "life state unreadable";}}
constexpr State evaluate(std::int32_t current,std::int32_t base_maximum,std::uint8_t life,bool player,const PlayerMaximum* global=nullptr,bool current_readable=true,bool base_maximum_readable=true,bool life_readable=true){
    State s;s.current=current;s.current_valid=current_readable&&current>=0&&current<10000000;s.life_valid=life_readable;
    s.alive_valid=s.current_valid&&s.life_valid&&(life!=0||current>0);s.alive=s.alive_valid&&life==0&&current>0;
    bool maximum_readable=base_maximum_readable;s.maximum=base_maximum;s.source=Source::BaseEntity;
    if(player){maximum_readable=global&&global->readable;s.maximum=maximum_readable?global->maximum:0;s.source=maximum_readable?Source::ControllerGlobal:Source::Unavailable;}
    s.maximum_valid=maximum_readable&&s.maximum>0&&s.maximum<10000000;
    s.pair_valid=s.current_valid&&s.maximum_valid&&current<=s.maximum;
    if(!current_readable)s.reason=Reason::CurrentUnreadable;
    else if(!s.current_valid)s.reason=Reason::CurrentOutOfRange;
    else if(!life_readable)s.reason=Reason::LifeUnreadable;
    else if(!maximum_readable)s.reason=Reason::GlobalUnreadable;
    else if(!s.maximum_valid)s.reason=Reason::MaximumOutOfRange;
    else if(!s.pair_valid)s.reason=Reason::AboveMaximum;
    else if(life==0&&current==0)s.reason=Reason::AliveWithZeroHealth;
    return s;
}
} // namespace health_state

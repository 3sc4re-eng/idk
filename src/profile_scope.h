#pragma once
#include "profile_subject.h"
#include "config.h"
#include "entities.h"
#include "game.h"
#include <string_view>
#include <vector>

namespace profile_scope {
struct Context {
    std::string key;std::vector<std::string> parents;
    bool operator==(const Context&)const=default;
};
inline constexpr bool segment(std::string_view value){
    if(value.empty()||value.size()>128||value=="."||value=="..")return false;
    for(unsigned char c:value)if(c<32||c==127||c=='/'||c=='\\')return false;
    return true;
}
inline Context for_subject(const profile_subject::Subject& subject,const GameModule* game=games::selected){
    if(!game||!game->name||!*game->name)return {};
    const std::string root=game->name;Context result{root+"/Global",{}};
    if(!subject.verified||!game->cfg_prefix||subject.game_id!=game->cfg_prefix)return result;
    if(subject.kind==profile_subject::Kind::Hero&&segment(subject.name))result.key=root+"/hero/"+subject.name;
    else if(subject.kind==profile_subject::Kind::Weapon){
        const auto category=segment(subject.category)?root+"/weapon/"+subject.category:std::string{};
        if(segment(subject.name)){
            result.key=root+"/weapon-item/"+subject.name;
            if(!category.empty())result.parents.push_back(category);
        }else if(!category.empty())result.key=category;
    }
    return result;
}
inline Context for_entity(const Entity& entity,const GameModule* game=games::selected){return for_subject(entity.settings_subject,game);}
class ActiveScope {
    Context context_;Settings::Scope scope_;
public:
    explicit ActiveScope(const Entity& entity,const GameModule* game=games::selected):context_(for_entity(entity,game)),scope_(context_.key,context_.parents){}
    explicit ActiveScope(const profile_subject::Subject& subject,const GameModule* game=games::selected):context_(for_subject(subject,game)),scope_(context_.key,context_.parents){}
    const Context& context()const noexcept{return context_;}
};
}

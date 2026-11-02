#pragma once
#include <string_view>
namespace profile_lifecycle_policy {
constexpr char fold_ascii(char c){return c>='A'&&c<='Z'?char(c-'A'+'a'):c;}
constexpr bool same_name(std::string_view left,std::string_view right){
    if(left.size()!=right.size())return false;
    for(std::size_t i=0;i<left.size();++i)if(fold_ascii(left[i])!=fold_ascii(right[i]))return false;
    return true;
}
enum class LoadAction {load_now,save_current,confirm_discard};
constexpr LoadAction before_load(bool dirty,bool autosave,bool preview,bool protected_file){
    if(!dirty)return LoadAction::load_now;
    return autosave&&!preview&&!protected_file?LoadAction::save_current:LoadAction::confirm_discard;
}
constexpr bool may_autosave(bool autosave,bool preview,bool protected_file,bool dirty){return autosave&&!preview&&!protected_file&&dirty;}
constexpr bool may_delete(std::string_view selected,std::string_view active){return !same_name(selected,active);}
}

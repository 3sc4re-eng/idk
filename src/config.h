#pragma once
#include "utils.h"
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <windows.h>
#include "../deps/json.hpp"
#include "action_pause_policy.h"
using json = nlohmann::json;
enum class SType{ Bool, Float, Int, Key, Color, Enum };
struct Setting{ std::string path, name, help; SType type=SType::Bool;
    float fmin=0, fmax=100, fstep=1; std::vector<std::string> enums;
    bool b=false; float f=0; int i=0; Color c{1,1,1,1}; };
class Settings{
public:
    std::map<std::string,Setting> m;
    bool dirty=false;json deadzone_migration=json::object();
    static thread_local std::string context;
    static thread_local bool visual_team;
    static inline thread_local std::string parents_context;
    static inline thread_local std::vector<std::string> context_parents;
    struct VisualScope{bool previous;VisualScope(bool team):previous(visual_team){visual_team=team;}~VisualScope(){visual_team=previous;}};
    std::map<std::string,std::map<std::string,Setting>> overrides;
    std::map<std::string,bool> override_enabled;
    static bool scoped(const std::string& key){return key.rfind("aim.",0)==0||key.rfind("trigger.",0)==0;}
    static void set_context(const std::string& name,std::vector<std::string> parents={}){context=name;parents_context=name;context_parents=std::move(parents);}
    struct Scope {
        std::string previous,previous_parents_context;std::vector<std::string> previous_parents;
        Scope(const std::string& name,std::vector<std::string> parents={}):previous(context),previous_parents_context(parents_context),previous_parents(context_parents){set_context(name,std::move(parents));}
        ~Scope(){context=std::move(previous);parents_context=std::move(previous_parents_context);context_parents=std::move(previous_parents);}
    };
    void enable_override(const std::string& name,bool enabled,std::vector<std::string> parents={}){
        if(enabled&&overrides.find(name)==overrides.end()){
            // Resolve effective category/global values before enabling a new
            // child. Existing groups retain their own values on re-enable.
            Scope inherited(name,std::move(parents));
            for(const auto& kv:m)if(scoped(kv.first)){
                const auto* inherited_value=resolved(kv.first.c_str());
                overrides[name][kv.first]=inherited_value?*inherited_value:kv.second;
            }
        }
        override_enabled[name]=enabled;dirty=true;
    }
    Setting* override_value(const std::string& group,const char* path,bool require_enabled){
        if(require_enabled){const auto flag=override_enabled.find(group);if(flag==override_enabled.end()||!flag->second)return nullptr;}
        const auto found=overrides.find(group);if(found==overrides.end())return nullptr;
        const auto value=found->second.find(path);return value==found->second.end()?nullptr:&value->second;
    }
    Setting* resolved(const char* path){
        if(visual_team&&std::string(path).rfind("esp.",0)==0){auto it=m.find(std::string("team.")+path);if(it!=m.end())return &it->second;}
        if(!context.empty()&&scoped(path)){
            const auto game=context.substr(0,context.find('/'));const auto global=game+"/Global";
            if(auto* value=override_value(context,path,true))return value;
            // Legacy direct assignments to context cannot inherit an unrelated
            // thread-local parent list from a different active settings scope.
            if(parents_context==context)for(const auto& parent:context_parents){
                if(parent==context||parent==global||parent.substr(0,parent.find('/'))!=game)continue;
                if(auto* value=override_value(parent,path,true))return value;
            }
            if(auto* value=override_value(global,path,false))return value;
        }
        auto it=m.find(path);return it==m.end()?nullptr:&it->second;
    }
    static bool action_output_binding(const std::string& path){
        if(path=="auto.reload_key")return true;
        bool action_namespace=path.rfind("auto.",0)==0||path.rfind("defense.",0)==0;
        bool key_suffix=path.size()>=4&&path.compare(path.size()-4,4,".key")==0;
        bool alternate_suffix=path.size()>=8&&path.compare(path.size()-8,8,".alt_key")==0;
        return action_namespace&&(key_suffix||alternate_suffix);
    }
    static constexpr bool action_output_key_allowed(int key){return key>0&&key<=254&&key!=VK_LBUTTON&&key!=VK_RBUTTON;}
    static int normalize_binding(const std::string& path,int key){return action_output_binding(path)&&!action_output_key_allowed(key)?0:key;}
    static json encode(const Setting& s){
        switch(s.type){case SType::Bool:return s.b;case SType::Float:return s.f;
        case SType::Color:return json::array({s.c.r,s.c.g,s.c.b,s.c.a});case SType::Key:return normalize_binding(s.path,s.i);default:return s.i;}
    }
    static void decode(Setting& s,const json& v){
        if(s.type==SType::Bool)s.b=v.get<bool>();
        else if(s.type==SType::Float){float n=v.get<float>();if(!std::isfinite(n))throw std::runtime_error("Invalid number");s.f=clampf(n,s.fmin,s.fmax);}
        else if(s.type==SType::Color){if(!v.is_array()||v.size()<3)throw std::runtime_error("Invalid color");const auto channel=[&](std::size_t index){float value=v[index].get<float>();if(!std::isfinite(value))throw std::runtime_error("Invalid color channel");return clampf(value,0.f,1.f);};s.c={channel(0),channel(1),channel(2),v.size()>3?channel(3):1.f};}
        else{s.i=(int)clampf((float)v.get<int>(),s.fmin,s.fmax);if(s.type==SType::Key)s.i=normalize_binding(s.path,s.i);}
    }
    void defb(const char* p,const char* n,bool d){ Setting s; s.path=p;s.name=n;s.type=SType::Bool;s.b=d; m[p]=s; }
    void deff(const char* p,const char* n,float d,float lo,float hi,float st=1){ Setting s; s.path=p;s.name=n;s.type=SType::Float;s.f=d;s.fmin=lo;s.fmax=hi;s.fstep=st; m[p]=s; }
    void defi(const char* p,const char* n,int d,int lo,int hi){ Setting s; s.path=p;s.name=n;s.type=SType::Int;s.i=d;s.fmin=(float)lo;s.fmax=(float)hi; m[p]=s; }
    void defk(const char* p,const char* n,int d){ Setting s; s.path=p;s.name=n;s.type=SType::Key;s.i=normalize_binding(p,d);s.fmin=0;s.fmax=254; m[p]=s; }
    void defc(const char* p,const char* n,float r,float g,float b,float a){ Setting s; s.path=p;s.name=n;s.type=SType::Color;s.c={r,g,b,a}; m[p]=s; }
    bool B(const char* p){auto* s=resolved(p);return action_pause_policy::operative_bool(p,s&&s->b);}
    float F(const char* p){auto* s=resolved(p);return s?s->f:0;}
    int I(const char* p){auto* s=resolved(p);return s?(s->type==SType::Key?normalize_binding(s->path,s->i):s->i):0;}
    Color C(const char* p){auto* s=resolved(p);return s?s->c:Color{};}
    Setting* find(const char* p){return resolved(p);}
    bool write_allowed(){ return B("adv.mem_write"); }
    json export_json()const{
        json j;j["_motion_settings_version"]=2;j["_deadzone_settings_version"]=2;if(!deadzone_migration.empty())j["_deadzone_migration"]=deadzone_migration;for(auto& kv:m)j[kv.first]=encode(kv.second);j["_override_enabled"]=override_enabled;
        for(auto& group:overrides)for(auto& kv:group.second)j["_overrides"][group.first][kv.first]=encode(kv.second);
        return j;
    }
    static void migrate_deadzone(json& group,const json* inherited=nullptr){
        if(!group.is_object())throw std::runtime_error("Invalid settings group");
        if(group.contains("_deadzone_migration")&&!group["_deadzone_migration"].is_object())group.erase("_deadzone_migration");
        auto has=[&](const char* key){return group.contains(key)||(inherited&&inherited->is_object()&&inherited->contains(key));};
        auto read=[&](const char* key,float fallback){const json* source=group.contains(key)?&group:(inherited&&inherited->is_object()&&inherited->contains(key)?inherited:nullptr);if(!source)return fallback;const auto& value=(*source)[key];if(!value.is_number())throw std::runtime_error("Invalid legacy dead-zone number");float number=value.get<float>();if(!std::isfinite(number))throw std::runtime_error("Invalid legacy dead-zone number");return number;};
        auto convert_pixels=[&](float pixels,float fallback,const char* label){
            pixels=clampf(pixels,0,25);if(pixels==0){group["_deadzone_migration"][label]="legacy_zero_pixels";return 0.f;}
            if(has("_deadzone_reference_width")&&has("_deadzone_reference_projection")){
                float width=read("_deadzone_reference_width",0),projection=read("_deadzone_reference_projection",0);
                if(width>0&&width<=32768&&projection>0&&projection<=100){float full_cone=rad2deg(2.f*std::atan(2.f*pixels/(width*projection)));group["_deadzone_migration"][label]=full_cone<=20?"legacy_pixels_explicit_projection":"legacy_pixels_explicit_projection_clamped";return clampf(full_cone,0,20);}
            }
            group["_deadzone_migration"][label]="legacy_pixels_default_without_reference";return fallback;
        };
        if(!group.contains("aim.deadzone_fov")&&(group.contains("aim.deadzone_units")||group.contains("aim.deadzone_degrees")||group.contains("aim.deadzone"))){
            const int units=(int)clampf(read("aim.deadzone_units",0),0,1);
            if(units==1){const float inherited_radius=has("aim.deadzone_fov")?read("aim.deadzone_fov",1.08f)*.5f:.54f;group["aim.deadzone_fov"]=2.f*clampf(read("aim.deadzone_degrees",inherited_radius),0,10);group["_deadzone_migration"]["aim"]="legacy_angular_radius_doubled";}
            else group["aim.deadzone_fov"]=convert_pixels(read("aim.deadzone",9),1.08f,"aim");
        }
        if(!group.contains("trigger.magnet_deadzone_fov")&&group.contains("trigger.magnet_deadzone"))group["trigger.magnet_deadzone_fov"]=convert_pixels(read("trigger.magnet_deadzone",3),.36f,"magnet");
    }
    static void migrate_smoothing(json& j){
        for(const char* key:{"aim.smooth","trigger.magnet_smooth"})if(j.contains(key)&&j[key].is_number()){float old=j[key].get<float>();if(std::isfinite(old))j[key]=1.f+99.f*std::log(clampf(old,1,20))/std::log(400.f);}
    }
    static void migrate_bones(json& j){
        const char* names[]={"head","neck","chest","waist","abdomen","arms","legs"};
        for(auto n:names)if(j.contains(std::string("aim.bone_")+n))return;
        if(!j.contains("aim.hitbox")&&!j.contains("aim.points_mask"))return;
        int mask=j.value("aim.points_mask",0),legacy=j.value("aim.hitbox",0);
        if(!mask){legacy=legacy<0?0:(legacy>2?2:legacy);mask=1<<legacy;}
        for(int i=0;i<7;i++)j[std::string("aim.bone_")+names[i]]=i<3&&(mask&(1<<i))?2:-1;
        if(!j.contains("aim.points_mask")&&legacy==0){j["aim.bone_neck"]=1;for(int i=2;i<6;i++)j[std::string("aim.bone_")+names[i]]=0;}
    }
    bool save(const std::string& file){
        json j=export_json();
        std::ofstream f(file+".tmp");if(!f)return false;
        f<<j.dump(2);f.flush();if(!f)return false;f.close();if(f.fail())return false;
        if(!MoveFileExA((file+".tmp").c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
        dirty=false;return true;
    }
    // File loads and a share-code importer use the same migration/validation path.
    // This adds an import API; no new share-import UI is claimed here.
    bool import_json(json j,bool mark_dirty=true){
        if(!j.is_object())return false;
        auto previous=m;auto old_overrides=overrides;auto old_flags=override_enabled;auto old_migration=deadzone_migration;const bool old_dirty=dirty;
        try{
            // The separate Counterspell binding survives the retired auto-parry
            // feature. Unknown parry.* options are omitted from registered data.
            if(!j.contains("defense.counterspell.key")&&j.contains("defense.parry_key"))j["defense.counterspell.key"]=j["defense.parry_key"];
            bool legacy_motion=j.value("_motion_settings_version",1)<2;
            if(legacy_motion)migrate_smoothing(j);
            migrate_bones(j);migrate_deadzone(j);
            deadzone_migration=j.contains("_deadzone_migration")&&j["_deadzone_migration"].is_object()?j["_deadzone_migration"]:json::object();
            Settings defaults;defaults.register_all();m=defaults.m;
            for(auto& item:j.items()){auto it=m.find(item.key());if(it!=m.end())decode(it->second,item.value());}
            overrides.clear();override_enabled.clear();
            if(j.contains("_override_enabled"))override_enabled=j["_override_enabled"].get<std::map<std::string,bool>>();
            if(j.contains("_overrides"))for(auto& group:j["_overrides"].items()){
                if(!group.value().is_object())throw std::runtime_error("Invalid override");
                if(legacy_motion)migrate_smoothing(group.value());
                migrate_bones(group.value());migrate_deadzone(group.value(),&j);
                if(group.value().contains("_deadzone_migration")&&group.value()["_deadzone_migration"].is_object())deadzone_migration["overrides"][group.key()]=group.value()["_deadzone_migration"];
                for(auto& item:group.value().items()){
                    auto base=m.find(item.key());if(base==m.end()||!scoped(item.key()))continue;
                    Setting value=base->second;decode(value,item.value());overrides[group.key()][item.key()]=value;
                }
            }
            dirty=mark_dirty;return true;
        }catch(...){m=std::move(previous);overrides=std::move(old_overrides);override_enabled=std::move(old_flags);deadzone_migration=std::move(old_migration);dirty=old_dirty;return false;}
    }
    bool load(const std::string& file){
        std::ifstream f(file);if(!f)return false;
        try{json j;f>>j;return import_json(std::move(j),false);}catch(...){return false;}
    }
    void register_all();
};
extern Settings cfg;

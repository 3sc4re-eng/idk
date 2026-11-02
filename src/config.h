#pragma once
#include "utils.h"
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <windows.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <initializer_list>
#include "../deps/json.hpp"
#include "action_pause_policy.h"
using json = nlohmann::json;
enum class SType{ Bool, Float, Int, Key, Color, Enum };
struct Setting{ std::string path, name, help; SType type=SType::Bool;
    float fmin=0, fmax=100, fstep=1; std::vector<std::string> enums;
    bool b=false; float f=0; int i=0; Color c{1,1,1,1}; };
class Settings{
    struct Value {SType type=SType::Bool;bool b=false;float f=0;int i=0;Color c{};};
    struct Values {
        std::map<std::string,Value> root;
        std::map<std::string,std::map<std::string,Value>> overrides;
        std::map<std::string,bool> enabled;
        u64 revision=0;
    };
    static std::recursive_mutex& access_mutex(){static std::recursive_mutex mutex;return mutex;}
    static std::mutex& save_mutex(){static std::mutex mutex;return mutex;}
    static inline thread_local unsigned editor_scope_depth_=0;
    const std::thread::id owner_thread_=std::this_thread::get_id();
    std::atomic<std::shared_ptr<const Values>> published_values_;
    unsigned write_depth_=0;
    bool values_changed_=false;
    u64 revision_=0;
    std::atomic<bool> publication_failed_{false};
public:
    class ReadScope;
private:
    static inline thread_local const ReadScope* read_scope_=nullptr;

public:
    // Raw Setting pointers and public editor tables are borrowed only inside a
    // WriteScope when the instance is shared. No device operation belongs in it.
    class WriteScope {
        Settings& owner_;std::unique_lock<std::recursive_mutex> lock_;
    public:
        explicit WriteScope(Settings& owner):owner_(owner),lock_(access_mutex()){++owner_.write_depth_;++editor_scope_depth_;}
        WriteScope(const WriteScope&)=delete;WriteScope& operator=(const WriteScope&)=delete;
        ~WriteScope() noexcept {
            if(--owner_.write_depth_==0&&owner_.values_changed_){
                try{owner_.publish_locked();}
                catch(...){owner_.publication_failed_=true;owner_.published_values_.store({},std::memory_order_release);}
            }
            --editor_scope_depth_;
        }
    };
    // A reader owns only immutable values, never the editor mutex. Nested scopes
    // for the same instance retain the same image; other instances remain distinct.
    class ReadScope {
        friend class Settings;
        const Settings* owner_;const ReadScope* previous_;std::shared_ptr<const Values> values_;
    public:
        explicit ReadScope(Settings& owner,bool refresh_owner=false):owner_(&owner),previous_(read_scope_){
            for(const auto* scope=previous_;scope;scope=scope->previous_)if(scope->owner_==&owner){values_=scope->values_;read_scope_=this;return;}
            if(refresh_owner&&std::this_thread::get_id()==owner.owner_thread_&&!owner.publication_failed_.load(std::memory_order_acquire)){
                // Direct foreground polling/serial fixtures can observe their
                // owner's latest values. Production reader threads use publish.
                std::lock_guard<std::recursive_mutex> lock(access_mutex());values_=owner.build_values_locked(owner.revision_);
            }else values_=owner.published_values_.load(std::memory_order_acquire);
            read_scope_=this;
        }
        ReadScope(const ReadScope&)=delete;ReadScope& operator=(const ReadScope&)=delete;
        ~ReadScope(){read_scope_=previous_;}
    };
    Settings()=default;
    Settings(const Settings& other){WriteScope edit(*this);copy_data_locked(other);}
    Settings& operator=(const Settings& other){if(this!=&other){WriteScope edit(*this);copy_data_locked(other);}return *this;}
    void mark_dirty(){WriteScope edit(*this);dirty=true;values_changed_=true;}
    bool set_bool(const char* path,bool value,bool make_dirty=true){
        WriteScope edit(*this);auto* setting=resolved(path);if(!setting||setting->type!=SType::Bool)return false;
        if(setting->b==value)return true;setting->b=value;values_changed_=true;if(make_dirty)dirty=true;return true;
    }
    bool set_int(const char* path,int value,bool make_dirty=true){
        WriteScope edit(*this);auto* setting=resolved(path);if(!setting)return false;
        if(setting->type!=SType::Int&&setting->type!=SType::Enum&&setting->type!=SType::Key)return false;
        value=(int)clampf((float)value,setting->fmin,setting->fmax);
        if(setting->type==SType::Key)value=normalize_binding(setting->path,value);
        if(setting->i==value)return true;setting->i=value;values_changed_=true;if(make_dirty)dirty=true;return true;
    }
    bool set_int_if_unchanged(const char* path,int value,std::initializer_list<std::pair<const char*,int>> expected,bool make_dirty=false){
        WriteScope edit(*this);
        for(const auto& match:expected){const auto* setting=resolved(match.first);if(!setting||setting->i!=match.second)return false;}
        return set_int(path,value,make_dirty);
    }
    bool publication_ready()const noexcept{return !publication_failed_.load(std::memory_order_acquire);}
    u64 published_revision()const{const auto values=published_values_.load(std::memory_order_acquire);return values?values->revision:0;}

    std::map<std::string,Setting> m;
    std::atomic<bool> dirty{false};json deadzone_migration=json::object();
    static thread_local std::string context;
    static thread_local bool visual_team;
    static thread_local std::string parents_context;
    static thread_local std::vector<std::string> context_parents;
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
        WriteScope settings_edit(*this);
        if(enabled&&overrides.find(name)==overrides.end()){
            // Resolve effective category/global values before enabling a new
            // child. Existing groups retain their own values on re-enable.
            Scope inherited(name,std::move(parents));
            for(const auto& kv:m)if(scoped(kv.first)){
                const auto* inherited_value=resolved(kv.first.c_str());
                overrides[name][kv.first]=inherited_value?*inherited_value:kv.second;
            }
        }
        override_enabled[name]=enabled;mark_dirty();
    }
    Setting* override_value(const std::string& group,const char* path,bool require_enabled){
        std::lock_guard<std::recursive_mutex> access(access_mutex());
        if(require_enabled){const auto flag=override_enabled.find(group);if(flag==override_enabled.end()||!flag->second)return nullptr;}
        const auto found=overrides.find(group);if(found==overrides.end())return nullptr;
        const auto value=found->second.find(path);return value==found->second.end()?nullptr:&value->second;
    }
    Setting* resolved(const char* path){
        std::lock_guard<std::recursive_mutex> access(access_mutex());
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
    void defb(const char* p,const char* n,bool d){ WriteScope settings_edit(*this); Setting s; s.path=p;s.name=n;s.type=SType::Bool;s.b=d; m[p]=s;values_changed_=true; }
    void deff(const char* p,const char* n,float d,float lo,float hi,float st=1){ WriteScope settings_edit(*this); Setting s; s.path=p;s.name=n;s.type=SType::Float;s.f=d;s.fmin=lo;s.fmax=hi;s.fstep=st; m[p]=s;values_changed_=true; }
    void defi(const char* p,const char* n,int d,int lo,int hi){ WriteScope settings_edit(*this); Setting s; s.path=p;s.name=n;s.type=SType::Int;s.i=d;s.fmin=(float)lo;s.fmax=(float)hi; m[p]=s;values_changed_=true; }
    void defk(const char* p,const char* n,int d){ WriteScope settings_edit(*this); Setting s; s.path=p;s.name=n;s.type=SType::Key;s.i=normalize_binding(p,d);s.fmin=0;s.fmax=254; m[p]=s;values_changed_=true; }
    void defc(const char* p,const char* n,float r,float g,float b,float a){ WriteScope settings_edit(*this); Setting s; s.path=p;s.name=n;s.type=SType::Color;s.c={r,g,b,a}; m[p]=s;values_changed_=true; }
    bool B(const char* p){const auto value=current_value(p);return action_pause_policy::operative_bool(p,value.b);}
    float F(const char* p){return current_value(p).f;}
    int I(const char* p){return current_value(p).i;}
    Color C(const char* p){return current_value(p).c;}
    Setting* find(const char* p){
        std::lock_guard<std::recursive_mutex> access(access_mutex());
        if(std::this_thread::get_id()!=owner_thread_&&!write_depth_)throw std::logic_error("Mutable settings access requires WriteScope");
        return resolved(p);
    }
    bool write_allowed(){ return B("adv.mem_write"); }
    json export_json()const{
        std::lock_guard<std::recursive_mutex> access(access_mutex());
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
    bool save(const std::string& file,bool clear_dirty=true){
        // Lock order is save -> editor. An editor transaction on any Settings
        // instance must finish before this thread can wait for another saver.
        if(editor_scope_depth_)return false;
        std::lock_guard<std::mutex> saving(save_mutex());
        json j;u64 saved_revision=0;
        {std::lock_guard<std::recursive_mutex> access(access_mutex());j=export_json();saved_revision=revision_;}
        std::ofstream f(file+".tmp");if(!f)return false;
        f<<j.dump(2);f.flush();if(!f)return false;f.close();if(f.fail())return false;
        if(!MoveFileExA((file+".tmp").c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
        {std::lock_guard<std::recursive_mutex> access(access_mutex());if(clear_dirty&&revision_==saved_revision)dirty=false;}
        return true;
    }
    // File loads and a share-code importer use the same migration/validation path.
    // This adds an import API; no new share-import UI is claimed here.
    bool import_json(json j,bool mark_dirty=true){
        WriteScope settings_edit(*this);
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
            dirty=mark_dirty;values_changed_=true;return true;
        }catch(...){m=std::move(previous);overrides=std::move(old_overrides);override_enabled=std::move(old_flags);deadzone_migration=std::move(old_migration);dirty=old_dirty;return false;}
    }
    bool load(const std::string& file){
        std::ifstream f(file);if(!f)return false;
        try{json j;f>>j;return import_json(std::move(j),false);}catch(...){return false;}
    }
    void register_all();
private:
    static Value copy_value(const Setting& setting){return {setting.type,setting.b,setting.f,setting.type==SType::Key?normalize_binding(setting.path,setting.i):setting.i,setting.c};}
    static const Value* snapshot_value(const Values& values,const char* path){
        const std::string key=path;
        if(visual_team&&key.rfind("esp.",0)==0){const auto found=values.root.find("team."+key);if(found!=values.root.end())return &found->second;}
        auto from_group=[&](const std::string& group,bool require_enabled)->const Value*{
            if(require_enabled){const auto flag=values.enabled.find(group);if(flag==values.enabled.end()||!flag->second)return nullptr;}
            const auto found=values.overrides.find(group);if(found==values.overrides.end())return nullptr;
            const auto value=found->second.find(key);return value==found->second.end()?nullptr:&value->second;
        };
        if(!context.empty()&&scoped(key)){
            const auto game=context.substr(0,context.find('/'));const auto global=game+"/Global";
            if(const auto* value=from_group(context,true))return value;
            if(parents_context==context)for(const auto& parent:context_parents){
                if(parent==context||parent==global||parent.substr(0,parent.find('/'))!=game)continue;
                if(const auto* value=from_group(parent,true))return value;
            }
            if(const auto* value=from_group(global,false))return value;
        }
        const auto found=values.root.find(key);return found==values.root.end()?nullptr:&found->second;
    }
    std::shared_ptr<const Values> build_values_locked(u64 revision)const{
        auto values=std::make_shared<Values>();values->revision=revision;values->enabled=override_enabled;
        for(const auto& entry:m)values->root.emplace(entry.first,copy_value(entry.second));
        for(const auto& group:overrides)for(const auto& entry:group.second)values->overrides[group.first].emplace(entry.first,copy_value(entry.second));
        return values;
    }
    void publish_locked(){
        const auto next=revision_==UINT64_MAX?revision_:revision_+1;auto values=build_values_locked(next);
        published_values_.store(std::move(values),std::memory_order_release);
        revision_=next;values_changed_=false;publication_failed_=false;
    }
    void copy_data_locked(const Settings& other){
        auto root=other.m;auto groups=other.overrides;auto flags=other.override_enabled;auto migration=other.deadzone_migration;
        m=std::move(root);overrides=std::move(groups);override_enabled=std::move(flags);deadzone_migration=std::move(migration);
        dirty.store(other.dirty.load(std::memory_order_acquire),std::memory_order_release);values_changed_=true;
    }
    Value current_value(const char* path){
        for(const auto* scope=read_scope_;scope;scope=scope->previous_)if(scope->owner_==this){const auto* value=scope->values_?snapshot_value(*scope->values_,path):nullptr;return value?*value:Value{};}
        if(std::this_thread::get_id()!=owner_thread_){if(publication_failed_.load(std::memory_order_acquire))return {};const auto values=published_values_.load(std::memory_order_acquire);const auto* value=values?snapshot_value(*values,path):nullptr;return value?*value:Value{};}
        // Owner/UI reads see edits immediately. Copy the value before releasing
        // the mutex; callers cannot retain a pointer into a replaced profile.
        std::lock_guard<std::recursive_mutex> lock(access_mutex());const auto* setting=resolved(path);return setting?copy_value(*setting):Value{};
    }

};
extern Settings cfg;

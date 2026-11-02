#pragma once
#include "json.hpp"
#include <windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace profile_selection_index {
inline constexpr const char* file_path="configs/_active_profiles.json";
inline constexpr std::size_t maximum_bytes=16384;
constexpr bool valid_prefix(std::string_view prefix){return prefix=="deadlock"||prefix=="cod"||prefix=="marathon"||prefix=="cs2"||prefix=="overwatch";}
constexpr bool valid_name(std::string_view name){
    if(name.empty()||name.size()>40)return false;
    for(char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
    return true;
}
constexpr bool may_remember(bool preview,bool profile_loaded){return !preview&&profile_loaded;}
// The bundled JSON lexer treats a raw NUL as end-of-input. Reject it before
// parsing so a valid prefix cannot hide trailing invalid bytes.
constexpr bool valid_document_bytes(std::string_view bytes){
    if(bytes.empty()||bytes.size()>maximum_bytes)return false;
    for(char c:bytes)if(c=='\0')return false;
    return true;
}
enum class ReadStatus {missing,ready,protected_failure};
struct ReadResult {
    ReadStatus status=ReadStatus::missing;
    std::map<std::string,std::string> entries;
    std::string error;
    std::string selected(const std::string& prefix)const{const auto found=entries.find(prefix);return found==entries.end()?std::string{}:found->second;}
};
inline ReadResult read(const std::string& path=file_path){
    auto fail=[](const char* message){ReadResult result;result.status=ReadStatus::protected_failure;result.error=message;return result;};
    std::error_code ec;const bool exists=std::filesystem::exists(path,ec);
    if(ec)return fail("Could not inspect the startup profile index.");
    if(!exists)return {};
    std::ifstream input(path,std::ios::binary);if(!input)return fail("Could not read the startup profile index; it was left unchanged.");
    std::string bytes(maximum_bytes+1,'\0');input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));const auto count=input.gcount();
    if(input.bad())return fail("Could not read the startup profile index; it was left unchanged.");
    if(count>static_cast<std::streamsize>(maximum_bytes))return fail("Startup profile index is too large; it was left unchanged.");
    bytes.resize(static_cast<std::size_t>(count));
    if(!valid_document_bytes(bytes))return fail("Startup profile index has invalid bytes; it was left unchanged.");
    try{
        // Ambiguous duplicate members are preserved as a protected failure,
        // rather than silently choosing the parser's last value.
        bool duplicate_member=false;
        std::vector<std::set<std::string>> object_keys;
        const auto unique_members=[&](int,nlohmann::json::parse_event_t event,nlohmann::json& value){
            if(event==nlohmann::json::parse_event_t::object_start)object_keys.emplace_back();
            else if(event==nlohmann::json::parse_event_t::key){
                if(object_keys.empty()||!object_keys.back().insert(value.get<std::string>()).second)duplicate_member=true;
            }else if(event==nlohmann::json::parse_event_t::object_end){
                if(!object_keys.empty())object_keys.pop_back();
            }
            return true;
        };
        const auto document=nlohmann::json::parse(bytes,unique_members);
        if(duplicate_member)return fail("Startup profile index has duplicate fields; it was left unchanged.");
        if(!document.is_object()||document.size()!=2||!document.contains("version")||!document["version"].is_number_integer()||document["version"]!=1||!document.contains("active_profiles")||!document["active_profiles"].is_object()||document["active_profiles"].size()>5)return fail("Startup profile index has an unsupported format; it was left unchanged.");
        ReadResult result;result.status=ReadStatus::ready;
        for(const auto& entry:document["active_profiles"].items()){
            if(!valid_prefix(entry.key())||!entry.value().is_string())return fail("Startup profile index contains an invalid selection; it was left unchanged.");
            const auto name=entry.value().get<std::string>();if(!valid_name(name))return fail("Startup profile index contains an invalid profile name; it was left unchanged.");
            result.entries.emplace(entry.key(),name);
        }
        return result;
    }catch(...){return fail("Startup profile index is invalid; it was left unchanged.");}
}
inline bool remember(const std::string& prefix,const std::string& name,std::string& error,const std::string& path=file_path){
    error.clear();if(!valid_prefix(prefix)||!valid_name(name)){error="Invalid startup profile selection.";return false;}
    // Re-read to retain the latest other-game entries. Atomic replacement is not
    // a multi-process transaction; no synchronization claim is made here.
    auto current=read(path);if(current.status==ReadStatus::protected_failure){error=current.error;return false;}
    current.entries[prefix]=name;
    std::error_code ec;const auto parent=std::filesystem::path(path).parent_path();if(!parent.empty())std::filesystem::create_directories(parent,ec);
    if(ec){error="Could not access the startup profile index folder.";return false;}
    static std::atomic<unsigned long long> sequence{0};
    const std::string temporary=path+".tmp."+std::to_string(GetCurrentProcessId())+"."+std::to_string(sequence.fetch_add(1));
    auto fail=[&](const char* message){DeleteFileA(temporary.c_str());error=message;return false;};
    try{
        const nlohmann::json document={{"version",1},{"active_profiles",current.entries}};
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output){error="Could not write the startup profile index.";return false;}
        output<<document.dump(2);output.flush();if(!output){output.close();return fail("Could not write the startup profile index.");}
        output.close();if(output.fail())return fail("Could not close the startup profile index.");
        if(!MoveFileExA(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return fail("Could not replace the startup profile index.");
        return true;
    }catch(...){return fail("Could not write the startup profile index.");}
}
}

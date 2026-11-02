#include "cooperative_capture.h"
#include "cooperative_capture_imgui.h"
#include <cassert>
#include <string>
using namespace cooperative_capture::protocol;
static_assert(id32("0123456789abcdef0123456789abcdef"));
static_assert(!id32("0123456789abcdef0123456789abcdeF"));
static_assert(!id32("../0123456789abcdef0123456789abc"));
static_assert(!id32("000000000000000000000000000000000"));
static_assert(dimensions(1920,1080)&&dimensions(4096,4096));
static_assert(!dimensions(0,1080)&&!dimensions(4097,100));
static_assert(view("overlay")&&view("menu")&&!view("desktop")&&!view("game"));
static_assert(request_time(100,130,100)&&request_time(100,130,129));
static_assert(!request_time(100,130,130)&&!request_time(101,130,100));
static_assert(!request_time(100,131,100)&&!request_time(100,99,100));
static_assert(!request_time(UINT64_MAX-1,0,UINT64_MAX-1));
static_assert(target_identity(42,1000,"build-A",42,1000,"build-A"));
static_assert(!target_identity(43,1000,"build-A",42,1000,"build-A"));
static_assert(!target_identity(42,1001,"build-A",42,1000,"build-A"));
static_assert(!target_identity(42,1000,"build-B",42,1000,"build-A"));
static_assert(output_alpha(0,true)==255&&output_alpha(87,true)==255);
static_assert(output_alpha(0,false)==0&&output_alpha(87,false)==87);
static_assert(rgba_to_bgra({255,0,0,33})==std::array<std::uint8_t,4>{0,0,255,33});
static_assert(rgba_to_bgra({0,0,255,255})==std::array<std::uint8_t,4>{255,0,0,255});
constexpr bool channel_roundtrip(){
    for(unsigned i=0;i<256;++i){const std::array<std::uint8_t,4> source{static_cast<std::uint8_t>(i),static_cast<std::uint8_t>(255-i),static_cast<std::uint8_t>(i^0x55),static_cast<std::uint8_t>(i)};
        const auto stored=rgba_to_bgra(source);if(stored[0]!=source[2]||stored[1]!=source[1]||stored[2]!=source[0]||stored[3]!=source[3]||rgba_to_bgra(stored)!=source)return false;}
    return true;
}
static_assert(channel_roundtrip());
constexpr int scope_cleanup(bool early){int calls=0;const auto body=[&]{cooperative_capture::detail::scope_exit guard([&]()noexcept{++calls;});if(early)return;};body();return calls;}
static_assert(scope_cleanup(true)==1&&scope_cleanup(false)==1);
struct FixtureWindow {const FixtureWindow* ParentWindowInBeginStack=nullptr;};
constexpr bool menu_ownership_fixture(){
    using namespace cooperative_capture::detail;
    FixtureWindow menu,hud,child{&menu},popup{&child},tooltip{&popup},other{&hud},cycle;cycle.ParentWindowInBeginStack=&cycle;
    return menu_ownership(&menu,&menu)==WindowOwnership::inside&&menu_ownership(&child,&menu)==WindowOwnership::inside&&
        menu_ownership(&popup,&menu)==WindowOwnership::inside&&menu_ownership(&tooltip,&menu)==WindowOwnership::inside&&
        menu_ownership(&hud,&menu)==WindowOwnership::outside&&menu_ownership(&other,&menu)==WindowOwnership::outside&&
        menu_ownership(&cycle,&menu)==WindowOwnership::invalid;
}
static_assert(menu_ownership_fixture());
void cooperative_capture_scope_exception_fixture(){int calls=0;try{cooperative_capture::detail::scope_exit guard([&]()noexcept{++calls;});throw 7;}catch(int){}assert(calls==1);}

// Source-only parser/admission fixtures. These assertions are COMPILED, never
// executed on the main PC. static_asserts above are compiler-evaluated.
void cooperative_capture_parser_fixtures(){
    const std::string request=R"({"protocol":1,"request_id":"00000000000000000000000000000001","job_id":"00000000000000000000000000000002","installation_id":"00000000000000000000000000000003","session_id":"00000000000000000000000000000004","target_pid":1234,"target_created_ticks":639000000000000000,"expected_build_id":"fixture-build","created_unix":100,"expires_unix":125,"requested_view":"overlay"})";
    const std::string context=R"({"protocol":1,"installation_id":"00000000000000000000000000000003","session_id":"00000000000000000000000000000004","target_pid":1234,"target_created_ticks":639000000000000000,"expected_build_id":"fixture-build","session_expires_unix":200})";
    Request r;Context c;std::string error;
    assert(parse_request(request,r,error)&&parse_context(context,c,error));
    assert(admit(r,c,1234,639000000000000000ULL,"fixture-build",110,error));
    assert(!admit(r,c,1235,639000000000000000ULL,"fixture-build",110,error));
    assert(!admit(r,c,1234,639000000000000001ULL,"fixture-build",110,error));
    assert(!admit(r,c,1234,639000000000000000ULL,"other-build",110,error));
    assert(!admit(r,c,1234,639000000000000000ULL,"fixture-build",125,error));
    c.session_id[0]='a';assert(!admit(r,c,1234,639000000000000000ULL,"fixture-build",110,error));
    auto invalid=request;invalid.insert(1,"\"protocol\":1,");assert(!parse_request(invalid,r,error));
    invalid=request;invalid.insert(1,"\"protoco\\u006c\":1,");assert(!parse_request(invalid,r,error));
    invalid=request;invalid.insert(1,"\"output_path\":\"C:/other.png\",");assert(!parse_request(invalid,r,error));
    invalid=request;invalid.replace(invalid.find("\"target_pid\":1234"),17,"\"target_pid\":true");assert(!parse_request(invalid,r,error));
    invalid=request;invalid.replace(invalid.find("\"target_pid\":1234"),17,"\"target_pid\":1234.0");assert(!parse_request(invalid,r,error));
    invalid=request;invalid.replace(invalid.find("\"target_pid\":1234"),17,"\"target_pid\":-1234");assert(!parse_request(invalid,r,error));
    invalid=request;invalid.replace(invalid.find("\"overlay\""),9,"\"desktop\"");assert(!parse_request(invalid,r,error));
    assert(!parse_request(request+"{}",r,error));
    assert(!parse_request(std::string(max_request_bytes+1,' '),r,error));
    assert(!parse_context(context.substr(0,context.size()-1),c,error));
}

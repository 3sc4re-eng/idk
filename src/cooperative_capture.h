#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <string>
#include <string_view>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct HWND__;

// Opt-in, one-shot capture of OUR existing overlay back buffer. The renderer is
// the sole D3D caller; the writer thread receives CPU bytes only. No game APIs.
namespace cooperative_capture {
namespace protocol {
inline constexpr std::uint64_t max_ttl_seconds=30;
inline constexpr std::size_t max_request_bytes=4096;
inline constexpr std::size_t max_png_bytes=8*1024*1024;
inline constexpr std::uint32_t max_dimension=4096;
inline constexpr std::uint64_t max_pixels=16777216;
constexpr bool id32(std::string_view value) {
    if(value.size()!=32)return false;
    for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
constexpr bool dimensions(std::uint32_t w,std::uint32_t h) {
    return w&&h&&w<=max_dimension&&h<=max_dimension&&std::uint64_t(w)*h<=max_pixels;
}
constexpr bool view(std::string_view value) {return value=="overlay"||value=="menu";}
constexpr bool request_time(std::uint64_t created,std::uint64_t expires,std::uint64_t now) {
    return created<=now&&expires>now&&expires>created&&expires-created<=max_ttl_seconds;
}
constexpr bool target_identity(std::uint32_t target_pid,std::uint64_t target_ticks,std::string_view target_build,
                               std::uint32_t own_pid,std::uint64_t own_ticks,std::string_view own_build) {
    return target_pid==own_pid&&target_ticks==own_ticks&&target_build==own_build;
}
// Fuser capture changes only alpha. Transparent capture retains associated
// framebuffer RGBA exactly, rather than pretending it is straight-alpha PNG.
constexpr std::uint8_t output_alpha(std::uint8_t stored,bool fuser) {return fuser?255:stored;}
// Storage-order conversion only. BGRA is supported by the native WIC PNG
// encoder; the encoded PNG's R/G/B/A channel values remain the source values.
constexpr std::array<std::uint8_t,4> rgba_to_bgra(std::array<std::uint8_t,4> pixel) {
    return {pixel[2],pixel[1],pixel[0],pixel[3]};
}
struct Binding {
    std::string installation_id,session_id,expected_build_id;
    std::uint32_t target_pid=0;
    std::uint64_t target_created_ticks=0;
};
struct Context:Binding {std::uint64_t session_expires_unix=0;};
struct Request:Binding {std::string request_id,job_id,requested_view;std::uint64_t created_unix=0,expires_unix=0;};
// Strict UTF-8 JSON: exact key sets, duplicate decoded keys rejected, unsigned
// integer tokens only (no bool/negative/floating/string coercion), <=4096 bytes.
bool parse_request(std::string_view bytes,Request& out,std::string& error);
bool parse_context(std::string_view bytes,Context& out,std::string& error);
bool admit(const Request&,const Context&,std::uint32_t own_pid,std::uint64_t own_created_ticks,
           std::string_view own_build,std::uint64_t now,std::string& error);
}

class Service {
public:
    using Log=void(*)(const char*);
    Service();
    ~Service();
    Service(const Service&)=delete;
    Service& operator=(const Service&)=delete;
    // Does not create the private directory. Missing/unsafe context means no
    // capture. Call after the overlay's COM initialization on its render thread.
    bool initialize(std::string_view compiled_build_id,Log logger=nullptr) noexcept;
    // Call once before menu drawing. True suppresses MENU DRAWING for this one
    // requested overlay frame only. Never change the real menu/input-gate state.
    // A menu-view request requires the real menu already open; no input is sent.
    bool prepare_frame(bool actual_menu_open) noexcept;
    // The renderer calls this instead of copying when menu draw-list exclusion
    // cannot be proven. Normal UI submission/rendering continues unchanged.
    void reject_prepared(std::string_view detail) noexcept;
    // Call exactly after ImGui_ImplDX11_RenderDrawData and before Present.
    // False fuser means the actual transparent overlay mode, NOT game content.
    void after_draw(ID3D11Device*,ID3D11DeviceContext*,IDXGISwapChain*,HWND__* own_window,
                    bool fuser,std::uint64_t render_frame_id) noexcept;
    // Call on that same thread before releasing the device/context/COM. Pending
    // jobs fail explicitly; CPU-only writer is joined before returning.
    void shutdown() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
namespace detail {
template<class Callback>class scope_exit {
    Callback callback_;
public:
    explicit constexpr scope_exit(Callback callback):callback_(callback){}
    scope_exit(const scope_exit&)=delete;
    constexpr ~scope_exit()noexcept {callback_();}
};
}
}

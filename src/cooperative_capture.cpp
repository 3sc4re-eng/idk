#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wincodec.h>
#include <wincrypt.h>
#include <objidl.h>
#include <aclapi.h>
#include "cooperative_capture.h"
#include "../deps/json.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <set>
#include <thread>
#include <utility>
#include <vector>

namespace cooperative_capture {
namespace {
using Json=nlohmann::json;
template<class T>struct Com {
    T* p=nullptr;
    ~Com(){reset();}
    Com()=default;Com(const Com&)=delete;Com& operator=(const Com&)=delete;
    void reset(){if(p){p->Release();p=nullptr;}}
    T** put(){reset();return &p;}
    T* operator->()const{return p;}
};
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    ~Handle(){reset();}
    void reset(HANDLE h=INVALID_HANDLE_VALUE){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=h;}
    explicit operator bool()const{return value!=INVALID_HANDLE_VALUE;}
};
bool build_id(std::string_view s){
    if(s.empty()||s.size()>96)return false;
    for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='.'||c=='_'||c=='-'))return false;
    return true;
}
bool json_document(std::string_view bytes,Json& out,std::string& error){
    if(bytes.empty()||bytes.size()>protocol::max_request_bytes){error="JSON size bound";return false;}
    // No BOM, comments, trailing values, duplicate decoded keys, or deep JSON.
    if(bytes.size()>=3&&std::uint8_t(bytes[0])==0xef&&std::uint8_t(bytes[1])==0xbb&&std::uint8_t(bytes[2])==0xbf){error="JSON BOM unsupported";return false;}
    bool duplicate=false,deep=false;
    std::vector<std::set<std::string>> keys;
    try {
        auto callback=[&](int depth,Json::parse_event_t event,Json& value){
            if(depth>3)deep=true;
            if(event==Json::parse_event_t::object_start)keys.emplace_back();
            else if(event==Json::parse_event_t::key){if(keys.empty()||!keys.back().insert(value.get<std::string>()).second)duplicate=true;}
            else if(event==Json::parse_event_t::object_end&&!keys.empty())keys.pop_back();
            return true;
        };
        out=Json::parse(bytes.begin(),bytes.end(),callback,true,false);
    } catch(const std::exception&){error="Invalid JSON";return false;}
    if(duplicate||deep||!out.is_object()){error=duplicate?"Duplicate JSON key":"Expected shallow JSON object";return false;}
    return true;
}
bool exact_keys(const Json& value,std::initializer_list<const char*> expected){
    if(value.size()!=expected.size())return false;
    for(const auto* key:expected)if(!value.contains(key))return false;
    return true;
}
bool unsigned_value(const Json& j,const char* key,std::uint64_t& value){
    if(!j.contains(key)||!j[key].is_number_unsigned())return false;
    value=j[key].get<std::uint64_t>();return true;
}
bool string_value(const Json& j,const char* key,std::string& value){
    if(!j.contains(key)||!j[key].is_string())return false;
    value=j[key].get<std::string>();return true;
}
bool binding(const Json& j,protocol::Binding& out){
    std::uint64_t pid=0,version=0;
    return unsigned_value(j,"protocol",version)&&version==1&&
        string_value(j,"installation_id",out.installation_id)&&protocol::id32(out.installation_id)&&
        string_value(j,"session_id",out.session_id)&&protocol::id32(out.session_id)&&
        string_value(j,"expected_build_id",out.expected_build_id)&&build_id(out.expected_build_id)&&
        unsigned_value(j,"target_pid",pid)&&pid>0&&pid<=0x7fffffff&&
        (out.target_pid=static_cast<std::uint32_t>(pid),true)&&
        unsigned_value(j,"target_created_ticks",out.target_created_ticks)&&out.target_created_ticks>621355968000000000ULL&&out.target_created_ticks<3155378976000000000ULL;
}
std::uint64_t unix_seconds(){FILETIME t{};GetSystemTimeAsFileTime(&t);const auto value=(std::uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;return value/10000000ULL-11644473600ULL;}
std::uint64_t creation_ticks(){FILETIME c{},e{},k{},u{};if(!GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u))return 0;return ((std::uint64_t(c.dwHighDateTime)<<32)|c.dwLowDateTime)+504911232000000000ULL;}
std::string utc_now(){SYSTEMTIME t{};GetSystemTime(&t);char text[40]{};std::snprintf(text,sizeof(text),"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);return text;}
std::string hr_text(HRESULT hr){char text[20]{};std::snprintf(text,sizeof(text),"0x%08lX",static_cast<unsigned long>(hr));return text;}
bool regular_handle(HANDLE file,bool directory){
    BY_HANDLE_FILE_INFORMATION info{};
    return GetFileType(file)==FILE_TYPE_DISK&&GetFileInformationByHandle(file,&info)&&
        !(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&
        bool(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==directory&&(directory||info.nNumberOfLinks==1);
}
bool private_directory(HANDLE directory){
    Handle token;HANDLE raw=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&raw))return false;token.reset(raw);
    DWORD bytes=0;GetTokenInformation(token.value,TokenUser,nullptr,0,&bytes);
    std::vector<std::uint8_t> identity(bytes);
    if(!bytes||!GetTokenInformation(token.value,TokenUser,identity.data(),bytes,&bytes))return false;
    const auto current=reinterpret_cast<const TOKEN_USER*>(identity.data())->User.Sid;
    std::array<std::uint8_t,SECURITY_MAX_SID_SIZE> system{};DWORD system_size=static_cast<DWORD>(system.size());
    if(!CreateWellKnownSid(WinLocalSystemSid,nullptr,system.data(),&system_size))return false;
    PSECURITY_DESCRIPTOR descriptor=nullptr;PSID owner=nullptr;PACL dacl=nullptr;
    const DWORD status=GetSecurityInfo(directory,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&dacl,nullptr,&descriptor);
    if(status!=ERROR_SUCCESS)return false;
    SECURITY_DESCRIPTOR_CONTROL control{};DWORD revision=0;
    bool ok=owner&&EqualSid(owner,current)&&dacl&&dacl->AceCount>0&&GetSecurityDescriptorControl(descriptor,&control,&revision)&&(control&SE_DACL_PROTECTED);
    for(DWORD i=0;ok&&i<dacl->AceCount;++i){void* raw_ace=nullptr;if(!GetAce(dacl,i,&raw_ace)){ok=false;break;}const auto* ace=static_cast<const ACCESS_ALLOWED_ACE*>(raw_ace);
        if(ace->Header.AceType!=ACCESS_ALLOWED_ACE_TYPE){ok=false;break;}
        const auto sid=const_cast<DWORD*>(&ace->SidStart);
        if(!EqualSid(sid,current)&&!EqualSid(sid,system.data()))ok=false;
    }
    LocalFree(descriptor);return ok;
}
bool safe_parents(std::filesystem::path path){
    const auto root=path.root_path().wstring();
    if(root.size()!=3||root[1]!=L':'||GetDriveTypeW(root.c_str())!=DRIVE_FIXED)return false;
    for(;;){const DWORD attr=GetFileAttributesW(path.c_str());if(attr==INVALID_FILE_ATTRIBUTES||!(attr&FILE_ATTRIBUTE_DIRECTORY)||(attr&FILE_ATTRIBUTE_REPARSE_POINT))return false;auto parent=path.parent_path();if(parent==path||parent.empty())break;path=std::move(parent);}
    return true;
}
bool read_small(const std::filesystem::path& path,std::string& bytes){
    Handle file;file.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    LARGE_INTEGER size{};
    if(!file||!regular_handle(file.value,false)||!GetFileSizeEx(file.value,&size)||size.QuadPart<1||size.QuadPart>static_cast<LONGLONG>(protocol::max_request_bytes))return false;
    bytes.resize(static_cast<std::size_t>(size.QuadPart));DWORD got=0;
    return ReadFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&got,nullptr)&&got==bytes.size();
}
bool write_new(const std::filesystem::path& path,const void* bytes,std::size_t size){
    Handle file;file.value=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(!file||!regular_handle(file.value,false))return false;
    const auto* input=static_cast<const std::uint8_t*>(bytes);
    for(std::size_t done=0;done<size;){const DWORD next=static_cast<DWORD>(std::min<std::size_t>(size-done,1024*1024));DWORD written=0;if(!WriteFile(file.value,input+done,next,&written,nullptr)||written!=next)return false;done+=written;}
    return FlushFileBuffers(file.value)!=FALSE;
}
bool atomic_new(const std::filesystem::path& final,const void* bytes,std::size_t size){
    auto temporary=final;temporary+=L".partial";
    if(!write_new(temporary,bytes,size))return false;
    // No REPLACE_EXISTING: an already accepted output is immutable. WRITE_THROUGH
    // completes the rename before success; PNG is committed before its receipt.
    if(MoveFileExW(temporary.c_str(),final.c_str(),MOVEFILE_WRITE_THROUGH))return true;
    DeleteFileW(temporary.c_str());return false;
}
bool atomic_json(const std::filesystem::path& final,const Json& j){const auto text=j.dump(2);return atomic_new(final,text.data(),text.size());}
Json bindings(const protocol::Request& r){return {{"protocol",1},{"request_id",r.request_id},{"job_id",r.job_id},{"installation_id",r.installation_id},{"session_id",r.session_id},{"target_pid",r.target_pid},{"target_created_ticks",r.target_created_ticks},{"expected_build_id",r.expected_build_id},{"created_unix",r.created_unix},{"expires_unix",r.expires_unix},{"requested_view",r.requested_view}};}
std::string sha256(const std::vector<std::uint8_t>& bytes){
    std::array<BYTE,32> digest{};DWORD length=static_cast<DWORD>(digest.size());
    if(!CryptHashCertificate2(L"SHA256",0,nullptr,bytes.data(),static_cast<DWORD>(bytes.size()),digest.data(),&length)||length!=digest.size())return {};
    static constexpr char digits[]="0123456789abcdef";std::string value;value.reserve(64);for(BYTE b:digest){value+=digits[b>>4];value+=digits[b&15];}return value;
}
struct CpuFrame {protocol::Request request;Json metadata;std::vector<std::uint8_t> rgba;std::uint32_t width=0,height=0;};
bool encode_png(CpuFrame& frame,std::vector<std::uint8_t>& png,HRESULT& error){
    Com<IWICImagingFactory> factory;Com<IStream> stream;Com<IWICBitmapEncoder> encoder;Com<IWICBitmapFrameEncode> image;Com<IPropertyBag2> options;
    error=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.put()));if(FAILED(error))return false;
    error=CreateStreamOnHGlobal(nullptr,TRUE,stream.put());if(FAILED(error))return false;
    error=factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.put());if(FAILED(error))return false;
    error=encoder->Initialize(stream.p,WICBitmapEncoderNoCache);if(FAILED(error))return false;
    error=encoder->CreateNewFrame(image.put(),options.put());if(FAILED(error))return false;
    error=image->Initialize(options.p);if(FAILED(error))return false;
    error=image->SetSize(frame.width,frame.height);if(FAILED(error))return false;
    WICPixelFormatGUID pixel=GUID_WICPixelFormat32bppBGRA;
    error=image->SetPixelFormat(&pixel);if(FAILED(error)||!IsEqualGUID(pixel,GUID_WICPixelFormat32bppBGRA)){error=WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;return false;}
    // The native PNG encoder accepts BGRA, not32bppRGBA. Reorder storage bytes
    // on this CPU-only worker; do not use a premultiplied format/converter or
    // alter the numerical R/G/B/A samples, gamma or dimensions. This frame is
    // encoded once and never reused as an RGBA buffer afterwards.
    for(std::size_t i=0;i<frame.rgba.size();i+=4){const auto bgra=protocol::rgba_to_bgra({frame.rgba[i],frame.rgba[i+1],frame.rgba[i+2],frame.rgba[i+3]});std::memcpy(frame.rgba.data()+i,bgra.data(),4);}
    error=image->WritePixels(frame.height,frame.width*4,static_cast<UINT>(frame.rgba.size()),frame.rgba.data());if(FAILED(error))return false;
    error=image->Commit();if(FAILED(error))return false;
    error=encoder->Commit();if(FAILED(error))return false;
    STATSTG stat{};error=stream->Stat(&stat,STATFLAG_NONAME);if(FAILED(error))return false;
    if(!stat.cbSize.QuadPart||stat.cbSize.QuadPart>protocol::max_png_bytes){error=HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);return false;}
    LARGE_INTEGER zero{};error=stream->Seek(zero,STREAM_SEEK_SET,nullptr);if(FAILED(error))return false;
    png.resize(static_cast<std::size_t>(stat.cbSize.QuadPart));ULONG count=0;error=stream->Read(png.data(),static_cast<ULONG>(png.size()),&count);
    return SUCCEEDED(error)&&count==png.size();
}
}

namespace protocol {
bool parse_request(std::string_view bytes,Request& out,std::string& error){
    Json j;Request candidate;
    if(!json_document(bytes,j,error))return false;
    if(!exact_keys(j,{"protocol","request_id","job_id","installation_id","session_id","target_pid","target_created_ticks","expected_build_id","created_unix","expires_unix","requested_view"})||!binding(j,candidate)||
       !string_value(j,"request_id",candidate.request_id)||!id32(candidate.request_id)||!string_value(j,"job_id",candidate.job_id)||!id32(candidate.job_id)||
       !string_value(j,"requested_view",candidate.requested_view)||!view(candidate.requested_view)||
       !unsigned_value(j,"created_unix",candidate.created_unix)||!unsigned_value(j,"expires_unix",candidate.expires_unix)){error="Invalid capture request fields";return false;}
    out=std::move(candidate);error.clear();return true;
}
bool parse_context(std::string_view bytes,Context& out,std::string& error){
    Json j;Context candidate;
    if(!json_document(bytes,j,error))return false;
    if(!exact_keys(j,{"protocol","installation_id","session_id","target_pid","target_created_ticks","expected_build_id","session_expires_unix"})||!binding(j,candidate)||!unsigned_value(j,"session_expires_unix",candidate.session_expires_unix)){error="Invalid cooperative context fields";return false;}
    out=std::move(candidate);error.clear();return true;
}
bool admit(const Request& r,const Context& c,std::uint32_t pid,std::uint64_t ticks,std::string_view build,std::uint64_t now,std::string& error){
    if(!target_identity(r.target_pid,r.target_created_ticks,r.expected_build_id,pid,ticks,build)){error="Request targets a different ACE process/build";return false;}
    if(!target_identity(c.target_pid,c.target_created_ticks,c.expected_build_id,pid,ticks,build)||c.installation_id!=r.installation_id||c.session_id!=r.session_id){error="Cooperative context binding mismatch";return false;}
    if(!request_time(r.created_unix,r.expires_unix,now)||c.session_expires_unix<=now||r.expires_unix>c.session_expires_unix){error="Request/session expired or outside 30-second window";return false;}
    error.clear();return true;
}
}

struct Service::Impl {
    std::filesystem::path directory;
    Handle directory_handle;
    std::string build,last_document;
    std::uint32_t pid=0;std::uint64_t ticks=0,last_check=0;
    DWORD owner_thread=0;Log logger=nullptr;
    bool initialized=false,armed=false,pending=false;
    Com<ID3D11Texture2D> staging;Com<ID3D11Query> ready;
    protocol::Request request;Json metadata;
    std::uint64_t copy_started=0;std::uint32_t width=0,height=0;
    bool fuser=false;
    std::thread writer;std::atomic<bool> writer_done{false};std::string writer_report;
    void log(const std::string& text){if(logger)logger(text.c_str());}
    std::filesystem::path output(const std::string& id,const wchar_t* suffix)const{return directory/(L"capture-"+std::wstring(id.begin(),id.end())+suffix);}
    bool context(protocol::Context& value,std::string& error){std::string bytes;if(!read_small(directory/L"cooperative-context.json",bytes)){error="Cooperative context unavailable";return false;}return protocol::parse_context(bytes,value,error);}
    bool current_admission(const protocol::Request& r,std::string& error){protocol::Context c;return context(c,error)&&protocol::admit(r,c,pid,ticks,build,unix_seconds(),error);}
    void failure(const protocol::Request& r,const Json& base,std::string stage,std::string detail,HRESULT hr=E_FAIL){
        Json value=base;value["outcome"]="unavailable";value["stage"]=std::move(stage);value["detail"]=std::move(detail);value["hresult"]=hr_text(hr);value["completed_utc"]=utc_now();
        if(!atomic_json(output(r.request_id,L".json"),value))log("Cooperative capture: failure metadata could not be committed; durable claim prevents retry.");
    }
    void reset_gpu(){staging.reset();ready.reset();pending=false;}
    void fail_pending(std::string stage,std::string detail,HRESULT hr=E_FAIL){failure(request,metadata,std::move(stage),std::move(detail),hr);reset_gpu();}
    void finish_writer(){if(writer.joinable()&&writer_done.load(std::memory_order_acquire)){writer.join();log(writer_report);writer_report.clear();}}
    Json base_metadata(const protocol::Request& r){Json value=bindings(r);value["build_id"]=build;value["process_id"]=pid;value["created_ticks"]=ticks;value["method"]="ACE/D3D11-post-ImGui-pre-Present";value["scope"]="ACE-owned overlay backbuffer only; no native game image";return value;}
    bool claim(const protocol::Request& r){auto value=bindings(r);value["state"]="claimed_before_gpu_copy";value["claimed_utc"]=utc_now();const auto text=value.dump();
        const auto job=directory/(L"capture-job-"+std::wstring(r.job_id.begin(),r.job_id.end())+L".claim.json");
        return write_new(job,text.data(),text.size())&&write_new(output(r.request_id,L".claim.json"),text.data(),text.size());
    }
    bool open_directory(){
        if(directory_handle)return true;
        if(!safe_parents(directory))return false;
        directory_handle.value=CreateFileW(directory.c_str(),FILE_READ_ATTRIBUTES|READ_CONTROL,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(!directory_handle||!regular_handle(directory_handle.value,true)||!private_directory(directory_handle.value)){directory_handle.reset();return false;}
        return true;
    }
    bool prepare(bool actual_menu_open){
        finish_writer();if(pending||armed||writer.joinable())return false;
        const auto now=GetTickCount64();if(now-last_check<250)return false;last_check=now;
        if(!open_directory())return false;
        std::string bytes;if(!read_small(directory/L"capture-request.json",bytes)||bytes==last_document)return false;
        last_document=bytes;
        protocol::Request r;std::string error;
        if(!protocol::parse_request(bytes,r,error)){log("Cooperative capture rejected before GPU: "+error);return false;}
        // Another ACE instance must not consume or claim a job addressed elsewhere.
        if(!protocol::target_identity(r.target_pid,r.target_created_ticks,r.expected_build_id,pid,ticks,build))return false;
        if(!claim(r)){log("Cooperative capture: request already claimed or claim write failed; no GPU copy.");return false;}
        auto meta=base_metadata(r);
        if(!current_admission(r,error)){failure(r,meta,"admission",error);return false;}
        if(r.requested_view=="menu"&&!actual_menu_open){failure(r,meta,"requested_view","Menu view requested but the actual ACE menu is closed; no input was sent");return false;}
        meta["menu_open_actual"]=actual_menu_open;meta["menu_draw_suppressed"]=r.requested_view=="overlay"&&actual_menu_open;
        request=std::move(r);metadata=std::move(meta);armed=true;return request.requested_view=="overlay";
    }
    void begin(ID3D11Device* device,ID3D11DeviceContext* ctx,IDXGISwapChain* swap,HWND window,bool is_fuser,std::uint64_t frame_id){
        auto r=request;auto meta=metadata;std::string error;
        if(!current_admission(r,error)){failure(r,meta,"admission_at_draw",error);return;}
        DWORD actual_pid=0;wchar_t name[64]{};
        if(!window||!GetWindowThreadProcessId(window,&actual_pid)||actual_pid!=pid||!IsWindowVisible(window)||IsIconic(window)||GetClassNameW(window,name,64)==0||std::wstring_view(name)!=L"ACEOverlay"){
            failure(r,meta,"window","Owned visible ACEOverlay window unavailable");return;
        }
        Com<ID3D11Device> context_device,swap_device;
        ctx->GetDevice(context_device.put());HRESULT hr=swap->GetDevice(IID_PPV_ARGS(swap_device.put()));
        if(FAILED(hr)||context_device.p!=device||swap_device.p!=device||ctx->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE){failure(r,meta,"device","Existing renderer device/context/swapchain mismatch",hr);return;}
        Com<ID3D11Texture2D> source;hr=swap->GetBuffer(0,IID_PPV_ARGS(source.put()));
        if(FAILED(hr)){failure(r,meta,"backbuffer","GetBuffer failed",hr);return;}
        D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
        if(!protocol::dimensions(desc.Width,desc.Height)||desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||desc.SampleDesc.Count!=1||desc.SampleDesc.Quality!=0||desc.MipLevels!=1||desc.ArraySize!=1){failure(r,meta,"format","Only bounded single-sample RGBA8_UNORM backbuffers are supported");return;}
        RECT client{};POINT a{},b{};
        if(!GetClientRect(window,&client)||client.right-client.left!=static_cast<LONG>(desc.Width)||client.bottom-client.top!=static_cast<LONG>(desc.Height)){
            failure(r,meta,"dimensions","Client and framebuffer dimensions disagree; no guessed scaling");return;
        }
        b={client.right,client.bottom};
        if(!ClientToScreen(window,&a)||!ClientToScreen(window,&b)){failure(r,meta,"rectangle","ACE client rectangle unavailable");return;}
        meta["client_screen_rect_logical"]={a.x,a.y,b.x,b.y};
        using PhysicalPoint=BOOL(WINAPI*)(HWND,LPPOINT);
        PhysicalPoint convert=nullptr;
        const auto entry=GetProcAddress(GetModuleHandleW(L"user32.dll"),"LogicalToPhysicalPointForPerMonitorDPI");
        static_assert(sizeof(convert)==sizeof(entry));std::memcpy(&convert,&entry,sizeof(convert));
        if(convert&&convert(window,&a)&&convert(window,&b)){
            meta["client_screen_rect_physical"]={a.x,a.y,b.x,b.y};
            const bool one_to_one=b.x-a.x==static_cast<LONG>(desc.Width)&&b.y-a.y==static_cast<LONG>(desc.Height);
            if(one_to_one){meta["rect"]={a.x,a.y,b.x,b.y};meta["coordinate_space"]="ACE client physical screen pixels";meta["placement_verified"]=true;meta["placement_detail"]="Client physical extent equals unscaled framebuffer extent";}
            else {meta["rect"]={0,0,desc.Width,desc.Height};meta["coordinate_space"]="framebuffer pixels";meta["placement_verified"]=false;meta["placement_detail"]="Physical client extent differs from framebuffer; no 1:1 alignment or resize performed";}
        }
        else {meta["client_screen_rect_physical"]=nullptr;meta["rect"]={0,0,desc.Width,desc.Height};meta["coordinate_space"]="framebuffer pixels";meta["placement_verified"]=false;meta["placement_detail"]="Physical screen placement unavailable";}
        meta["framebuffer_rect"]={0,0,desc.Width,desc.Height};meta["width"]=desc.Width;meta["height"]=desc.Height;
        meta["source_dxgi_format"]="DXGI_FORMAT_R8G8B8A8_UNORM";meta["source_sample_count"]=desc.SampleDesc.Count;
        meta["render_frame_id"]=frame_id;meta["captured_utc"]=utc_now();meta["captured_unix"]=unix_seconds();meta["captured_tick_ms"]=GetTickCount64();
        meta["overlay_mode"]=is_fuser?"fuser_rgb_opaque":"transparent_overlay";
        meta["alpha_semantics"]=is_fuser?"PNG alpha forced to 255; owned framebuffer RGB retained exactly":"stored framebuffer associated RGB/alpha retained exactly; PNG alpha is not straight-alpha; no unpremultiplication";
        meta["color_semantics"]="RGBA8 UNORM stored bytes; no scaling, gamma transform, color correction, or game compositing";
        meta["window_class"]="ACEOverlay";meta["window_title"]="AceSoftware";
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
        hr=device->CreateTexture2D(&desc,nullptr,staging.put());if(FAILED(hr)){failure(r,meta,"staging","CreateTexture2D failed",hr);return;}
        D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};hr=device->CreateQuery(&query,ready.put());
        if(FAILED(hr)){staging.reset();failure(r,meta,"query","CreateQuery failed",hr);return;}
        // Re-read authorization immediately before the ONLY GPU copy. The render
        // thread and existing immediate context remain the sole D3D owners.
        if(!current_admission(r,error)){staging.reset();ready.reset();failure(r,meta,"admission",error);return;}
        request=std::move(r);metadata=std::move(meta);width=desc.Width;height=desc.Height;fuser=is_fuser;copy_started=GetTickCount64();
        ctx->CopyResource(staging.p,source.p);ctx->End(ready.p);pending=true;
    }
    void drain(ID3D11DeviceContext* ctx){
        std::string error;
        if(!current_admission(request,error)){fail_pending("admission_after_copy",error);return;}
        if(GetTickCount64()-copy_started>2000){fail_pending("gpu_timeout","Staging copy did not become readable within 2 seconds");return;}
        BOOL complete=FALSE;HRESULT hr=ctx->GetData(ready.p,&complete,sizeof(complete),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(hr==S_FALSE||(SUCCEEDED(hr)&&!complete))return;
        if(FAILED(hr)){fail_pending("gpu_query","Staging completion query failed",hr);return;}
        // Every potentially allocating CPU-state copy happens BEFORE Map.
        CpuFrame frame;frame.request=request;frame.metadata=metadata;frame.width=width;frame.height=height;
        frame.rgba.resize(std::size_t(width)*height*4);
        D3D11_MAPPED_SUBRESOURCE map{};hr=ctx->Map(staging.p,0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&map);
        if(hr==DXGI_ERROR_WAS_STILL_DRAWING)return;
        if(FAILED(hr)){fail_pending("map","Staging Map failed",hr);return;}
        const bool rows_valid=map.pData&&map.RowPitch>=width*4;
        {
            // Constructed immediately after successful Map, with no allocations.
            // This scope ALWAYS ends before fail_pending/reset_gpu can release
            // staging, including exceptions and the invalid-row branch.
            ID3D11Texture2D* const mapped_resource=staging.p;
            detail::scope_exit unmap([ctx,mapped_resource]()noexcept{ctx->Unmap(mapped_resource,0);});
            if(rows_valid)for(std::uint32_t y=0;y<height;++y){auto* row=frame.rgba.data()+std::size_t(y)*width*4;std::memcpy(row,static_cast<const std::uint8_t*>(map.pData)+std::size_t(y)*map.RowPitch,std::size_t(width)*4);if(fuser)for(std::uint32_t x=0;x<width;++x)row[4*x+3]=protocol::output_alpha(row[4*x+3],true);}
        }
        if(!rows_valid){fail_pending("row_pitch","Mapped rows are incomplete");return;}
        reset_gpu();
        if(!current_admission(frame.request,error)){failure(frame.request,frame.metadata,"admission_before_encode",error);return;}
        writer_done=false;
        try {writer=std::thread([this,frame=std::move(frame)]()mutable{
            const HRESULT initialized_com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            try {
                if(FAILED(initialized_com)){failure(frame.request,frame.metadata,"wic_com","Writer COM initialization failed",initialized_com);writer_report="Cooperative capture unavailable: writer COM.";}
                else {
                    std::vector<std::uint8_t> png;HRESULT hr=E_FAIL;
                    if(!encode_png(frame,png,hr)){failure(frame.request,frame.metadata,"png_encode","Lossless RGBA PNG failed or exceeded the 8 MiB transport bound",hr);writer_report="Cooperative capture unavailable: PNG encoding/bound.";}
                    else {
                        const auto digest=sha256(png);
                        // Check before PNG output and AGAIN after its potentially
                        // slow durable flush, immediately before success receipt.
                        std::string error;
                        if(digest.empty()||!current_admission(frame.request,error)){failure(frame.request,frame.metadata,"admission_before_commit",digest.empty()?"PNG SHA-256 failed":error);writer_report="Cooperative capture unavailable before output commit.";}
                        else if(!atomic_new(output(frame.request.request_id,L".png"),png.data(),png.size())){failure(frame.request,frame.metadata,"png_commit","Durable immutable PNG commit failed");writer_report="Cooperative capture unavailable: PNG commit.";}
                        else {
                            Json success=frame.metadata;
                            success["outcome"]="captured";success["stage"]="complete";success["detail"]="Actual ACE-owned framebuffer copied losslessly; native game/composite not captured or verified";
                            success["png_sha256"]=digest;success["png_bytes"]=png.size();success["png_file"]="capture-"+frame.request.request_id+".png";success["completed_utc"]=utc_now();
                            if(!current_admission(frame.request,error)){failure(frame.request,frame.metadata,"admission_after_png_commit",error);writer_report="Cooperative capture unavailable: authorization changed during PNG commit; orphan PNG is not success.";}
                            else writer_report=atomic_json(output(frame.request.request_id,L".json"),success)?"Cooperative capture: PNG and metadata committed.":"Cooperative capture: PNG committed but metadata commit failed; claim prevents repeat.";
                        }
                    }
                }
            }catch(...){try{failure(frame.request,frame.metadata,"writer_exception","CPU encoder/output exception");}catch(...){}writer_report="Cooperative capture unavailable: CPU writer exception.";}
            if(SUCCEEDED(initialized_com))CoUninitialize();
            writer_done.store(true,std::memory_order_release);
        });}catch(...){failure(request,metadata,"writer_start","CPU writer thread could not start");}
    }
};

Service::Service()=default;
Service::~Service(){shutdown();}
bool Service::initialize(std::string_view build,Log logger)noexcept{
    try {
        shutdown();auto state=std::make_unique<Impl>();state->logger=logger;
        if(!build_id(build))return false;
        std::vector<wchar_t> executable(32768);const DWORD n=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));if(!n||n>=executable.size())return false;
        state->directory=std::filesystem::path(std::wstring(executable.data(),n)).parent_path()/L".ace-lifecycle";
        // The helper creates this directory with current-user/SYSTEM-only ACL.
        // The product never creates a less-private replacement or reads secrets.
        // Directory may be prepared by enrollment after ACE starts; opening it is
        // retried at the same 250 ms idle cadence, with strict ACL/link checks.
        state->open_directory();
        state->pid=GetCurrentProcessId();state->ticks=creation_ticks();if(!state->ticks)return false;
        state->build=build;state->owner_thread=GetCurrentThreadId();state->initialized=true;impl_=std::move(state);return true;
    }catch(...){return false;}
}
bool Service::prepare_frame(bool actual_menu_open)noexcept{
    if(!impl_||!impl_->initialized||impl_->owner_thread!=GetCurrentThreadId())return false;
    try{return impl_->prepare(actual_menu_open);}catch(...){try{if(impl_->armed){impl_->armed=false;impl_->failure(impl_->request,impl_->metadata,"prepare_exception","Request preparation exception");}}catch(...){}return false;}
}
void Service::reject_prepared(std::string_view detail)noexcept{
    if(!impl_||!impl_->armed||impl_->owner_thread!=GetCurrentThreadId())return;
    impl_->armed=false;
    try{impl_->failure(impl_->request,impl_->metadata,"menu_draw_filter",std::string(detail));}catch(...){}
}
void Service::after_draw(ID3D11Device* device,ID3D11DeviceContext* ctx,IDXGISwapChain* swap,HWND__* window,bool fuser,std::uint64_t frame)noexcept{
    if(!impl_||!impl_->initialized||impl_->owner_thread!=GetCurrentThreadId()||!device||!ctx||!swap)return;
    try {
        auto& s=*impl_;s.finish_writer();
        if(s.pending){s.drain(ctx);return;}
        if(s.armed){
            s.armed=false;
            try{s.begin(device,ctx,swap,window,fuser,frame);}
            catch(...){s.failure(s.request,s.metadata,"capture_exception","Capture allocation/API exception");s.reset_gpu();}
        }
    }catch(...){try{if(impl_->pending)impl_->fail_pending("capture_exception","Capture allocation/API exception");else impl_->log("Cooperative capture: request processing exception; no synthetic output.");}catch(...){impl_->reset_gpu();}}
}
void Service::shutdown()noexcept{
    if(!impl_)return;
    try {
        if(impl_->pending)impl_->fail_pending("renderer_shutdown","Renderer stopped before GPU readback completed");
        if(impl_->armed){impl_->armed=false;impl_->failure(impl_->request,impl_->metadata,"renderer_shutdown","Renderer stopped before the requested frame was copied");}
        if(impl_->writer.joinable()){impl_->writer.join();impl_->log(impl_->writer_report);}
    }catch(...){}
    impl_.reset();
}
}

#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// Standalone proposal: no process handles, no target writes, no live execution.
namespace incremental_discovery {
using Address=std::uint64_t;
// Zero permanently requests a resync after unsigned saturation; a revision is
// never silently reused. These are host membership/metadata revisions only.
constexpr std::uint64_t next_identity_revision(std::uint64_t revision)noexcept{return revision?revision+1:0;}
constexpr bool identity_revision_changed(std::uint64_t copied,std::uint64_t current)noexcept{return !current||copied!=current;}
struct Request {
    Address address=0;
    std::uint8_t* destination=nullptr;
    std::uint32_t size=0;
    // True only if EVERY requested byte was acquired. Never zero-pad success.
    bool complete=false;
};
using Read=std::function<bool(Address,void*,std::uint32_t)>;
using Scatter=std::function<bool(std::vector<Request>&)>;
struct Class {int kind=0;std::string name;};
using Classify=std::function<bool(std::uint32_t vtable_rva,Class&)>;
struct Identity {
    Address entity=0,record=0,vtable=0;
    std::uint32_t handle=0;
    int kind=0;
    std::string name;
    bool operator==(const Identity&)const=default;
};
struct Config {
    Address entities_global=0,client_base=0;
    std::uint32_t client_size=0;
    std::uint64_t session_token=0;
    std::uint32_t stride=0x70,entries_per_chunk=512,chunk_count=64;
    std::uint32_t chunk_table_offset=0x10,handle_offset=0x10,flags_offset=0x30;
    std::uint32_t object_identity_offset=0x10,reject_flags=0x717,index_mask=0x7fff;
    std::uint32_t max_headers_per_step=128;
    std::uint64_t cycle_interval_ms=1000;
};
struct Diagnostics {
    std::uint64_t cycle=0,attempted_ms=0,completed_ms=0,interval_ms=0;
    std::uint32_t chunks=0,invalid_chunks=0,pages_read=0,pages_unavailable=0;
    std::uint32_t records_read=0,empty_records=0,flags_rejected=0,invalid_addresses=0,serial_mismatches=0;
    std::uint32_t candidates=0,object_unreadable=0,vtable_outside_client=0,unknown_classes=0,known_classes=0,identity_mismatches=0;
    std::uint32_t accepted=0,pawns=0,controllers=0,step_headers=0,step_pages=0;
    std::uint32_t direct_calls=0,scatter_calls=0;
    Address first_unavailable_page=0,first_unknown_vtable=0;
    bool active=false,complete=false;
    std::string status="not started";
};
class Engine {
public:
    Engine(Read read,Scatter scatter,Classify classify);
    // Call on reconnect/PID/module/layout change, manual recache, or reset.
    void reset(const Config&);
    // Call every reader iteration, including while a cycle is in progress.
    // At most ONE allocated chunk and max_headers_per_step candidates are read.
    // The cycle interval delays only a NEW cycle, never a continuation step.
    bool step(std::uint64_t now_ms);
    void set_cycle_interval(std::uint64_t ms){config_.cycle_interval_ms=ms<=60000?ms:60000;}
    const std::unordered_map<std::uint32_t,Identity>& identities() const{return identities_;}
    std::uint64_t identity_revision()const noexcept{return identity_revision_;}
    const Diagnostics& diagnostics() const{return diagnostics_;}
    bool active()const{return diagnostics_.active;}
private:
    struct Candidate {Address entity=0,record=0;std::uint32_t handle=0,index=0;};
    Read read_;Scatter scatter_;Classify classify_;Config config_;
    std::unordered_map<std::uint32_t,Identity> identities_;
    std::uint64_t identity_revision_=1;
    void erase_identity(std::uint32_t index);
    void put_identity(std::uint32_t index,Identity value);
    std::unordered_map<std::uint32_t,Class> known_class_cache_;
    std::unordered_map<std::uint32_t,bool> unknown_class_cache_;
    std::vector<Address> chunks_;
    std::vector<Candidate> pending_;
    std::size_t pending_position_=0;
    std::uint32_t chunk_position_=0;
    Diagnostics diagnostics_;
    bool valid_config()const;
    bool begin_cycle(std::uint64_t);
    void finish_cycle(std::uint64_t,const std::string&);
    bool load_one_chunk();
    bool read_headers(std::uint32_t budget);
    void summarize();
};
} // namespace incremental_discovery

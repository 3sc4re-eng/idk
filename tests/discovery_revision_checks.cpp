// Source-only fixture bodies are compiled, never linked/executed. The pure
// revision/state assertions below ARE evaluated by the compiler.
#include "scan_incremental_checks.cpp"
#include <limits>
namespace incremental_discovery::proposal_checks {
static_assert(next_identity_revision(1)==2);
static_assert(next_identity_revision(std::numeric_limits<std::uint64_t>::max())==0);
static_assert(next_identity_revision(0)==0);
static_assert(identity_revision_changed(0,1));
static_assert(!identity_revision_changed(3,3));
static_assert(identity_revision_changed(3,4));
static_assert(identity_revision_changed(0,0)); // saturated: always resync
static_assert(identity_revision_changed(7,0));

// The I/O-work return is deliberately absent from the production predicate.
constexpr bool compile_time_transition_checks(){
    std::uint64_t current=1,copied=0;
    if(!identity_revision_changed(copied,current))return false;copied=current;
    // true idle, then an active/pending step that does not change identities
    if(identity_revision_changed(copied,current))return false;
    if(identity_revision_changed(copied,current))return false;
    // a false-work step which erases a trailing absent chunk
    current=next_identity_revision(current);
    if(!identity_revision_changed(copied,current))return false;copied=current;
    // reset/recache restarts the engine and clears the client's copied token
    copied=0;current=next_identity_revision(1);
    return identity_revision_changed(copied,current);
}
static_assert(compile_time_transition_checks());

void source_only_revision_scenarios(){
    Fixture fixture;
    Engine engine([&](Address a,void* b,std::uint32_t n){return fixture.read(a,b,n);},[&](std::vector<Request>& r){return fixture.scatter(r);},[](std::uint32_t rva,Class& out){if(rva!=0x1000)return false;out={1,"FixturePawn"};return true;});
    Config config;config.entities_global=0x100000;config.client_base=0x400000;config.client_size=0x10000;config.session_token=1;config.chunk_count=1;config.max_headers_per_step=2;config.cycle_interval_ms=1000;engine.reset(config);
    const auto empty_revision=engine.identity_revision();
    assert(engine.step(100));assert(engine.active());assert(engine.identities().size()==2);assert(identity_revision_changed(empty_revision,engine.identity_revision()));
    assert(engine.step(101));assert(engine.step(102));assert(engine.step(103));assert(!engine.active());assert(engine.identities().size()==7);
    const auto stable=engine.identity_revision();const auto calls=fixture.scatter_calls;
    assert(!engine.step(104));assert(engine.identity_revision()==stable);assert(fixture.scatter_calls==calls); // true idle
    assert(engine.step(1104));assert(engine.active());assert(engine.identity_revision()==stable); // active/pending, same validated members
    assert(engine.step(1105));assert(engine.step(1106));assert(engine.step(1107));assert(engine.identity_revision()==stable);
    // A completely absent trailing chunk removes old members but performs no
    // chunk/header scatter. A boolean-I/O guard would lose these removals.
    write64(fixture.regions[0x200000],16,0);
    assert(!engine.step(2200));assert(engine.identities().empty());assert(identity_revision_changed(stable,engine.identity_revision()));
    const auto removed=engine.identity_revision();config.session_token=2;engine.reset(config);
    assert(identity_revision_changed(removed,engine.identity_revision()));assert(engine.identities().empty());assert(!engine.active());
    // Restored chunk plus recycled full serial updates the identity revision.
    write64(fixture.regions[0x200000],16,0x300000);write32(fixture.regions[0x300000],0x10,0x8000);
    assert(engine.step(2300));assert(engine.identities().at(0).handle==0x8000);assert(identity_revision_changed(removed,engine.identity_revision()));
}
} // namespace incremental_discovery::proposal_checks

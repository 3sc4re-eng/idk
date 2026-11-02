// Compile-only behavioral scenarios; no executable entry point or device calls.
#include "aim_model.h"
#include <cassert>

void aim_visibility_boundary_scenarios_not_executed(){
    Settings previous=cfg;const bool managed=snap.backend_managed;float previous_view[16];
    std::copy(std::begin(snap.view),std::end(snap.view),previous_view);
    cfg=Settings{};cfg.register_all();
    for(int i=0;i<7;++i)cfg.find(aim_model::key(i).c_str())->i=i==0?2:-1;
    snap.backend_managed=false;std::fill(std::begin(snap.view),std::end(snap.view),0);
    snap.view[0]=snap.view[5]=.001f;snap.view[15]=1;
    Entity e;e.base=2;e.alive=true;e.pos={0,0,0};e.visible=false;
    assert(!aim_model::select(e,1000,1000,1000,true).valid());
    assert(aim_model::select(e,1000,1000,1000,false).valid());
    assert(aim_model::weighted_select(e,1000,1000,.5f,1000,-1,false).valid());
    e.invuln=true;assert(!aim_model::select(e,1000,1000,1000,false).valid());e.invuln=false;
    // Visibility bypass cannot supply absent or unsupported native coordinates.
    snap.backend_managed=true;e.modifiers_valid=true;
    assert(!aim_model::select(e,1000,1000,1000,false).valid());
    assert(!aim_model::weighted_select(e,1000,1000,.5f,1000,-1,false).valid());
    cfg=previous;snap.backend_managed=managed;std::copy(std::begin(previous_view),std::end(previous_view),snap.view);
}

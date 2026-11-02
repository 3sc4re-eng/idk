// Source-only owned-snapshot scenarios. Runtime assertions are authored, not run.
#include "decision_frame.h"
#include <cassert>
void owned_decision_frame_scenarios_not_executed(){
    decision_frame::Snapshot first;first.view[0]=first.view[5]=first.view[15]=1;
    first.local.vel={1,2,3};first.camera={4,5,6};first.binding_epoch=7;
    first.managed=first.frame_valid=true;first.entity_sample_ms=1000;first.matrix_sample_ms=1001;
    Entity target;target.base=99;first.targets.push_back(target);
    {
        decision_frame::Scope outer(first);const auto* captured=decision_frame::active();
        first.view[0]=90;first.local.vel.x=90;first.targets.clear();
        assert(captured->view[0]==1&&captured->local.vel.x==1&&captured->targets.size()==1);
        assert(captured->project({1,0,0},1000,1000).x==1000);
        assert(captured->fresh(1250)&&!captured->fresh(1251)&&!captured->fresh(999));
        decision_frame::Snapshot next;next.binding_epoch=8;
        {decision_frame::Scope inner(next);assert(decision_frame::binding_epoch()==8);}
        assert(decision_frame::active()==captured&&decision_frame::binding_epoch()==7);
    }
    assert(!decision_frame::active());
}

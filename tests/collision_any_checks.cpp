// Source-only fixture. Compile with collision.cpp; do not run on this PC.
#include "collision.h"
#include <cassert>
#include <limits>

int main(){
    collision::Scene empty;
    assert(!empty.trace_any({-1,0,0},{1,0,0},collision::Vision).known);
    collision::Scene local;
    local.build({
        {{0,-10,-10},{0,10,-10},{0,0,10},0,0,collision::Vision},
        {{2,-10,-10},{2,10,-10},{2,0,10},0,0,collision::Bullet},
        {{4,0,-2},{4,0,2},{},.5f,1,collision::Projectile}
    },"source_fixture");
    for(Vec3 target:std::initializer_list<Vec3>{{5,0,0},{5,8,8},{-4,0,0},{2,0,0}}){
        Vec3 from{-5,0,0};auto vision=local.trace(from,target,collision::Vision);auto bullet=local.trace(from,target,collision::Bullet);
        auto combined=local.trace_any(from,target,collision::Vision|collision::Bullet);
        assert(combined.known==bool(vision.known&&bullet.known));
        assert(combined.clear==bool(vision.clear&&bullet.clear));
        if(!combined.clear)assert(combined.fraction>=0&&combined.fraction<1);
    }
    auto near=local.trace({-5,0,0},{5,0,0},collision::Vision|collision::Bullet);
    assert(near.known&&!near.clear&&std::fabs(near.fraction-.5f)<.0001f);
    auto short_ray=local.trace_any({-5,0,0},{-5.001f,0,0},collision::Vision|collision::Bullet);
    assert(short_ray.known&&short_ray.clear);
    float nan=std::numeric_limits<float>::quiet_NaN();
    assert(!local.trace_any({nan,0,0},{1,0,0},collision::Vision).known);
    assert(!local.trace_any({-1,0,0},{1,0,0},0).known);

    collision::Obstacle a{{1,2,3},{-1,-1,-1},{1,1,1},{0,0,0},1};
    collision::Obstacle b{{4,5,6},{-2,-2,-2},{2,2,2},{0,45,0},1};
    collision::set_obstacles({});auto baseline=collision::revision();
    collision::set_obstacles({a,b});auto first=collision::revision();
    assert(first.scene==baseline.scene&&first.obstacles!=baseline.obstacles);
    collision::set_obstacles({b,a,a});assert(collision::revision()==first);
    b.origin.x+=.25f;collision::set_obstacles({a,b});auto changed=collision::revision();
    assert(changed.scene==first.scene&&changed.obstacles!=first.obstacles);
    collision::set_obstacles({});auto cleared=collision::revision();assert(cleared.obstacles!=changed.obstacles);
    collision::set_obstacles({});assert(collision::revision()==cleared);
}

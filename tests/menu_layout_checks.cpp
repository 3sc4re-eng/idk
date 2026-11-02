#include "menu_layout.h"
using namespace menu_layout;
constexpr auto preview_small=size(1160,780,50,true);
static_assert(preview_small.width==600&&preview_small.height==420);
constexpr auto preview_normal=size(1160,780,100,true);
static_assert(preview_normal.width==1160&&preview_normal.height==780);
constexpr auto supported_small=size(640,480,150,false);
static_assert(supported_small.width==600&&supported_small.height==432);
constexpr auto tiny=size(320,240,50,false);
static_assert(tiny.width<=320&&tiny.height<=240);
constexpr auto moved=origin(500,900,preview_small,1160,780);
static_assert(moved.x==500&&moved.y==360);
constexpr auto clamped=origin(-100,-100,supported_small,640,480);
static_assert(clamped.x==20&&clamped.y==24);
constexpr bool narrow_tabs_fit(){
    // Measured-text inputs exercise the actual row planner, including long labels.
    const auto plan=tabs({90,80,100,140,70},396);
    if(plan.cells.size()!=5||plan.height!=68)return false;
    for(std::size_t i=0;i<plan.cells.size();++i){const auto& cell=plan.cells[i];if(cell.x<0||cell.x+cell.width>396.001f)return false;if(i&&cell.y==plan.cells[i-1].y&&cell.x+0.001f<plan.cells[i-1].x+plan.cells[i-1].width)return false;}
    return true;
}
static_assert(narrow_tabs_fit());
constexpr bool normal_tabs_fit(){const auto plan=tabs({90,80,100,140,70},772);return plan.cells.size()==5&&plan.height==32&&plan.cells.back().x+plan.cells.back().width<=772.001f;}
static_assert(normal_tabs_fit());
constexpr bool oversized_label_clips_to_one_cell(){const auto plan=tabs({500,80},100);return plan.height==68&&plan.cells.front().width==100&&plan.cells.back().width==100;}
static_assert(oversized_label_clips_to_one_cell());
// Syntax-only / compile-time fixture. No executable entry point or GUI operation.

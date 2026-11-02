#include "projectile_presentation.h"
#include <limits>
using projectile_presentation::Style;
using projectile_presentation::visible;
using projectile_presentation::normalized;
using projectile_presentation::in_range;

constexpr bool all_presentation_combinations(){
    for(int mask=0;mask<8;++mask){Style style;style.marker=(mask&1)!=0;style.name=(mask&2)!=0;style.distance=(mask&4)!=0;if(visible(style)!=(mask!=0))return false;}
    return true;
}
static_assert(all_presentation_combinations());
static_assert(in_range(0,120)&&in_range(120,120)&&!in_range(120.01f,120)&&!in_range(-1,120));
static_assert(!in_range(std::numeric_limits<float>::infinity(),120));
static_assert(!in_range(std::numeric_limits<float>::quiet_NaN(),120));
constexpr Style invalid{true,false,true,std::numeric_limits<float>::quiet_NaN(),100,-1,{-1,2,std::numeric_limits<float>::quiet_NaN(),.25f}};
constexpr auto safe=normalized(invalid);
static_assert(safe.marker_size==6&&safe.font_size==32&&safe.max_dist==1);
static_assert(safe.color.r==0&&safe.color.g==1&&safe.color.b==1&&safe.color.a==.25f);
constexpr Style lower{true,false,false,-10,1,900,{1,1,1,1}};
static_assert(normalized(lower).marker_size==2&&normalized(lower).font_size==8&&normalized(lower).max_dist==400);
static_assert(!visible(Style{false,false,false}));

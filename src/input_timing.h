#pragma once
#include <cstdint>
#include <algorithm>
namespace input_timing{
struct Schedule{
    uint64_t last=0;
    uint64_t ready_at(uint64_t now,unsigned added,unsigned spacing)const{return std::max(now+added,last?last+spacing:uint64_t(0));}
    void sent(uint64_t time){last=time;}
};
}

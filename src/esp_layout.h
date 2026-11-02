#pragma once
#include "imgui.h"
namespace esp_layout{
inline ImVec2 label(int side,float x1,float y1,float x2,float y2,ImVec2 size,int lane){
    if(side==1)return {(x1+x2-size.x)/2,y2+7+lane*18};
    if(side==2)return {x1-size.x-14,y1+lane*18};
    if(side==3)return {x2+14,y1+lane*18};
    return {(x1+x2-size.x)/2,y1-size.y-7-lane*18};
}
inline ImVec2 stacked(int side,float x1,float y1,float x2,float y2,ImVec2 size,float& offset){
    ImVec2 p=side==1?ImVec2{(x1+x2-size.x)/2,y2+7+offset}:side==2?ImVec2{x1-size.x-14,y1+offset}:side==3?ImVec2{x2+14,y1+offset}:ImVec2{(x1+x2-size.x)/2,y1-size.y-7-offset};offset+=size.y+4;return p;
}
}

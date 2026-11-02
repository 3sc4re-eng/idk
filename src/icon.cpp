#include "icon.h"
#include "utils.h"
#include <cstdio>
#include <vector>
#include <ctime>
#include <cmath>
static void put16(std::vector<u8>&v,u32 x){ v.push_back((u8)(x&0xFF)); v.push_back((u8)((x>>8)&0xFF)); }
static void put32(std::vector<u8>&v,u32 x){ v.push_back((u8)(x&0xFF)); v.push_back((u8)((x>>8)&0xFF));
                                         v.push_back((u8)((x>>16)&0xFF)); v.push_back((u8)((x>>24)&0xFF)); }
HICON ensure_ace_icon(){
    const int S=32;
    if(GetFileAttributesA("ace.ico")==INVALID_FILE_ATTRIBUTES){
        std::vector<u8> px(S*S*4);
        auto setpx=[&](int x,int y,u8 r,u8 g,u8 b,u8 a){
            if(x<0||y<0||x>=S||y>=S) return;
            int i=((S-1-y)*S+x)*4;
            px[i]=b; px[i+1]=g; px[i+2]=r; px[i+3]=a;
        };
        float cx=16.f, cy=16.f, s=11.f;
        for(int y=0;y<S;y++) for(int x=0;x<S;x++){
            float dx=(float)x-cx, dy=(float)y-cy;
            bool corner=((x<3)||(x>S-4))&&((y<3)||(y>S-4));
            bool border=(x==0||y==0||x==S-1||y==S-1);
            if(corner){ setpx(x,y,0,0,0,0); continue; }
            if(border){ setpx(x,y,232,180,74,255); continue; }
            setpx(x,y,26,24,20,255);
            float d1=std::sqrt((dx+s*0.45f)*(dx+s*0.45f)+(dy-s*0.15f)*(dy-s*0.15f));
            float d2=std::sqrt((dx-s*0.45f)*(dx-s*0.45f)+(dy-s*0.15f)*(dy-s*0.15f));
            bool sp=(d1<s*0.42f)||(d2<s*0.42f);
            if(dy>=-0.95f*s&&dy<=0.2f*s){
                float w=0.72f*s*((dy+0.95f*s)/(1.15f*s));
                if(std::fabs(dx)<=w) sp=true;
            }
            if(dy>0.3f*s&&dy<0.95f*s){
                float wi=0.16f*s*((0.95f*s-dy)/(0.65f*s));
                if(std::fabs(dx)<=wi) sp=true;
                if(dy>=0.55f*s){
                    float wo=0.48f*s*((0.95f*s-dy)/(0.40f*s));
                    if(std::fabs(dx)<=wo) sp=true;
                }
            }
            if(sp) setpx(x,y,232,180,74,255);
        }
        std::vector<u8> ico;
        put16(ico,0); put16(ico,1); put16(ico,1);
        ico.push_back((u8)S); ico.push_back((u8)S);
        ico.push_back(0); ico.push_back(0);
        put16(ico,1); put16(ico,32);
        put32(ico,(u32)(40+px.size()+128));
        put32(ico,(u32)(6+16));
        put32(ico,40); put32(ico,S); put32(ico,S*2);
        put16(ico,1); put16(ico,32);
        for(int i=0;i<6;i++) put32(ico,0);
        ico.insert(ico.end(),px.begin(),px.end());
        for(int i=0;i<128;i++) ico.push_back(0);
        FILE* f=fopen("ace.ico","wb");
        if(f){ fwrite(ico.data(),1,ico.size(),f); fclose(f); }
    }
    return (HICON)LoadImageA(nullptr,"ace.ico",IMAGE_ICON,32,32,LR_LOADFROMFILE);
}

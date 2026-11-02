#pragma once
#include <windows.h>
#include <string>
namespace km_protocol{
inline int hid(int vk){
    if(vk>='A'&&vk<='Z')return vk-'A'+4;
    if(vk>='1'&&vk<='9')return vk-'1'+30;if(vk=='0')return 39;
    if(vk>=VK_F1&&vk<=VK_F12)return vk-VK_F1+58;
    if(vk>=VK_NUMPAD1&&vk<=VK_NUMPAD9)return vk-VK_NUMPAD1+89;
    switch(vk){
    case VK_RETURN:return 40;case VK_ESCAPE:return 41;case VK_BACK:return 42;case VK_TAB:return 43;case VK_SPACE:return 44;
    case VK_OEM_MINUS:return 45;case VK_OEM_PLUS:return 46;case VK_OEM_4:return 47;case VK_OEM_6:return 48;case VK_OEM_5:return 49;
    case VK_OEM_1:return 51;case VK_OEM_7:return 52;case VK_OEM_3:return 53;case VK_OEM_COMMA:return 54;case VK_OEM_PERIOD:return 55;case VK_OEM_2:return 56;
    case VK_CAPITAL:return 57;case VK_SNAPSHOT:return 70;case VK_SCROLL:return 71;case VK_PAUSE:return 72;case VK_INSERT:return 73;case VK_HOME:return 74;
    case VK_PRIOR:return 75;case VK_DELETE:return 76;case VK_END:return 77;case VK_NEXT:return 78;case VK_RIGHT:return 79;case VK_LEFT:return 80;case VK_DOWN:return 81;case VK_UP:return 82;
    case VK_NUMLOCK:return 83;case VK_DIVIDE:return 84;case VK_MULTIPLY:return 85;case VK_SUBTRACT:return 86;case VK_ADD:return 87;case VK_NUMPAD0:return 98;case VK_DECIMAL:return 99;
    case VK_CONTROL:case VK_LCONTROL:return 224;case VK_SHIFT:case VK_LSHIFT:return 225;case VK_MENU:case VK_LMENU:return 226;case VK_LWIN:return 227;case VK_RCONTROL:return 228;case VK_RSHIFT:return 229;case VK_RMENU:return 230;case VK_RWIN:return 231;default:return -1;}
}
inline std::string press(int vk,int milliseconds){int code=hid(vk);if(code<0)return {};return "km.press("+std::to_string(code)+","+std::to_string(milliseconds)+",0)\r\n";}
inline std::string chord(int modifier,int key,bool down){int first=hid(modifier),second=hid(key);if(first<0||second<0)return {};return std::string(down?"km.multidown(":"km.multiup(")+std::to_string(first)+","+std::to_string(second)+")\r\n";}
}

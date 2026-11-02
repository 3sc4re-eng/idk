#pragma once
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstring>
namespace map_identity{
inline std::string from_model(std::string path){
    for(auto& c:path){if(c=='\\')c='/';else if(c>='A'&&c<='Z')c=char(c-'A'+'a');}
    if(path.rfind("maps/",0)!=0)return {};
    auto end=path.find('/',5);if(end==std::string::npos)end=path.find('.',5);if(end==std::string::npos)return {};
    std::string map=path.substr(5,end-5);if(map.empty()||map.size()>64)return {};
    if(!std::all_of(map.begin(),map.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_';}))return {};
    return map;
}
// Never require bytes beyond a terminator on a following, possibly absent page.
template<class Read>bool cstring(Read read,std::uint64_t address,std::string& out,std::size_t limit=384){
    out.clear();if(address<0x10000||address>=0x00007fffffffffffULL||limit>4096)return false;
    while(out.size()<limit){char bytes[64]{};auto count=std::min<std::size_t>({sizeof(bytes),limit-out.size(),4096-(address&4095)});
        if(!read(address,bytes,(std::uint32_t)count)){out.clear();return false;}
        auto end=(const char*)std::memchr(bytes,0,count);std::size_t length=end?(std::size_t)(end-bytes):count;
        for(std::size_t i=0;i<length;i++)if((unsigned char)bytes[i]<32||(unsigned char)bytes[i]>126){out.clear();return false;}
        out.append(bytes,length);if(end)return true;address+=count;
    }out.clear();return false;
}
}

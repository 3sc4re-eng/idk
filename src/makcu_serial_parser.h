#pragma once
#include <string>
#include <deque>
#include <cctype>
#include <cstdint>
#include <utility>

// Independent implementation of the documented MAKCU text/prompt protocol.
namespace makcu_serial{
inline std::string trimmed(std::string value){while(!value.empty()&&std::isspace((unsigned char)value.front()))value.erase(value.begin());while(!value.empty()&&std::isspace((unsigned char)value.back()))value.pop_back();return value;}
inline bool ownership(const std::string& frame,const std::string& query,int& result){
    bool found=false;size_t from=0;
    while(from<=frame.size()){
        auto end=frame.find('\n',from);std::string line=trimmed(frame.substr(from,end==std::string::npos?std::string::npos:end-from));
        if(!line.empty()&&line!=query){if(found||line.size()!=1||line[0]<'0'||line[0]>'3')return false;result=line[0]-'0';found=true;}
        if(end==std::string::npos)break;from=end+1;
    }return found;
}
class Parser{
    std::string pending_;std::deque<std::string> frames_;
public:
    uint64_t stream_events=0,overflows=0;unsigned last_stream_mask=0;
    void reset(){pending_.clear();frames_.clear();stream_events=overflows=0;last_stream_mask=0;}
    void discard_completed(){frames_.clear();}
    std::string pending_preview()const{return pending_.substr(0,128);}
    void feed(const char* bytes,size_t size){
        pending_.append(bytes,size);
        // A prefixed mask can itself equal CR/LF. Consume its byte unconditionally;
        // bare control bytes are never guessed to be events on unknown firmware.
        for(size_t at=0;at+4<=pending_.size();){
            if(pending_.compare(at,3,"km.")==0&&(unsigned char)pending_[at+3]<=31){last_stream_mask=(unsigned char)pending_[at+3];stream_events++;pending_.erase(at,4);}
            else at++;
        }
        for(size_t end;(end=pending_.find(">>>"))!=std::string::npos;){
            frames_.push_back(pending_.substr(0,end));pending_.erase(0,end+3);
            if(frames_.size()>64){frames_.pop_front();overflows++;}
        }
        if(pending_.size()>8192){pending_.clear();overflows++;}
    }
    bool take(std::string& frame){if(frames_.empty())return false;frame=std::move(frames_.front());frames_.pop_front();return true;}
};
}

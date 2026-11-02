// Source-only checks for root to compile; no hardware/device code is linked here.
#include "makcu_serial_parser.h"
#include <cassert>
int main(){
    makcu_serial::Parser parser;std::string frame;int value=-1;
    parser.feed("km.",3);const char newline_mask='\n';parser.feed(&newline_mask,1);
    const std::string partial="\r\nkm.side1()\r\n3\r\n>";parser.feed(partial.data(),partial.size());parser.feed(">> ",3);
    assert(parser.stream_events==1&&parser.last_stream_mask==10);
    assert(parser.take(frame)&&makcu_serial::ownership(frame,"km.side1()",value)&&value==3);
    assert((value&1)!=0&&(value&2)!=0);
    assert(makcu_serial::ownership("2\r\n","km.side1()",value)&&value==2&&(value&1)==0);
    assert(makcu_serial::ownership("0\r\n","km.side2()",value)&&value==0);
    assert(!makcu_serial::ownership("km.left(1)\r\n","km.side1()",value));
    assert(!makcu_serial::ownership("1\r\n2\r\n","km.side1()",value));
    assert(!makcu_serial::ownership("ERR\r\n","km.side1()",value));
    assert(!makcu_serial::ownership(std::string(1,'\x10')+"1\r\n","km.side1()",value));
}

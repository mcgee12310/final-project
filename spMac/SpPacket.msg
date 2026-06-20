cplusplus {{
#include "MacPacket_m.h"
}}

class MacPacket;

packet SpPacket extends MacPacket {
    int spType = 1; // 1: DATA
    int originalSourceId;
    int senderId;
    int spSeqNumber;
    double creationTime;
    
    // Tọa độ phục vụ tính toán khoảng cách
    double sourceX;
    double sourceY;
    double senderX;
    double senderY;
    
    int hopCount;
    int ttl;
    string payload;
}
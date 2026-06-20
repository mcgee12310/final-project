// WppPacket.msg
cplusplus {{
#include "MacPacket_m.h"
}}

class MacPacket;

packet WppPacket extends MacPacket {
    int wppType = 1; // 1: DATA
    int originalSourceId;
    int senderId;
    int wppSeqNumber;
    double creationTime;
    
    // Tọa độ để tính khoảng cách (D)
    double sourceX;
    double sourceY;
    double senderX;
    double senderY;
    
    int hopCount;
    int ttl;
    string payload;
}
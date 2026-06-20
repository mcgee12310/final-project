//
// FloodingPacket.msg
//

cplusplus {{
    #include "MacPacket_m.h"
}}

class MacPacket;

enum FloodingMessageType {
    FLOODING_DATA = 1;
};

packet FloodingPacket extends MacPacket {

    int floodingType enum(FloodingMessageType);

    int originalSourceId;
    int senderId;

    int floodSeqNumber;

    double creationTime;

    double sourceX;
    double sourceY;

    double senderX;
    double senderY;

    int hopCount = 0;

    int ttl = 20;

    string payload;
}
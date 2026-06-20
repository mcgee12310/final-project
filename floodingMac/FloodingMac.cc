// FloodingMac.cc

#include "FloodingMac.h"
#include <sstream>
#include <cmath>
#include "VirtualMobilityManager.h"
#include "BpabTraCIManager.h"

Define_Module(FloodingMac);

#define WEBLOG(msgArgs) \
    do { \
        std::ostringstream _ss; \
        _ss << msgArgs; \
        trace() << _ss.str(); \
        /* --- GHI VÀO FILE LOG CHUNG --- */ \
        BpabTraCIManager::writeToUnifiedLog(simTime().dbl(), self, "MAC_EVENT", _ss.str()); \
        /* --- GỬI SANG BIẾN STATIC CỦA TRACIMANAGER (CŨ) --- */ \
        if (BpabTraCIManager::tcpClientSocket != -1) { \
            std::ostringstream _netSs; \
            _netSs << simTime().dbl() << " MAC " << _ss.str() << "\n"; \
            std::string _msg = _netSs.str(); \
            ::send(BpabTraCIManager::tcpClientSocket, _msg.c_str(), _msg.length(), 0); \
        } \
    } while(0)

// ======================================================
// STARTUP
// ======================================================

void FloodingMac::startup() {
    VirtualMac::startup();

    maxBroadcastRange = 400.0;

    jitterMax = par("jitterMax");
    maxHopCount = par("maxHopCount");
    seqCounter = 0;

    cModule *node = getParentModule()->getParentModule();
    cModule *mobility = node->getSubmodule("MobilityManager");

    mobilityModule = check_and_cast<VirtualMobilityManager*>(mobility);

    // Bắt đầu lấy tọa độ khởi điểm
    double myX = mobilityModule->getLocation().x;
    double myY = mobilityModule->getLocation().y;
}

// ======================================================
// DUPLICATE DETECTION
// ======================================================

bool FloodingMac::alreadySeen(int src, int seq) {
    std::pair<int,int> key(src, seq);

    if (seenPackets.count(key)) {
        return true;
    }

    seenPackets.insert(key);
    return false;
}

// ======================================================
// DISTANCE
// ======================================================

double FloodingMac::distance(double x1, double y1, double x2, double y2) {
    double dx = x1 - x2;
    double dy = y1 - y2;
    return sqrt(dx * dx + dy * dy);
}

// ======================================================
// FROM NETWORK LAYER
// ======================================================

void FloodingMac::fromNetworkLayer(cPacket *pkt, int destination) {
    double myX = mobilityModule->getLocation().x;
    double myY = mobilityModule->getLocation().y;

    FloodingPacket *floodPkt = new FloodingPacket("FloodingData", MAC_LAYER_PACKET);
    floodPkt->setFloodingType(FLOODING_DATA);
    floodPkt->setOriginalSourceId(self);
    floodPkt->setSenderId(self);
    floodPkt->setFloodSeqNumber(seqCounter++);
    floodPkt->setCreationTime(simTime().dbl());
    floodPkt->setSourceX(myX);
    floodPkt->setSourceY(myY);
    floodPkt->setSenderX(myX);
    floodPkt->setSenderY(myY);
    floodPkt->setHopCount(0);
    floodPkt->setTtl(maxHopCount);
    floodPkt->setPayload("FLOOD");

    // encapsulate tu tinh byteLength
    floodPkt->encapsulate(pkt);

    seenPackets.insert(std::make_pair(floodPkt->getOriginalSourceId(), floodPkt->getFloodSeqNumber()));

    WEBLOG("SEND src=" << self << " seq=" << floodPkt->getFloodSeqNumber() << " bytes=" << floodPkt->getByteLength());

    toRadioLayer(floodPkt);
    toRadioLayer(createRadioCommand(SET_STATE, TX));
}

// ======================================================
// FROM RADIO LAYER
// ======================================================

void FloodingMac::fromRadioLayer(cPacket *pkt, double rssi, double lqi) {
    FloodingPacket *floodPkt = dynamic_cast<FloodingPacket*>(pkt);

    if (!floodPkt) {
        WEBLOG("INVALID_PACKET_TYPE node=" << self);
        return; // Đã bỏ delete pkt;
    }
    int src = floodPkt->getOriginalSourceId();
    int seq = floodPkt->getFloodSeqNumber();

    // Range Check
    double myX = mobilityModule->getLocation().x;
    double myY = mobilityModule->getLocation().y;
    double d = distance(myX, myY, floodPkt->getSenderX(), floodPkt->getSenderY());

    if (d > maxBroadcastRange) {
//        WEBLOG("DROP_RANGE node=" << self << " src=" << src << " dist=" << d);
        return; // Đã bỏ delete floodPkt;
    }

    // Duplicate Check
    if (alreadySeen(src, seq)) {
        WEBLOG("DROP_DUPLICATE node=" << self << " src=" << src << " seq=" << seq);
        return; // Đã bỏ delete floodPkt;
    }

    // TTL Check
    if (floodPkt->getTtl() <= 0) {
        WEBLOG("DROP_TTL node=" << self << " src=" << src << " seq=" << seq);
        return; // Đã bỏ delete floodPkt;
    }

    WEBLOG("RECV node=" << self << " src=" << src << " seq=" << seq << " hop=" << floodPkt->getHopCount() << " dist=" << d);

    // Rebroadcast
    rebroadcastPacket(floodPkt);

    // Deliver lên network layer
    cPacket *netPkt = floodPkt->decapsulate();
    if (netPkt) {
        toNetworkLayer(netPkt); // Gửi thẳng netPkt, network layer sẽ tự lo quản lý bộ nhớ
    }
}

// ======================================================
// REBROADCAST (WITH JITTER QUEUE)
// ======================================================

void FloodingMac::rebroadcastPacket(FloodingPacket *pkt) {
    if (pkt->getTtl() <= 1) return;

    FloodingPacket *copy = pkt->dup();
    copy->setSenderId(self);
    copy->setSenderX(mobilityModule->getLocation().x);
    copy->setSenderY(mobilityModule->getLocation().y);
    copy->setHopCount(pkt->getHopCount() + 1);
    copy->setTtl(pkt->getTtl() - 1);

    // Đẩy gói tin vào hàng chờ
    rebroadcastQueue.push(copy);

    // [ĐÃ SỬA LỖI CRASH Ở ĐÂY]
    // Dùng size() của Queue thay vì getTimer() để tránh lỗi NULL Pointer của Castalia
    if (rebroadcastQueue.size() == 1) {
        double randomJitter = uniform(0, jitterMax);
        setTimer(TIMER_REBROADCAST, randomJitter);
    }
}

// ======================================================
// TIMER
// ======================================================

void FloodingMac::timerFiredCallback(int index) {
    if (index == TIMER_REBROADCAST) {
        if (!rebroadcastQueue.empty()) {
            // 1. Lấy gói tin ra khỏi hàng đợi
            FloodingPacket *pktToTransmit = rebroadcastQueue.front();
            rebroadcastQueue.pop();

            WEBLOG("REBROADCAST_EXEC node=" << self << " hop=" << pktToTransmit->getHopCount());

            // 2. Ra lệnh Radio phát sóng
            toRadioLayer(pktToTransmit);
            toRadioLayer(createRadioCommand(SET_STATE, TX));

            // 3. Nếu hàng đợi VẪN CÒN gói tin khác, tiếp tục hẹn giờ phát tiếp
            if (!rebroadcastQueue.empty()) {
                double randomJitter = uniform(0, jitterMax);
                setTimer(TIMER_REBROADCAST, randomJitter);
            }
        }
    }
}

// ======================================================
// RADIO CONTROL
// ======================================================

int FloodingMac::handleRadioControlMessage(cMessage *msg) {
    return 0;
}

// ======================================================
// DESTRUCTOR
// ======================================================

FloodingMac::~FloodingMac() {
    // Dọn dẹp sạch sẽ RAM tránh rò rỉ bộ nhớ (Memory Leak)
    while (!rebroadcastQueue.empty()) {
        FloodingPacket *pkt = rebroadcastQueue.front();
        rebroadcastQueue.pop();
        delete pkt; // Xóa an toàn object
    }
}
